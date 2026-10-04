"""The server list as a picture in the Glassed theme's look (tools/shell_skin.py):
dark glass in white hairlines over the busiest game's map, in Rajdhani.
discord_feeds.py attaches it to the server tracker's message."""

import io
import urllib.request
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONTS = Path(__file__).resolve().parent.parent / "port" / "assets" / "fonts"
ART_URL = "https://halo.milenko.org/art"

# the Glassed theme's (tools/shell_art.py, shell_skin.py)
WHITE = (255, 255, 255)
SHADE = (6, 8, 12)
GREEN = (150, 255, 180)

# drawn at twice the size Discord shows an embed's picture, so it stays sharp
WIDTH = 1040
MARGIN = 36
HEADER_HEIGHT = 112
COLUMNS_HEIGHT = 40
ROW_HEIGHT = 66
FOOTER_HEIGHT = 92
THUMBNAIL = (72, 54)
MAXIMUM_ROWS = 15
# where each column starts, from the panel's left
MAP_X, MODE_X = 420, 660


def font(weight, size):
    return ImageFont.truetype(str(FONTS / f"Rajdhani-{weight}.ttf"), size)


def fetch_art(path, user_agent):
    try:
        request = urllib.request.Request(f"{ART_URL}/{path}", headers={"User-Agent": user_agent})
        with urllib.request.urlopen(request, timeout=10) as response:
            return Image.open(io.BytesIO(response.read())).convert("RGB")
    except (OSError, ValueError):
        return None


def cover(picture, size):
    """the picture scaled and cut to fill size"""
    scale = max(size[0] / picture.width, size[1] / picture.height)
    picture = picture.resize((round(picture.width * scale), round(picture.height * scale)), Image.LANCZOS)
    left, top = (picture.width - size[0]) // 2, (picture.height - size[1]) // 2
    return picture.crop((left, top, left + size[0], top + size[1]))


def fit(draw, text, typeface, width):
    """text cut short to fit width"""
    if draw.textlength(text, font=typeface) <= width:
        return text
    while text and draw.textlength(text + "…", font=typeface) > width:
        text = text[:-1]
    return text + "…"


def spaced(text):
    """capitals set apart, as the Glassed theme's headings"""
    return " ".join(text.upper())


def server_card(games, map_name, mode_name, map_art, user_agent):
    """games: the list's games; map_name, mode_name name a game's map and mode;
    map_art gives a map's picture's path under ART_URL"""
    active = sorted((game for game in games if game["players"] > 0), key=lambda game: -game["players"])
    empty = [game for game in games if game["players"] == 0]
    shown = active[:MAXIMUM_ROWS]
    rows = max(len(shown), 1) + (1 if len(active) > MAXIMUM_ROWS else 0)
    height = HEADER_HEIGHT + COLUMNS_HEIGHT + rows * ROW_HEIGHT + FOOTER_HEIGHT
    size = (WIDTH, height)

    # the busiest game's map, blurred and dimmed, behind it all
    backdrop = fetch_art(map_art(active[0]["map"]), user_agent) if active else None
    if backdrop:
        card = cover(backdrop, size).filter(ImageFilter.GaussianBlur(14))
        card = Image.blend(card, Image.new("RGB", size, SHADE), 0.62)
    else:
        card = Image.new("RGB", size, (14, 20, 30))
    card = card.convert("RGBA")
    glass = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(glass)

    title, label, name_font, detail_font, small = (font("Bold", 46), font("SemiBold", 20), font("Bold", 30),
                                                    font("SemiBold", 26), font("SemiBold", 21))
    players = sum(game["players"] for game in active)

    # the heading
    draw.text((MARGIN, 34), spaced("OpenCE") + "   " + spaced("Servers"), font=title, fill=WHITE + (235,))
    summary = f"{players} {'PLAYER' if players == 1 else 'PLAYERS'}  ·  {len(active)} " \
              f"{'SERVER' if len(active) == 1 else 'SERVERS'}"
    draw.text((WIDTH - MARGIN, 52), summary, font=detail_font, fill=WHITE + (170,), anchor="ra")

    # the list's pane of dark glass, and its column headings
    left, right = MARGIN, WIDTH - MARGIN
    top = HEADER_HEIGHT
    bottom = top + COLUMNS_HEIGHT + rows * ROW_HEIGHT
    draw.rectangle((left, top, right, bottom), fill=SHADE + (120,), outline=WHITE + (70,))
    draw.rectangle((left, top, right, top + COLUMNS_HEIGHT), fill=WHITE + (26,))
    draw.line((left, top + COLUMNS_HEIGHT, right, top + COLUMNS_HEIGHT), fill=WHITE + (150,))
    for x, heading in ((left + 20 + THUMBNAIL[0] + 18, "Server"), (left + MAP_X, "Map"), (left + MODE_X, "Mode")):
        draw.text((x, top + 11), spaced(heading), font=label, fill=WHITE + (160,))
    draw.text((right - 20, top + 11), spaced("Players"), font=label, fill=WHITE + (160,), anchor="ra")

    # a row a game
    y = top + COLUMNS_HEIGHT
    thumbnails = {}
    for game in shown:
        full = game["players"] >= game["maximum_players"]
        joinable = game["open"] and not full
        if joinable:
            draw.rectangle((left + 1, y + 1, left + 5, y + ROW_HEIGHT - 1), fill=GREEN + (230,))
        path = map_art(game["map"])
        if path not in thumbnails:
            thumbnails[path] = fetch_art(path, user_agent)
        thumbnail_box = (left + 20, y + (ROW_HEIGHT - THUMBNAIL[1]) // 2)
        if thumbnails[path]:
            card.paste(cover(thumbnails[path], THUMBNAIL), thumbnail_box)
        draw.rectangle((thumbnail_box[0], thumbnail_box[1], thumbnail_box[0] + THUMBNAIL[0] - 1,
                        thumbnail_box[1] + THUMBNAIL[1] - 1), outline=WHITE + (110,))
        name_x = thumbnail_box[0] + THUMBNAIL[0] + 18
        middle = y + ROW_HEIGHT // 2
        draw.text((name_x, middle), fit(draw, game["name"], name_font, left + MAP_X - name_x - 16),
                  font=name_font, fill=WHITE + (240,), anchor="lm")
        draw.text((left + MAP_X, middle), fit(draw, map_name(game["map"]), detail_font, MODE_X - MAP_X - 16),
                  font=detail_font, fill=WHITE + (190,), anchor="lm")
        draw.text((left + MODE_X, middle), fit(draw, mode_name(game), detail_font, right - left - MODE_X - 120),
                  font=detail_font, fill=WHITE + (190,), anchor="lm")
        count = f"{game['players']}/{game['maximum_players']}"
        draw.text((right - 20, middle), count, font=name_font, fill=(GREEN if joinable else WHITE) +
                  ((240,) if joinable else (120,)), anchor="rm")
        y += ROW_HEIGHT
        draw.line((left + 20, y, right - 20, y), fill=WHITE + (36,))
    if not shown:
        draw.text((WIDTH // 2, y + ROW_HEIGHT // 2), spaced("No games right now"), font=detail_font,
                  fill=WHITE + (150,), anchor="mm")
    elif len(active) > MAXIMUM_ROWS:
        draw.text((WIDTH // 2, y + ROW_HEIGHT // 2), f"+{len(active) - MAXIMUM_ROWS} MORE", font=detail_font,
                  fill=WHITE + (150,), anchor="mm")

    # the empty games, and how to join
    footer_y = bottom + 20
    if empty:
        names = "EMPTY   " + "  ·  ".join(game["name"] for game in empty)
        draw.text((MARGIN, footer_y), fit(draw, names, small, WIDTH - 2 * MARGIN), font=small, fill=WHITE + (120,))
    draw.text((MARGIN, footer_y + 34), spaced("Join from the server browser in game"), font=small,
              fill=WHITE + (90,))

    card = Image.alpha_composite(card, glass).convert("RGB")
    out = io.BytesIO()
    card.save(out, "PNG", optimize=True)
    return out.getvalue()
