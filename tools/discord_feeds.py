"""DamnationCE's posts in its Discord server, through channel webhooks.

  discord_feeds.py servers
      The live server list: the game list server's games as one message,
      edited in place (.github/workflows/server-list.yml, every five
      minutes), the list drawn as a picture (discord_card.py).
        DISCORD_SERVERS_WEBHOOK  the server tracker channel's webhook URL
        DISCORD_SERVERS_MESSAGE  the message to edit; empty posts a new one
                                 and prints its ID, for the repository variable

  discord_feeds.py release <tag> <notes file>
      A release's heading drawn as a card (discord_card.py), which tells
      @everyone, over its changelog's text, posted silently, as it is
      published (release.yml).
      The text's message IDs are printed, for release-edit.
        DISCORD_RELEASES_WEBHOOK  the changelog channel's webhook URL

  discord_feeds.py release-edit <tag> <notes file> <message ID>
      A release's text message edited to match its notes, as they are in
      CHANGELOG.md now (.github/workflows/discord-release-edit.yml). The ID
      is the text's, under the card (in Discord: the message's ..., Copy
      Message ID, with Developer Mode on).
        DISCORD_RELEASES_WEBHOOK  the changelog channel's webhook URL

  discord_feeds.py upstream <build> <status> <commits file> [<details>]
      An OpenCE build the upstream merger took (upstream.yml): a heading card,
      what became of it (status: merged, unpublished, ours, failed or conflict, with
      details: a link or the files), and its commits' titles, one a line in
      the file. Posted silently.
        DISCORD_UPSTREAM_WEBHOOK  the upstream channel's webhook URL

  discord_feeds.py nightly <version> <commit> <commits file>
      A nightly build published (nightly.yml): a heading card, where to
      download it, and the titles of its commits since the last nightly,
      one a line in the file. Posted silently.
        DISCORD_NIGHTLY_WEBHOOK  the nightly builds channel's webhook URL

  discord_feeds.py rules
      The rules channel's picture, of discord_rules.md: posted, or with
      DISCORD_RULES_MESSAGE that message edited to match the file.
        DISCORD_RULES_WEBHOOK  the rules channel's webhook URL
        DISCORD_RULES_MESSAGE  the rules' message, once posted

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


def update_servers():
    # (here, not at the top: the card needs Pillow, which a release's post goes without)
    import discord_card

    webhook = os.environ["DISCORD_SERVERS_WEBHOOK"]
    message_id = os.environ.get("DISCORD_SERVERS_MESSAGE", "")
    # (a picture of its own rather than an embed's, which Discord shows smaller)
    updated = f"-# Updated <t:{int(time.time())}:R>"
    try:
        games = parse_games(request(LIST_URL))
        text = updated
        files = [("servers.png", discord_card.server_card(games, map_name, mode_name, map_art, USER_AGENT))]
    except (urllib.error.URLError, TimeoutError) as error:
        text = f"The server list can't be reached right now ({type(error).__name__}).\n{updated}"
        files = []
    # (the attachments listed replace the message's last ones)
    body = {"content": text, "embeds": [],
            "attachments": [{"id": index, "filename": name} for index, (name, _) in enumerate(files)],
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


def heading(label, title, aside_label):
    """a heading (discord_card.py) over one of the Xbox maps, the same for a
    title each time, dated today; None without Pillow"""
    try:
        import discord_card
    except ImportError:
        return None
    maps = sorted(XBOX_MAPS)
    backdrop = map_art(maps[sum(map(ord, title)) % len(maps)])
    date = time.strftime("%d %B %Y", time.gmtime()).lstrip("0")
    return discord_card.heading_card(label, title, aside_label, date, backdrop, USER_AGENT)


def release_heading(tag):
    """the release's heading"""
    return heading("DamnationCE", tag.lstrip("v"), "Release notes")


def post_card(webhook, name, card, everyone=False):
    """the card as a message of its own, posted silently, or telling @everyone"""
    message = {"content": "@everyone", "allowed_mentions": {"parse": ["everyone"]}} if everyone else \
        {"content": "", "flags": SUPPRESS_NOTIFICATIONS, "allowed_mentions": {"parse": []}}
    request(webhook, "POST", {**message, "attachments": [{"id": 0, "filename": name}]}, [(name, card)])


def post_release(tag, notes_path):
    webhook = os.environ["DISCORD_RELEASES_WEBHOOK"]
    with open(notes_path, encoding="utf-8") as notes:
        changelog = unwrap(notes.read())
    heading = release_heading(tag)
    if heading:
        post_card(webhook, "release.png", heading, everyone=True)
    else:
        changelog = f"@everyone **DamnationCE {tag}**\n" + changelog
    # (Discord's own text, readable at any length; a long changelog goes on in further messages; without
    # the card, its first part tells @everyone)
    for index, part in enumerate(message_parts(changelog)):
        ping = not heading and index == 0
        message = json.loads(request(f"{webhook}?wait=true", "POST",
                                     {"content": part, "flags": SUPPRESS_EMBEDS | (0 if ping else SUPPRESS_NOTIFICATIONS),
                                      "allowed_mentions": {"parse": ["everyone"] if ping else []}}))
        print(f"Posted {tag}'s text as message {message['id']}")


def edit_release(tag, notes_path, message_id):
    webhook = os.environ["DISCORD_RELEASES_WEBHOOK"]
    if not message_id.isdigit():
        sys.exit(f"{message_id!r} isn't a Discord message ID.")
    with open(notes_path, encoding="utf-8") as notes:
        parts = message_parts(unwrap(notes.read()))
    if len(parts) != 1:
        sys.exit(f"{tag}'s notes take {len(parts)} messages; only one can be edited in place.")
    request(f"{webhook}/messages/{message_id}", "PATCH", {"content": parts[0], "allowed_mentions": {"parse": []}})
    print(f"Edited {tag}'s text, message {message_id}")


# what became of an OpenCE build the upstream merger took
UPSTREAM_STATUSES = {
    "merged": "Merged into DamnationCE and built on every platform: players get it as the latest build.",
    "ours": "It's DamnationCE's own work coming back from OpenCE, which DamnationCE already has, so nothing changed.",
    "unpublished": "Merged into DamnationCE, but the build offering it to players failed: {details}",
    "failed": "It merged cleanly but didn't build, so it wasn't merged: {details}",
    "conflict": "It conflicts with DamnationCE's own changes, so it's waiting for a merge by hand. The files: {details}",
}
# the most of a build's commits listed
COMMITS_LISTED = 25


def commit_lines(commits_path):
    """a build's commits (one title a line in the file) as a post's list,
    the first COMMITS_LISTED of them; none for none"""
    with open(commits_path, encoding="utf-8") as commits_file:
        commits = [line.strip() for line in commits_file if line.strip()]
    if not commits:
        return []
    lines = ["", "What's in it:"] + [f"- {commit}" for commit in commits[:COMMITS_LISTED]]
    if len(commits) > COMMITS_LISTED:
        lines.append(f"- and {len(commits) - COMMITS_LISTED} more")
    return lines


def post_feed(webhook, card_name, card, lines):
    """a heading card (if Pillow drew one) and the lines under it, silently"""
    if card:
        post_card(webhook, card_name, card)
    for part in message_parts("\n".join(lines)):
        request(webhook, "POST", {"content": part, "flags": SUPPRESS_EMBEDS | SUPPRESS_NOTIFICATIONS,
                                  "allowed_mentions": {"parse": []}})


def post_upstream(build, status, commits_path, details=""):
    lines = [f"**[OpenCE {build}](https://github.com/OpenCommunityEdition/OpenCE/releases/tag/{build})**",
             UPSTREAM_STATUSES[status].format(details=details)] + commit_lines(commits_path)
    card = heading("OpenCE", build, "Upstream update")
    post_feed(os.environ["DISCORD_UPSTREAM_WEBHOOK"], "upstream.png", card, lines)


def post_nightly(version, commit, commits_path):
    lines = [f"**[DamnationCE {version}](https://github.com/xshxdex98/DamnationCE/releases/tag/nightly)**",
             f"Last night's build of main ({commit[:7]}), for Windows, Linux, macOS and Android: for testing, "
             "downloaded by hand (it doesn't update itself)."] + commit_lines(commits_path)
    card = heading("DamnationCE", version, "Nightly build")
    post_feed(os.environ["DISCORD_NIGHTLY_WEBHOOK"], "nightly.png", card, lines)


def update_rules():
    import discord_card

    webhook = os.environ["DISCORD_RULES_WEBHOOK"]
    message_id = os.environ.get("DISCORD_RULES_MESSAGE", "")
    with open(os.path.join(os.path.dirname(__file__), "discord_rules.md"), encoding="utf-8") as rules:
        # (the rules are short: one page)
        card = discord_card.document_pages("OpenCE", "RULES", "Read before posting", "", rules.read(),
                                           map_art("damnation"), USER_AGENT)[0]
    if not message_id:
        message = json.loads(request(f"{webhook}?wait=true", "POST", {
            "content": "", "allowed_mentions": {"parse": []}, "attachments": [{"id": 0, "filename": "rules.png"}]},
            [("rules.png", card)]))
        print(f"Posted the rules as message {message['id']}: give it as DISCORD_RULES_MESSAGE to edit them.")
        return
    request(f"{webhook}/messages/{message_id}", "PATCH", {"attachments": [{"id": 0, "filename": "rules.png"}]},
            [("rules.png", card)])


def main():
    if sys.argv[1:2] == ["servers"]:
        update_servers()
    elif sys.argv[1:2] == ["release"] and len(sys.argv) == 4:
        post_release(sys.argv[2], sys.argv[3])
    elif sys.argv[1:2] == ["release-edit"] and len(sys.argv) == 5:
        edit_release(sys.argv[2], sys.argv[3], sys.argv[4])
    elif sys.argv[1:2] == ["upstream"] and len(sys.argv) in (5, 6) and sys.argv[3] in UPSTREAM_STATUSES:
        post_upstream(*sys.argv[2:])
    elif sys.argv[1:2] == ["nightly"] and len(sys.argv) == 5:
        post_nightly(*sys.argv[2:])
    elif sys.argv[1:2] == ["rules"]:
        update_rules()
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
