"""The server list as a picture in the Glassed theme's look (tools/shell_skin.py):
dark glass in white hairlines over the busiest game's map, in Rajdhani.
discord_feeds.py attaches it to the server tracker's message.

It is wide and always the same size: Discord fits a picture into a box
wider than it is tall, so a wide one is shown largest, and a constant size
keeps the message from jumping as games come and go."""

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

WIDTH, HEIGHT = 1600, 1000
MARGIN = 36
HEADER_HEIGHT = 140
FOOTER_HEIGHT = 80
COLUMNS = 2
COLUMN_GAP = 24
# a column has this many rows at most at full size; more games make every row smaller
FULL_SIZE_ROWS = 5
THUMBNAIL = (144, 108)


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


class Card:
    def __init__(self, backdrop):
        size = (WIDTH, HEIGHT)
        if backdrop:
            image = cover(backdrop, size).filter(ImageFilter.GaussianBlur(18))
            image = Image.blend(image, Image.new("RGB", size, SHADE), 0.62)
        else:
            image = Image.new("RGB", size, (14, 20, 30))
        self.image = image.convert("RGBA")
        self.glass = Image.new("RGBA", size, (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.glass)
        # (maps' pictures go over the glass, so it doesn't darken them)
        self.pictures = []

    def picture(self, picture, position):
        self.pictures.append((picture, position))

    def png(self):
        image = Image.alpha_composite(self.image, self.glass)
        edges = Image.new("RGBA", image.size, (0, 0, 0, 0))
        for picture, (x, y) in self.pictures:
            image.paste(picture, (x, y))
            ImageDraw.Draw(edges).rectangle((x, y, x + picture.width - 1, y + picture.height - 1),
                                            outline=WHITE + (110,))
        out = io.BytesIO()
        Image.alpha_composite(image, edges).convert("RGB").save(out, "PNG", optimize=True)
        return out.getvalue()


def draw_game(card, game, box, scale, thumbnail, map_name, mode_name):
    """a game's row, scale times its full size: its map's picture, its
    server's name over its map and mode, and how many are playing (green
    while it can be joined)"""
    left, top, right, bottom = box
    draw = card.draw
    joinable = game["open"] and game["players"] < game["maximum_players"]
    draw.rectangle(box, fill=WHITE + (14,))
    if joinable:
        draw.rectangle((left, top, left + 5, bottom), fill=GREEN + (230,))

    def scaled(size):
        return round(size * scale)

    picture_size = (scaled(THUMBNAIL[0]), scaled(THUMBNAIL[1]))
    picture_left, picture_top = left + scaled(22), top + (bottom - top - picture_size[1]) // 2
    if thumbnail:
        card.picture(cover(thumbnail, picture_size), (picture_left, picture_top))

    # the name's line shares its width with the count; the map and mode have the whole row's
    count_font, name_font, detail_font = font("Bold", scaled(58)), font("Bold", scaled(54)), font("SemiBold", scaled(42))
    count = f"{game['players']}/{game['maximum_players']}"
    baseline = (top + bottom) // 2 - scaled(6)
    text_left = picture_left + picture_size[0] + scaled(22)
    text_right = right - scaled(24)
    draw.text((text_right, baseline), count, font=count_font, anchor="rs",
              fill=GREEN + (240,) if joinable else WHITE + (120,))
    name_width = text_right - draw.textlength(count, font=count_font) - scaled(24) - text_left
    draw.text((text_left, baseline), fit(draw, game["name"], name_font, name_width), font=name_font,
              fill=WHITE + (240,), anchor="ls")
    details = f"{map_name(game['map'])}  ·  {mode_name(game)}"
    draw.text((text_left, baseline + scaled(14)), fit(draw, details, detail_font, text_right - text_left),
              font=detail_font, fill=WHITE + (175,), anchor="lt")


def server_card(games, map_name, mode_name, map_art, user_agent):
    """games: the list's games; map_name, mode_name name a game's map and mode;
    map_art gives a map's picture's path under ART_URL"""
    active = sorted((game for game in games if game["players"] > 0), key=lambda game: -game["players"])
    empty = [game for game in games if game["players"] == 0]
    rows_per_column = max(FULL_SIZE_ROWS, -(-len(active) // COLUMNS))
    card = Card(fetch_art(map_art(active[0]["map"]), user_agent) if active else None)
    draw = card.draw
    players = sum(game["players"] for game in active)

    # the heading
    draw.text((MARGIN, HEADER_HEIGHT // 2), spaced("OpenCE") + "   " + spaced("Servers"), font=font("Bold", 76),
              fill=WHITE + (235,), anchor="lm")
    summary = (f"{players} {'PLAYER' if players == 1 else 'PLAYERS'}  ·  "
               f"{len(active)} {'SERVER' if len(active) == 1 else 'SERVERS'}")
    draw.text((WIDTH - MARGIN, HEADER_HEIGHT // 2), summary, font=font("SemiBold", 46), fill=WHITE + (180,),
              anchor="rm")

    # the games' pane of dark glass, a column of rows each side
    left, top = MARGIN, HEADER_HEIGHT
    right, bottom = WIDTH - MARGIN, HEIGHT - FOOTER_HEIGHT
    draw.rectangle((left, top, right, bottom), fill=SHADE + (125,), outline=WHITE + (70,))
    column_width = (right - left - 2 * 16 - (COLUMNS - 1) * COLUMN_GAP) // COLUMNS
    row_height = (bottom - top - 2 * 16) // rows_per_column
    scale = FULL_SIZE_ROWS / rows_per_column
    thumbnails = {}
    for index, game in enumerate(active):
        column, row = divmod(index, rows_per_column)
        x = left + 16 + column * (column_width + COLUMN_GAP)
        y = top + 16 + row * row_height
        path = map_art(game["map"])
        if path not in thumbnails:
            thumbnails[path] = fetch_art(path, user_agent)
        gap = max(2, round(4 * scale))
        draw_game(card, game, (x, y + gap, x + column_width, y + row_height - gap), scale, thumbnails[path],
                  map_name, mode_name)
    if not active:
        draw.text(((left + right) // 2, (top + bottom) // 2), spaced("No games right now"), font=font("SemiBold", 46),
                  fill=WHITE + (150,), anchor="mm")

    # the empty games, and how to join
    footer_font = font("SemiBold", 36)
    join = spaced("Join in game")
    join_width = draw.textlength(join, font=footer_font)
    middle = HEIGHT - FOOTER_HEIGHT // 2
    draw.text((WIDTH - MARGIN, middle), join, font=footer_font, fill=WHITE + (100,), anchor="rm")
    if empty:
        names = [game["name"] for game in empty]
        names[0] = f"{len(empty)} EMPTY   {names[0]}"
        width = WIDTH - 2 * MARGIN - join_width - 40
        # (the largest letters at which the names fit on two lines, else the smallest, cut short)
        for size in range(34, 21, -2):
            lines = wrap(draw, names, font("SemiBold", size), width)
            if len(lines) <= 2:
                break
        footer = font("SemiBold", size)
        lines = lines[:1] + [fit(draw, "  ·  ".join(lines[1:]), footer, width)] if len(lines) > 1 else lines
        draw.multiline_text((MARGIN, middle), "\n".join(lines), font=footer, fill=WHITE + (120,), anchor="lm",
                            spacing=4)
    return card.png()


def wrap(draw, names, typeface, width):
    """names joined with dots into lines no wider than width"""
    lines = []
    for name in names:
        line = f"{lines[-1]}  ·  {name}" if lines else name
        if lines and draw.textlength(line, font=typeface) <= width:
            lines[-1] = line
        else:
            lines.append(name)
    return lines
