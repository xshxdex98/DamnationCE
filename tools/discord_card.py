"""Discord's pictures in the Glassed theme's look (tools/shell_skin.py): dark
glass in white hairlines over a map, in Rajdhani. discord_feeds.py posts them.

- server_card: the server list. It is wide and always the same size:
  Discord fits a picture into a box wider than it is tall, so a wide one is
  shown largest, and a constant size keeps the message from jumping as games
  come and go.
- document_pages: a release's changelog, or the rules, as pages.
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

# A document is drawn as pages: each a heading on the Glassed theme's strip of
# dark glass between two hairlines (tools/shell_skin.py), over two columns of
# its text on a pane of glass. Discord shows a picture no wider than about a
# third of these, so the text is set large and a long document takes more
# pages, each as wide as it is tall at most, which Discord shows largest.
PAGE_WIDTH, PAGE_HEIGHT = 1600, 1000
STRIP_TOP, STRIP_BOTTOM = 24, 196
PANE_TOP = STRIP_BOTTOM + 32
PANE_PADDING = 40
TEXT_COLUMN_GAP = 56
TEXT_SIZE = 48
LINE_HEIGHT = 64
BLOCK_GAP = 14
HEADING_HEIGHT, HEADING_GAP = 64, 22
INDENTS = {"paragraph": 0, "item": 52, "numbered": 64}


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


def groups_of(draw, blocks, width):
    """the blocks as groups of rows that go down a column together: a block's
    rows, or a heading's and the block's after it. A row is ("heading",
    text), ("line", kind, pieces, marker) with the marker (a bullet or
    number) on an item's first line, or ("gap",) before a block."""
    groups = []
    for kind, text, number in blocks:
        rows = [("gap",)] if groups else []
        if kind == "heading":
            rows.append(("heading", spaced(text)))
        else:
            marker = "•" if kind == "item" else number
            rows += [("line", kind, line, marker if index == 0 else None)
                     for index, line in enumerate(wrap_text(draw, text, width - INDENTS[kind]))]
        if groups and groups[-1][-1][0] == "heading":
            groups[-1] += rows[1:]
        else:
            groups.append(rows)
    return groups


def row_height(row, top_of_column):
    if row[0] == "gap":
        return 0 if top_of_column else BLOCK_GAP
    if row[0] == "heading":
        return HEADING_HEIGHT + (0 if top_of_column else HEADING_GAP)
    return LINE_HEIGHT


def column_height_of(rows):
    return sum(row_height(row, index == 0) for index, row in enumerate(rows))


def lines_in(rows):
    return sum(1 for row in rows if row[0] == "line")


def flow(groups, column_height):
    """the groups down columns no taller than column_height. A group that
    doesn't fit goes on in the next column if two of its lines or more stay
    and go, and no heading is left at a column's foot; else it moves whole."""
    columns, column = [], []
    for group in groups:
        while group:
            if column_height_of(column + group) <= column_height:
                column += group
                break
            fit = 0
            while fit < len(group) and column_height_of(column + group[:fit + 1]) <= column_height:
                fit += 1
            # (one line short of what fits, rather than one left to go on alone)
            while fit > 0 and lines_in(group[fit:]) < 2:
                fit -= 1
            splits = lines_in(group[:fit]) >= 2 and lines_in(group[fit:]) >= 2 and group[fit - 1][0] != "heading"
            if splits or not column:
                # (a group taller than a whole column is split wherever it has to be)
                fit = fit if splits else max(fit, 1)
                column += group[:fit]
                group = group[fit:]
            columns.append(column)
            column = []
    return columns + [column] if column else columns


def draw_row(draw, row, x, y, width, top_of_column):
    if row[0] == "heading":
        y += 0 if top_of_column else HEADING_GAP
        draw.text((x, y + 8), row[1], font=font("SemiBold", 34), fill=WHITE + (175,))
        draw.line((x, y + 52, x + width, y + 52), fill=WHITE + (70,), width=2)
    elif row[0] == "line":
        _, kind, pieces, marker = row
        if marker:
            draw.text((x + (8 if kind == "item" else 0), y), marker, font=text_font(True), fill=GREEN + (230,))
        line_x = x + INDENTS[kind]
        for piece, bold in pieces:
            draw.text((line_x, y), piece, font=text_font(bold), fill=WHITE + ((245,) if bold else (220,)))
            line_x += draw.textlength(piece, font=text_font(bold))


def document_pages(label, title, aside_label, aside, markdown, backdrop_path, user_agent):
    """a document's pages as PNGs: label (small, spaced) over title (large),
    and aside_label over aside, on each page's strip; then its markdown's
    text, two columns a page. The last page is as tall as its text needs."""
    measure = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    blocks = parse_blocks(markdown)
    column_height = PAGE_HEIGHT - PANE_TOP - MARGIN - 2 * PANE_PADDING
    # (a document that fits one column the page's width is set so; else in two)
    column_width = PAGE_WIDTH - 2 * MARGIN - 2 * PANE_PADDING
    columns = flow(groups_of(measure, blocks, column_width), column_height)
    if len(columns) > 1:
        column_width = (column_width - TEXT_COLUMN_GAP) // 2
        columns = flow(groups_of(measure, blocks, column_width), column_height)
    pages = [columns[index:index + 2] for index in range(0, len(columns), 2)]
    # (the last page's two columns as near the same height as they go)
    if len(pages[-1]) == 2:
        rows = pages[-1][0] + pages[-1][1]
        # (its groups again: each begins at a gap)
        groups = [[]]
        for row in rows:
            if row[0] == "gap" and groups[-1]:
                groups.append([])
            groups[-1].append(row)
        height = column_height_of(rows) // 2
        while len(flow(groups, height)) > 2:
            height += LINE_HEIGHT // 4
        pages[-1] = flow(groups, height)
    backdrop = fetch_art(backdrop_path, user_agent)

    images = []
    for number, page in enumerate(pages, 1):
        text_height = max(column_height_of(column) for column in page)
        height = PAGE_HEIGHT if number < len(pages) else PANE_TOP + text_height + 2 * PANE_PADDING + MARGIN
        card = Card(backdrop, (PAGE_WIDTH, height), blur=9, dim=0.45)
        draw = card.draw

        # the heading on its strip
        draw.rectangle((0, STRIP_TOP, PAGE_WIDTH, STRIP_BOTTOM), fill=SHADE + (140,))
        for y in (STRIP_TOP, STRIP_BOTTOM):
            draw.line((0, y, PAGE_WIDTH, y), fill=WHITE + (90,), width=2)
        middle = (STRIP_TOP + STRIP_BOTTOM) // 2
        left, right = MARGIN * 2, PAGE_WIDTH - MARGIN * 2
        page_label = aside_label + (f"   {number} / {len(pages)}" if len(pages) > 1 else "")
        draw.text((left, middle - 40), spaced(label), font=font("SemiBold", 34), fill=WHITE + (190,), anchor="ls")
        draw.text((left, middle + 60), title, font=title_font(100), fill=WHITE + (240,), anchor="ls")
        draw.text((right, middle - 40), spaced(page_label), font=font("SemiBold", 34), fill=WHITE + (150,), anchor="rs")
        draw.text((right, middle + 50), aside.upper(), font=font("SemiBold", 44), fill=WHITE + (190,), anchor="rs")

        # the text on its pane
        draw.rectangle((MARGIN, PANE_TOP, PAGE_WIDTH - MARGIN, height - MARGIN), fill=SHADE + (150,),
                       outline=WHITE + (70,))
        for side, column in enumerate(page):
            x = MARGIN + PANE_PADDING + side * (column_width + TEXT_COLUMN_GAP)
            y = PANE_TOP + PANE_PADDING
            for index, row in enumerate(column):
                draw_row(draw, row, x, y, column_width, index == 0)
                y += row_height(row, index == 0)
        images.append(card.png())
    return images
