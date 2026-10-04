"""DamnationCE's posts in its Discord server, through channel webhooks.

  discord_feeds.py servers
      The live server list: the game list server's games as one message,
      edited in place (.github/workflows/server-list.yml, every five
      minutes).
        DISCORD_SERVERS_WEBHOOK  the server tracker channel's webhook URL
        DISCORD_SERVERS_MESSAGE  the message to edit; empty posts a new one
                                 and prints its ID, for the repository variable

  discord_feeds.py release <tag> <notes file> <zip>...
      A release and its changelog, as it is published (release.yml).
        DISCORD_RELEASES_WEBHOOK  the changelog channel's webhook URL
        GITHUB_REPOSITORY         owner/name, for the release's links

The server list is what the game's own server browser reads, one game a line,
tab-separated: invite, name, map, engine, players, maximum players, open,
network version, gametype, score limit, teams, roster ("team:name|...").
Older list servers send fewer fields (port/linux/src/browser.c, parse_game).
"""

import json
import os
import sys
import time
import urllib.error
import urllib.request

LIST_URL = "https://halo.milenko.org/v1/games.txt"
SITE_URL = "https://halo.milenko.org"
# (Discord turns away requests without a user agent of its form)
USER_AGENT = "DiscordBot (https://github.com/xshxdex98/DamnationCE, 1)"

ENGINES = {1: "CTF", 2: "Slayer", 3: "Oddball", 4: "KOTH", 5: "Race"}
XBOX_MAPS = {
    "beavercreek": "Battle Creek", "bloodgulch": "Blood Gulch", "boardingaction": "Boarding Action",
    "carousel": "Derelict", "chillout": "Chill Out", "damnation": "Damnation", "hangemhigh": "Hang 'Em High",
    "longest": "Longest", "prisoner": "Prisoner", "putput": "Chiron TL-34", "ratrace": "Rat Race",
    "sidewinder": "Sidewinder", "wizard": "Wizard",
}
CE_MAPS = dict(XBOX_MAPS, **{
    "dangercanyon": "Danger Canyon", "deathisland": "Death Island", "gephyrophobia": "Gephyrophobia",
    "icefields": "Ice Fields", "infinity": "Infinity", "timberland": "Timberland",
})
CAMPAIGN_MAPS = {
    "a10": "Pillar of Autumn", "a30": "Halo", "a50": "Truth and Reconciliation", "b30": "The Silent Cartographer",
    "b40": "Assault on the Control Room", "c10": "343 Guilty Spark", "c20": "The Library",
    "c40": "Two Betrayals", "d20": "Keyes", "d40": "The Maw",
}

# Discord's limits: 4096 characters an embed's description, 1024 a field's value
DESCRIPTION_LENGTH = 4096
FIELD_LENGTH = 1024

COLOUR_LIVE = 0x3BA55D
COLOUR_QUIET = 0x5865F2
COLOUR_DOWN = 0xED4245
COLOUR_RELEASE = 0x5865F2
# (an embed's description holds 4096 characters)
CHANGELOG_PART_LENGTH = 4000


def request(url, method="GET", body=None):
    data = json.dumps(body).encode() if body is not None else None
    headers = {"User-Agent": USER_AGENT}
    if data:
        headers["Content-Type"] = "application/json"
    with urllib.request.urlopen(urllib.request.Request(url, data, headers, method=method), timeout=20) as response:
        return response.read().decode("utf-8")


def parse_games(text):
    games = []
    for line in text.splitlines():
        fields = line.split("\t")
        if len(fields) < 8 or len(fields[0]) != 64:
            continue
        game = {"name": fields[1], "map": fields[2], "engine": int(fields[3]), "players": int(fields[4]),
                "maximum_players": int(fields[5]), "open": fields[6] != "0", "score_limit": 0, "teams": False}
        if len(fields) >= 11:
            game["score_limit"] = int(fields[9] or 0)
            game["teams"] = fields[10] != "0"
        games.append(game)
    return games


def map_file(path):
    """a map's file and where it's from: "deathisland@ce" is Halo PC's"""
    file, _, source = path.replace("/", "\\").rsplit("\\", 1)[-1].partition("@")
    return file, source


def map_name(path):
    file, source = map_file(path)
    if source == "ce":
        return CE_MAPS.get(file, file) + " (PC)"
    if source == "md":
        return file + " (MD)"
    return CAMPAIGN_MAPS.get(file) or XBOX_MAPS.get(file) or file


def mode_name(game):
    if game["engine"] == 0 and map_file(game["map"])[0] in CAMPAIGN_MAPS:
        return "Co-op"
    name = ENGINES.get(game["engine"], "Game")
    # (Capture the Flag is played in teams only)
    return "Team " + name if game["teams"] and game["engine"] != 1 else name


def escape(text):
    for mark in "\\*_~`|>":
        text = text.replace(mark, "\\" + mark)
    return text


def clip(text, length):
    return text if len(text) <= length else text[:length - 1] + "…"


def game_line(game, count_width):
    """a game on one line: how many are playing, then its server's name, map
    and mode"""
    count = f"{game['players']}/{game['maximum_players']}".rjust(count_width)
    details = [map_name(game["map"]), mode_name(game)] + ([] if game["open"] else ["closed"])
    return f"`{count}` **{escape(game['name'])}** · " + " · ".join(details)


def servers_embed(games, error=None):
    """the list as an embed: a line for each game with players in it,
    busiest first, then the empty ones' names in small print; games is None
    while the list server can't be reached"""
    updated = f"-# Updated <t:{int(time.time())}:R>"
    embed = {"title": "DamnationCE servers", "url": SITE_URL, "footer": {"text": "Join from the server browser in game"}}
    if games is None:
        embed["color"] = COLOUR_DOWN
        embed["description"] = f"The server list can't be reached right now ({error}).\n{updated}"
        return embed

    active = sorted((game for game in games if game["players"] > 0), key=lambda game: -game["players"])
    empty = [game for game in games if game["players"] == 0]
    players = sum(game["players"] for game in active)
    count_width = max((len(f"{game['players']}/{game['maximum_players']}") for game in active), default=0)
    lines = [f"**{players}** {'player' if players == 1 else 'players'} on "
             f"**{len(active)}** {'server' if len(active) == 1 else 'servers'}", updated, ""]
    lines += [game_line(game, count_width) for game in active]
    if empty:
        lines += ["", "-# Empty: " + ", ".join(escape(game["name"]) for game in empty)]
    embed["color"] = COLOUR_LIVE if active else COLOUR_QUIET
    embed["description"] = clip("\n".join(lines), DESCRIPTION_LENGTH)
    return embed


def update_servers():
    webhook = os.environ["DISCORD_SERVERS_WEBHOOK"]
    message_id = os.environ.get("DISCORD_SERVERS_MESSAGE", "")
    try:
        embed = servers_embed(parse_games(request(LIST_URL)))
    except (urllib.error.URLError, TimeoutError) as error:
        embed = servers_embed(None, error=type(error).__name__)
    body = {"embeds": [embed], "allowed_mentions": {"parse": []}}

    if message_id:
        try:
            request(f"{webhook}/messages/{message_id}", "PATCH", body)
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            sys.exit(f"The server list's message {message_id} is gone: clear DISCORD_SERVERS_MESSAGE and run "
                     "the workflow by hand to post a new one.")
        return
    message = json.loads(request(f"{webhook}?wait=true", "POST", body))
    print(f"Posted the server list as message {message['id']}: set the repository variable "
          f"DISCORD_SERVERS_MESSAGE to it.")


def changelog_parts(notes, length=CHANGELOG_PART_LENGTH):
    """the changelog in pieces an embed holds, split between lines"""
    parts, part = [], ""
    for line in notes.strip().splitlines(keepends=True):
        if part and len(part) + len(line) > length:
            parts.append(part)
            part = ""
        part += line
    return parts + [part] if part else parts


def post_release(tag, notes_path, zips):
    webhook = os.environ["DISCORD_RELEASES_WEBHOOK"]
    page = f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/releases/tag/{tag}"
    with open(notes_path, encoding="utf-8") as notes:
        parts = changelog_parts(notes.read())
    downloads = "\n".join(f"[{os.path.basename(path)}]({page.replace('/tag/', '/download/')}/{os.path.basename(path)})"
                          for path in zips)

    # (a long changelog goes on in further messages, the downloads after its end)
    for index, part in enumerate(parts):
        embed = {"description": part, "color": COLOUR_RELEASE}
        if index == 0:
            embed.update(title=f"DamnationCE {tag}", url=page)
        if index == len(parts) - 1 and downloads:
            embed["fields"] = [{"name": "Downloads", "value": clip(downloads, FIELD_LENGTH), "inline": False}]
        request(webhook, "POST", {"embeds": [embed], "allowed_mentions": {"parse": []}})


def main():
    if sys.argv[1:2] == ["servers"]:
        update_servers()
    elif sys.argv[1:2] == ["release"] and len(sys.argv) >= 4:
        post_release(sys.argv[2], sys.argv[3], sys.argv[4:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
