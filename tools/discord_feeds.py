"""DamnationCE's posts in its Discord server, through channel webhooks.

  discord_feeds.py servers
      The live server list: the game list server's games as one message,
      edited in place (.github/workflows/server-list.yml, every five
      minutes), the list drawn as a picture (discord_card.py).
        DISCORD_SERVERS_WEBHOOK  the server tracker channel's webhook URL
        DISCORD_SERVERS_MESSAGE  the message to edit; empty posts a new one
                                 and prints its ID, for the repository variable

  discord_feeds.py release <tag> <notes file>
      A release's changelog as plain text, posted silently as it is
      published (release.yml).
        DISCORD_RELEASES_WEBHOOK  the changelog channel's webhook URL

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
import uuid

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

# (Discord's limit on a message's text)
MESSAGE_LENGTH = 2000

COLOUR_LIVE = 0x3BA55D
COLOUR_QUIET = 0x5865F2
COLOUR_DOWN = 0xED4245

# a message's flags: no link previews, and no notification (as @silent)
SUPPRESS_EMBEDS = 1 << 2
SUPPRESS_NOTIFICATIONS = 1 << 12


def request(url, method="GET", body=None, files=()):
    """body as JSON, or with files ((name, PNG bytes) pairs) as Discord's
    multipart form"""
    headers = {"User-Agent": USER_AGENT}
    data = json.dumps(body).encode() if body is not None else None
    if files:
        boundary = uuid.uuid4().hex
        parts = [('name="payload_json"', "application/json", data)]
        parts += [(f'name="files[{index}]"; filename="{name}"', "image/png", content)
                  for index, (name, content) in enumerate(files)]
        data = b"".join(f"--{boundary}\r\nContent-Disposition: form-data; {disposition}\r\n"
                        f"Content-Type: {kind}\r\n\r\n".encode() + content + b"\r\n"
                        for disposition, kind, content in parts) + f"--{boundary}--\r\n".encode()
        headers["Content-Type"] = f"multipart/form-data; boundary={boundary}"
    elif data:
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
                "maximum_players": int(fields[5]), "open": fields[6] != "0", "teams": False}
        if len(fields) >= 11:
            game["teams"] = fields[10] != "0"
        games.append(game)
    return games


def map_file(path):
    """a map's file and where it's from: "deathisland@ce" is Halo PC's"""
    file, _, source = path.replace("/", "\\").rsplit("\\", 1)[-1].partition("@")
    return file, source


def tidy_map_file(file):
    """a map file's name made readable: words apart, each begun with a capital"""
    return " ".join(word[:1].upper() + word[1:] for word in file.replace("-", "_").replace(".", "_").split("_") if word)


def map_name(path):
    file, source = map_file(path)
    if source == "ce":
        return (CE_MAPS.get(file) or tidy_map_file(file)) + " (PC)"
    if source == "md":
        return tidy_map_file(file) + " (MD)"
    return CAMPAIGN_MAPS.get(file) or XBOX_MAPS.get(file) or tidy_map_file(file)


def map_art(path):
    """a map's picture on the list server's site, as the site picks it"""
    file, source = map_file(path)
    if source == "ce":
        return f"maps/ce/{file if file in CE_MAPS else 'unknown'}.jpg"
    if source == "md":
        return "maps/ce/unknown.jpg"
    return f"maps/{file if file in XBOX_MAPS else 'unknown'}.jpg"


def mode_name(game):
    if game["engine"] == 0 and map_file(game["map"])[0] in CAMPAIGN_MAPS:
        return "Co-op"
    name = ENGINES.get(game["engine"], "Game")
    # (Capture the Flag is played in teams only)
    return "Team " + name if game["teams"] and game["engine"] != 1 else name


def servers_embed(games, error=None):
    """the list's embed: how many are playing and when it was drawn, over
    the card of its games; games is None while the list server can't be
    reached"""
    updated = f"-# Updated <t:{int(time.time())}:R>"
    embed = {"title": "OpenCE Servers", "url": SITE_URL}
    if games is None:
        embed["color"] = COLOUR_DOWN
        embed["description"] = f"The server list can't be reached right now ({error}).\n{updated}"
        return embed
    players = sum(game["players"] for game in games)
    servers = sum(1 for game in games if game["players"] > 0)
    embed["color"] = COLOUR_LIVE if players else COLOUR_QUIET
    embed["description"] = (f"**{players}** {'player' if players == 1 else 'players'} on "
                            f"**{servers}** {'server' if servers == 1 else 'servers'}\n{updated}")
    embed["image"] = {"url": "attachment://servers.png"}
    return embed


def update_servers():
    # (here, not at the top: the card needs Pillow, which a release's post goes without)
    import discord_card

    webhook = os.environ["DISCORD_SERVERS_WEBHOOK"]
    message_id = os.environ.get("DISCORD_SERVERS_MESSAGE", "")
    try:
        games = parse_games(request(LIST_URL))
        embed = servers_embed(games)
        files = [("servers.png", discord_card.server_card(games, map_name, mode_name, map_art, USER_AGENT))]
    except (urllib.error.URLError, TimeoutError) as error:
        embed = servers_embed(None, error=type(error).__name__)
        files = []
    # (the attachments listed replace the message's last ones)
    body = {"embeds": [embed], "attachments": [{"id": index, "filename": name} for index, (name, _) in enumerate(files)],
            "allowed_mentions": {"parse": []}}

    if message_id:
        try:
            request(f"{webhook}/messages/{message_id}", "PATCH", body, files)
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            sys.exit(f"The server list's message {message_id} is gone: clear DISCORD_SERVERS_MESSAGE and run "
                     "the workflow by hand to post a new one.")
        return
    message = json.loads(request(f"{webhook}?wait=true", "POST", body, files))
    print(f"Posted the server list as message {message['id']}: set the repository variable "
          f"DISCORD_SERVERS_MESSAGE to it.")


def message_parts(text, length=MESSAGE_LENGTH):
    """text in pieces a message holds, split between lines"""
    parts, part = [], ""
    for line in text.strip().splitlines(keepends=True):
        if part and len(part) + len(line) > length:
            parts.append(part)
            part = ""
        part += line
    return parts + [part] if part else parts


def unwrap(markdown):
    """CHANGELOG.md's paragraphs and list items each on one line, as Discord
    keeps the file's line breaks rather than flowing the text"""
    lines = []
    for line in markdown.splitlines():
        text = line.strip()
        starts_block = not text or text.startswith(("- ", "* ", "#")) or text.split(" ", 1)[0].rstrip(".").isdigit()
        if lines and lines[-1] and not starts_block:
            lines[-1] += " " + text
        else:
            lines.append(line.rstrip())
    return "\n".join(lines)


def post_release(tag, notes_path):
    webhook = os.environ["DISCORD_RELEASES_WEBHOOK"]
    with open(notes_path, encoding="utf-8") as notes:
        text = f"**DamnationCE {tag}**\n" + unwrap(notes.read())
    # (a long changelog goes on in further messages)
    for part in message_parts(text):
        request(webhook, "POST", {"content": part, "flags": SUPPRESS_EMBEDS | SUPPRESS_NOTIFICATIONS,
                                  "allowed_mentions": {"parse": []}})


def main():
    if sys.argv[1:2] == ["servers"]:
        update_servers()
    elif sys.argv[1:2] == ["release"] and len(sys.argv) == 4:
        post_release(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
