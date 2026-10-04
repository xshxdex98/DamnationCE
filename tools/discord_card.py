"""Discord's pictures in the Glassed theme's look (tools/shell_skin.py): dark
glass in white hairlines over a map, in Rajdhani. discord_feeds.py posts them.

- server_card: the server list. It is wide and always the same size:
  Discord fits a picture into a box wider than it is tall, so a wide one is
  shown largest, and a constant size keeps the message from jumping as games
  come and go.
- document_card: a release's changelog, or the rules.
"""

import functools
import io
import re
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


@functools.lru_cache(maxsize=None)
def font(weight, size):
    """the Glassed theme's text: Rajdhani, SemiBold or Bold"""
    return ImageFont.truetype(str(FONTS / f"Rajdhani-{weight}.ttf"), size)


@functools.lru_cache(maxsize=None)
def title_font(size):
    """the menus' titles': OpenCE (tools/title_font.py)"""
    return ImageFont.truetype(str(FONTS / "OpenCE-Regular.ttf"), size)


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
    def __init__(self, backdrop, size=(WIDTH, HEIGHT), blur=18, dim=0.62):
        """backdrop blurred by blur and darkened by dim (0 to 1) behind it all"""
        if backdrop:
            image = cover(backdrop, size).filter(ImageFilter.GaussianBlur(blur))
            image = Image.blend(image, Image.new("RGB", size, SHADE), dim)
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
    draw.text((MARGIN, HEADER_HEIGHT // 2), "OPENCE SERVERS", font=title_font(80),
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


# ---------- documents: a release's changelog, the rules

# A document is a card of its own: a heading on the Glassed theme's strip of
# dark glass between two hairlines (tools/shell_skin.py), over its text on a
# pane of glass. A long one flows into two columns, so the card stays wide
# enough for Discord to show it large.
DOCUMENT_WIDTH = 1600
STRIP_TOP, STRIP_BOTTOM = 30, 250
PANE_PADDING = 48
TEXT_COLUMN_GAP = 64
# (text taller than this goes into two columns)
ONE_COLUMN_HEIGHT = 700
TEXT_SIZE = 34
LINE_HEIGHT = 46
BLOCK_GAP = 12
HEADING_HEIGHT, HEADING_GAP = 58, 26
INDENTS = {"paragraph": 0, "item": 44, "numbered": 56}


def text_font(bold):
    return font("Bold" if bold else "SemiBold", TEXT_SIZE)


def wrap_text(draw, text, width):
    """the text's lines no wider than width, each a list of (piece, bold)
    pairs; **bold** words are bold, and backticks are dropped"""
    lines, line, line_width = [], [], 0.0
    for index, part in enumerate(text.replace("`", "").split("**")):
        bold = index % 2 == 1
        for token in re.findall(r"\s+|\S+", part):
            if token.isspace():
                if line:
                    line.append((" ", bold))
                    line_width += draw.textlength(" ", font=text_font(bold))
                continue
            token_width = draw.textlength(token, font=text_font(bold))
            if line and line_width + token_width > width:
                if line[-1][0] == " ":
                    line.pop()
                lines.append(line)
                line, line_width = [], 0.0
            line.append((token, bold))
            line_width += token_width
    return lines + [line] if line else lines


def parse_blocks(markdown):
    """the markdown's headings, list items and paragraphs, one a line (as
    discord_feeds.unwrap leaves them), as (kind, text, number) triples"""
    blocks = []
    for line in markdown.splitlines():
        text = line.strip()
        first_word = text.split(" ", 1)[0]
        if not text:
            continue
        if text.startswith("#"):
            blocks.append(("heading", text.lstrip("#").strip(), None))
        elif text.startswith(("- ", "* ")):
            blocks.append(("item", text[2:], None))
        elif " " in text and first_word.rstrip(".").isdigit():
            blocks.append(("numbered", text.split(" ", 1)[1], first_word.rstrip(".")))
        else:
            blocks.append(("paragraph", text, None))
    return blocks


def lay_out(draw, block, width, starts_column):
    """a block as (height, kind, lines or heading, number, starts_column)"""
    kind, text, number = block
    if kind == "heading":
        return (HEADING_HEIGHT + (0 if starts_column else HEADING_GAP), kind, spaced(text), None, starts_column)
    lines = wrap_text(draw, text, width - INDENTS[kind])
    return (len(lines) * LINE_HEIGHT + BLOCK_GAP, kind, lines, number, starts_column)


def draw_block(draw, block, x, y, width):
    _, kind, content, number, starts_column = block
    if kind == "heading":
        y += 0 if starts_column else HEADING_GAP
        draw.text((x, y + 6), content, font=font("SemiBold", 28), fill=WHITE + (170,))
        draw.line((x, y + 46, x + width, y + 46), fill=WHITE + (70,), width=2)
        return
    if kind == "item":
        draw.text((x + 8, y), "•", font=text_font(True), fill=GREEN + (220,))
    elif kind == "numbered":
        draw.text((x, y), number, font=text_font(True), fill=GREEN + (230,))
    for line in content:
        line_x = x + INDENTS[kind]
        for piece, bold in line:
            draw.text((line_x, y), piece, font=text_font(bold), fill=WHITE + ((245,) if bold else (215,)))
            line_x += draw.textlength(piece, font=text_font(bold))
        y += LINE_HEIGHT


def split_columns(blocks):
    """the blocks' indexes in two columns of about equal height, a heading
    never left at the foot of the first"""
    total, running, split = sum(block[0] for block in blocks), 0, len(blocks)
    for index, block in enumerate(blocks):
        if running + block[0] / 2 > total / 2:
            split = index
            break
        running += block[0]
    while 0 < split < len(blocks) and blocks[split - 1][1] == "heading":
        split -= 1
    return [range(split), range(split, len(blocks))]


def document_card(label, title, aside_label, aside, markdown, backdrop_path, user_agent):
    """a document's card: label (small, spaced) over title (large), and
    aside_label over aside, on its strip; then its markdown's text"""
    measure = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    pane_width = DOCUMENT_WIDTH - 2 * MARGIN
    parsed = parse_blocks(markdown)

    column_width = pane_width - 2 * PANE_PADDING
    blocks = [lay_out(measure, block, column_width, index == 0) for index, block in enumerate(parsed)]
    columns = [range(len(blocks))]
    if sum(block[0] for block in blocks) > ONE_COLUMN_HEIGHT:
        column_width = (pane_width - 2 * PANE_PADDING - TEXT_COLUMN_GAP) // 2
        blocks = [lay_out(measure, block, column_width, index == 0) for index, block in enumerate(parsed)]
        columns = split_columns(blocks)
        # (the second column's first block starts it, with no gap above)
        if columns[1]:
            first = columns[1][0]
            blocks[first] = lay_out(measure, parsed[first], column_width, True)
    body_height = max(sum(blocks[index][0] for index in column) for column in columns)

    pane_top = STRIP_BOTTOM + 40
    height = pane_top + body_height + 2 * PANE_PADDING + MARGIN
    card = Card(fetch_art(backdrop_path, user_agent), (DOCUMENT_WIDTH, height), blur=9, dim=0.45)
    draw = card.draw

    # the heading on its strip
    draw.rectangle((0, STRIP_TOP, DOCUMENT_WIDTH, STRIP_BOTTOM), fill=SHADE + (140,))
    for y in (STRIP_TOP, STRIP_BOTTOM):
        draw.line((0, y, DOCUMENT_WIDTH, y), fill=WHITE + (90,), width=2)
    middle = (STRIP_TOP + STRIP_BOTTOM) // 2
    left, right = MARGIN * 2, DOCUMENT_WIDTH - MARGIN * 2
    draw.text((left, middle - 52), spaced(label), font=font("SemiBold", 38), fill=WHITE + (190,), anchor="ls")
    draw.text((left, middle + 74), title, font=title_font(120), fill=WHITE + (240,), anchor="ls")
    draw.text((right, middle - 52), spaced(aside_label), font=font("SemiBold", 38), fill=WHITE + (150,), anchor="rs")
    draw.text((right, middle + 60), aside.upper(), font=font("SemiBold", 46), fill=WHITE + (190,), anchor="rs")

    # the text on its pane
    draw.rectangle((MARGIN, pane_top, DOCUMENT_WIDTH - MARGIN, height - MARGIN), fill=SHADE + (150,),
                   outline=WHITE + (70,))
    for number, column in enumerate(columns):
        x = MARGIN + PANE_PADDING + number * (column_width + TEXT_COLUMN_GAP)
        y = pane_top + PANE_PADDING
        for index in column:
            draw_block(draw, blocks[index], x, y, column_width)
            y += blocks[index][0]
    return card.png()
