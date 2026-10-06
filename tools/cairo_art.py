#!/usr/bin/env python3
"""Draws the Cairo theme's pictures: the menus in the look of Halo 2's.

    python tools/cairo_art.py

Deep navy over the scene, crossed by faint bands of streaming data and a
ruler; blue header bands that step down at a 45 degree angle; steel brackets
on the panes; slate rows, the chosen one lit behind a bright bracket; panes
of dark glass ruled with a fine grid and cut at two corners. The main menu
is a column in the middle of the screen under the title in chrome.

This writes the main menu's pictures (skin/cairo/shell/art); tools/
shell_skin.py draws the rest of the theme with the functions here. Pictures
are drawn in the menus' 640x480 units, SCALE times larger (menu_drawing.py).
The title's letters are taken from the Glassed theme's title picture
(shell/art/title_0.png), so no font is needed.
"""

import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

from menu_drawing import SCALE, box, new, paint, polygon, ramp

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
OUT = MENUS / "skin" / "cairo" / "shell" / "art"
FONTS = MENUS.parent / "fonts"

# ---------- the palette

NAVY_DEEP = (5, 13, 29)
NAVY = (10, 28, 58)
BLUE = (38, 108, 202)
BLUE_DARK = (13, 50, 112)
EDGE = (100, 164, 232)          # the panes' hairlines
ICE = (206, 230, 255)           # the brightest: band edges, the chosen item
SLATE = (66, 92, 132)           # a list's rows
GRID = (86, 142, 212)
STEEL_LIGHT = (232, 240, 248)
STEEL = (150, 172, 202)
STEEL_DARK = (58, 80, 116)
GREEN = (120, 230, 160)         # Online Games: a game to join
WHITE = (255, 255, 255)

# the screens' text, as 0xAARRGGBB (shell_skin.py's TEXT_COLORS)
TEXT = "#FFC8D6EA"
TEXT_DIM = "#FF8EA4C4"

# ---------- the frame every screen shares (screen units)

HEADER_TOP, TOP_LINE = 30, 58   # the header band, and the line it stands on
HEADER_LEFT_OF_SCREEN = 107     # a title's widget begins this far left of the 640 units,
HEADER_WIDGET_TOP = 11          # at the left of a 16:9 window, and this far down
HEADER_STEP = 28                # the band's step down to the line, across
HEADER_TITLE_X = 44             # where a screen's title begins
HEADER_TITLE_HEIGHT = 15        # its capitals' height
HEADER_TITLE_GAP = 26           # between the title's end and the band's step
BOTTOM_LINE = 446               # the line over the buttons' bar
GRID_SPACING = 8
CHAMFER = 10                    # the corners a pane is cut at
BRACKET = 3                     # a steel bracket's thickness
PANE_MARGIN = BRACKET + 2       # between a pane's glass and its picture's edges, for its brackets
ROW_GAP = 0.12                  # of a row's height, left clear above and below its bar

# the main menu (screen units; skin/cairo/shell/bitmaps.xml gives the same)
BACKDROP = (854, 480)           # the whole of a 16:9 window
BACKDROP_SCALE = 2              # (it is soft: fewer texels a unit than SCALE)
ITEM = (300, 22)
RULE = (420, 2)
TITLE = (390, 57)
TITLE_SHINE_FRAMES = 16
TITLE_SHINE_WIDTH = 54


# ---------- pieces

def vertical(size, stops):
    """An RGB picture of size (pixels) whose colour runs down it through
    stops, (fraction of the height, colour) pairs."""
    fractions = np.linspace(0.0, 1.0, size[1])
    positions = [stop[0] for stop in stops]
    rows = np.stack([np.interp(fractions, positions, [stop[1][channel] for stop in stops]) for channel in range(3)],
                    axis=1)
    return Image.fromarray(np.repeat(rows[:, None, :], size[0], axis=1).astype(np.uint8), "RGB")


def lay(image, picture, mask):
    """Lays an RGB picture over the image through the mask."""
    image.alpha_composite(Image.merge("RGBA", (*picture.split(), mask)))


def grid(image, area, scale, strength=26, fade=True):
    """Fine grid lines over the area (units), fading toward its bottom right."""
    left, top, right, bottom = area
    lines = Image.new("L", image.size, 0)
    draw = ImageDraw.Draw(lines)
    width = max(1, round(scale * 0.35))
    for x in np.arange(left, right, GRID_SPACING):
        draw.line([(x * scale, top * scale), (x * scale, bottom * scale)], fill=255, width=width)
    for y in np.arange(top, bottom, GRID_SPACING):
        draw.line([(left * scale, y * scale), (right * scale, y * scale)], fill=255, width=width)
    if fade:
        lines = ImageChops.multiply(lines, ramp(image.size, 255, 90))
        lines = ImageChops.multiply(lines, ramp(image.size, 255, 120, across=True))
    paint(image, GRID, lines, strength)


def steel(image, mask, scale):
    """Brushed steel through the mask: lit along its top and left edges,
    shaded along its bottom and right."""
    lay(image, vertical(image.size, [(0.0, STEEL_LIGHT), (0.5, STEEL), (1.0, STEEL_DARK)]), mask)
    step = max(1, round(scale * 0.6))
    shifted_down = ImageChops.offset(mask, step, step)
    shifted_up = ImageChops.offset(mask, -step, -step)
    paint(image, STEEL_LIGHT, ImageChops.subtract(mask, shifted_down), 230)
    paint(image, STEEL_DARK, ImageChops.subtract(mask, shifted_up), 200)


def chamfered(left, top, right, bottom, cut, corners="tl br"):
    """A box's outline points, cut at 45 degrees at the corners named
    (tl, tr, br, bl) by cut units."""
    points = []
    for corner, (x, y), (dx_in, dy_in), (dx_out, dy_out) in (
            ("tl", (left, top), (0, 1), (1, 0)), ("tr", (right, top), (-1, 0), (0, 1)),
            ("br", (right, bottom), (0, -1), (-1, 0)), ("bl", (left, bottom), (1, 0), (0, -1))):
        if corner in corners.split():
            points += [(x + dx_in * cut, y + dy_in * cut), (x + dx_out * cut, y + dy_out * cut)]
        else:
            points.append((x, y))
    return points


def bracket_shape(left, top, bottom, reach, thickness=BRACKET):
    """A bracket, [, down the left of a span: a bar with arms reaching right
    at its ends, its outer corners cut."""
    cut = thickness * 0.9
    return [(left + cut, top), (left + reach, top), (left + reach, top + thickness),
            (left + thickness, top + thickness), (left + thickness, bottom - thickness),
            (left + reach, bottom - thickness), (left + reach, bottom), (left + cut, bottom),
            (left, bottom - cut), (left, top + cut)]


def corner_bracket(x, y, reach, span, cut, up=False, left=False):
    """A steel corner piece, an L with its outer corner cut, at a pane's
    corner (x, y): its arms reach right and down from there (left: left,
    up: up)."""
    across, down = (-1 if left else 1), (-1 if up else 1)
    inner = BRACKET + cut * 0.4
    return [(x, y + down * cut), (x + across * cut, y), (x + across * reach, y),
            (x + across * reach, y + down * BRACKET), (x + across * inner, y + down * BRACKET),
            (x + across * BRACKET, y + down * inner), (x + across * BRACKET, y + down * span), (x, y + down * span)]


def thin_bracket(image, size, left, top, bottom, scale, color, strength, glow=0):
    """A light bracket, [, beside a row: what marks it, bright when it is chosen."""
    shape = polygon(size, bracket_shape(left, top, bottom, reach=3.0, thickness=1.0), scale=scale)
    if glow:
        paint(image, color, shape.filter(ImageFilter.GaussianBlur(scale * 1.6)), glow)
    paint(image, color, shape, strength)


def data_band(image, top, height, scale, seed, strength=34):
    """A band of streaming data: rows of tiny characters, as Halo 2's
    screens show behind everything."""
    rng = random.Random(seed)
    font = ImageFont.truetype(str(FONTS / "Overpass-750.ttf"), max(6, round(4.6 * scale)))
    mask = Image.new("L", image.size, 0)
    draw = ImageDraw.Draw(mask)
    alphabet = "0123456789ABCDEF:/.-<>[]#"
    rows = max(1, int(height // 5))
    for row in range(rows):
        y = (top + row * 5) * scale
        x = rng.uniform(-20, 0) * scale
        while x < image.width:
            word = "".join(rng.choice(alphabet) for _ in range(rng.randint(2, 9)))
            shade = rng.randint(90, 255)
            draw.text((x, y), word, font=font, fill=shade)
            x += draw.textlength(word, font=font) + rng.uniform(2, 14) * scale
    paint(image, GRID, mask, strength)


def ruler(image, y, scale, strength=60, label_every=48):
    """A ruler across the picture: a hairline with ticks, and now and then a time."""
    width_units = image.width / scale
    mask = Image.new("L", image.size, 0)
    draw = ImageDraw.Draw(mask)
    draw.line([(0, y * scale), (image.width, y * scale)], fill=150, width=max(1, round(scale * 0.4)))
    font = ImageFont.truetype(str(FONTS / "Overpass-750.ttf"), max(6, round(4.2 * scale)))
    for index, x in enumerate(np.arange(4, width_units, 6)):
        long = index % 8 == 0
        draw.line([(x * scale, (y - (3 if long else 1.5)) * scale), (x * scale, y * scale)], fill=255,
                  width=max(1, round(scale * 0.4)))
        if long and index % (label_every // 6) == 0:
            draw.text((x * scale + scale, (y - 9) * scale), f"04:{40 + index // 8:02d}", font=font, fill=200)
    paint(image, EDGE, mask, strength)


# ---------- the main menu

def backdrop():
    """The whole screen in deep navy, the scene showing through faintly in
    the middle, crossed by streaming data and a ruler."""
    image = new(BACKDROP, BACKDROP_SCALE)
    width, height = image.size
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    distance = np.sqrt(((x - width / 2) / (width * 0.55)) ** 2 + ((y - height * 0.45) / (height * 0.7)) ** 2)
    opacity = np.clip(180 + 70 * distance, 0, 245).astype(np.uint8)
    lay(image, vertical(image.size, [(0.0, NAVY_DEEP), (0.45, NAVY), (1.0, NAVY_DEEP)]),
        Image.fromarray(opacity, "L"))
    data_band(image, 150, 14, BACKDROP_SCALE, seed=2)
    ruler(image, 196, BACKDROP_SCALE)
    data_band(image, 372, 10, BACKDROP_SCALE, seed=5, strength=26)
    data_band(image, 452, 10, BACKDROP_SCALE, seed=8, strength=20)
    return image


def item(focused):
    """A main menu item: nothing, or with the focus a line of light under
    the text, brightest in the middle, over a soft glow."""
    image = new(ITEM)
    if not focused:
        return image
    width, height = ITEM
    across = np.linspace(-1.0, 1.0, image.width)
    down = np.linspace(-1.0, 1.0, image.height)
    oval = np.exp(-(across[None, :] / 0.42) ** 2 - (down[:, None] / 0.75) ** 2)
    paint(image, BLUE, Image.fromarray((oval * 255).astype(np.uint8), "L"), 110)
    fade = Image.fromarray(np.repeat(((1 - across ** 2) ** 1.5 * 255)[None, :], image.height, axis=0)
                           .astype(np.uint8), "L")
    line = polygon(ITEM, box(width * 0.08, height - 2.2, width * 0.92, height - 1.4))
    paint(image, ICE, line, fade)
    paint(image, ICE, line.filter(ImageFilter.GaussianBlur(1.5 * SCALE)), fade)
    return image


def rule():
    """A hairline across the bottom of the screen, fading out at both ends."""
    image = new(RULE)
    line = polygon(RULE, box(0, 0.5, RULE[0], 1.5))
    across = np.abs(np.linspace(-1.0, 1.0, image.width))
    fade = Image.fromarray(np.repeat(((1 - across ** 2) * 150)[None, :], image.height, axis=0).astype(np.uint8))
    paint(image, EDGE, line, fade)
    return image


def title_letters():
    """The title's letters, as a mask the size of the Cairo title: taken from
    the Glassed theme's title, whose letters are its bright, solid texels."""
    glassed = Image.open(MENUS / "shell" / "art" / "title_0.png").convert("RGBA")
    pixels = np.asarray(glassed).astype(np.float32)
    brightness = pixels[..., :3].min(axis=2) * pixels[..., 3] / 255.0
    letters = np.clip((brightness - 70.0) / 50.0, 0.0, 1.0)
    mask = Image.fromarray((letters * 255).astype(np.uint8), "L")
    return mask.resize((TITLE[0] * SCALE, TITLE[1] * SCALE), Image.Resampling.LANCZOS)


def title(letters, shine=None):
    """The title in chrome, as Halo 2's: silver above a sharp blue horizon,
    paler again below, in a dark edge and a blue glow; shine, from 0 to 1, a
    band of light that far across it."""
    image = new(TITLE)
    size = image.size
    rows = np.nonzero(np.asarray(letters).max(axis=1) > 128)[0]
    top, bottom = (rows.min(), rows.max()) if len(rows) else (0, size[1])
    edge = letters.filter(ImageFilter.MaxFilter(2 * round(0.6 * SCALE) + 1))
    paint(image, BLUE, edge.filter(ImageFilter.GaussianBlur(4 * SCALE)), 110)
    paint(image, NAVY_DEEP, edge, 235)

    def at(fraction):
        return (top + fraction * (bottom - top)) / size[1]

    chrome = [(0.0, (252, 253, 255)), (at(0.40), (200, 214, 236)), (at(0.50), (86, 118, 178)),
              (at(0.62), (124, 160, 216)), (1.0, (228, 238, 252))]
    lay(image, vertical(size, chrome), letters)
    lit_edge = ImageChops.subtract(letters, ImageChops.offset(letters, 0, max(1, round(0.5 * SCALE))))
    paint(image, WHITE, lit_edge, 220)
    if shine is not None:
        band = Image.new("L", size, 0)
        draw = ImageDraw.Draw(band)
        middle = -TITLE_SHINE_WIDTH * SCALE + shine * (size[0] + 2 * TITLE_SHINE_WIDTH * SCALE)
        half, slant = TITLE_SHINE_WIDTH * SCALE / 2, size[1] * 0.6
        for step in range(24):
            spread = half * (1 - step / 24)
            draw.polygon([(middle - spread + slant, 0), (middle + spread + slant, 0),
                          (middle + spread - slant, size[1]), (middle - spread - slant, size[1])],
                         fill=round(255 * (step + 1) / 24))
        band = band.filter(ImageFilter.GaussianBlur(2 * SCALE))
        paint(image, WHITE, ImageChops.multiply(band, letters), 230)
    return image


# ---------- the other screens' pictures (shell_skin.py redraws them with
# these): each takes the picture redrawn (shell_skin.Picture: its size and
# scale, and the box its shape fills, in screen units) and the frame

def strip(picture):
    """What a whole screen stands on, repeated across it: deep navy, the line
    the header band stands on with a ruler's ticks under it, and the
    buttons' bar at the foot."""
    image = picture.blank()
    width, height = picture.size
    lay(image, vertical(image.size, [(0.0, NAVY_DEEP), (0.5, NAVY), (1.0, NAVY_DEEP)]),
        Image.new("L", image.size, 228))
    paint(image, NAVY_DEEP, picture.shape(box(0, 0, width, TOP_LINE)), 150)
    paint(image, ICE, picture.shape(box(0, TOP_LINE, width, TOP_LINE + 1.1)), 215)
    paint(image, EDGE, picture.shape(box(0, TOP_LINE + 1.1, width, TOP_LINE + 3)), 50)
    paint(image, EDGE, picture.shape(box(width / 2 - 0.2, TOP_LINE + 4, width / 2 + 0.2, TOP_LINE + 6)), 80)
    paint(image, BLUE_DARK, picture.shape(box(0, BOTTOM_LINE, width, height)), 150)
    paint(image, EDGE, picture.shape(box(0, BOTTOM_LINE, width, BOTTOM_LINE + 1)), 170)
    return image


def header(picture, letters):
    """A screen's title on a header band: blue, lit along its top, stepping
    down at its right end to the line the strip carries on across the screen.
    letters is the title's mask, as the original picture has it. The
    picture's widget begins at the left of a 16:9 window and
    HEADER_WIDGET_TOP down (shell_skin.py moves it there)."""
    image = picture.blank()
    scale = picture.scale
    letters = letters.crop(letters.getbbox())
    left_of_screen, down = HEADER_LEFT_OF_SCREEN, HEADER_WIDGET_TOP
    height = HEADER_TITLE_HEIGHT * scale
    width = round(letters.width * height / letters.height)
    letters = letters.resize((width, round(height)), Image.Resampling.LANCZOS)
    title_x = HEADER_TITLE_X + left_of_screen
    band_end = title_x + width / scale + HEADER_TITLE_GAP
    top, bottom = HEADER_TOP - down, TOP_LINE - down
    outline = [(0, top), (band_end, top), (band_end + HEADER_STEP, bottom), (0, bottom)]
    band = picture.shape(outline)
    lay(image, vertical(image.size, [(0.0, BLUE), (bottom / picture.size[1], BLUE_DARK), (1.0, BLUE_DARK)]), band)
    hatch = Image.new("L", image.size, 0)
    draw = ImageDraw.Draw(hatch)
    for x in range(-image.height, image.width, round(5 * scale)):
        draw.line([(x, image.height), (x + image.height, 0)], fill=255, width=max(1, round(scale * 0.5)))
    paint(image, WHITE, ImageChops.multiply(hatch, band), 14)
    edge = picture.shape([(0, top), (band_end, top), (band_end + HEADER_STEP, bottom)], outline=1.1, closed=False)
    paint(image, ICE, edge, 230)
    paint(image, ICE, edge.filter(ImageFilter.GaussianBlur(scale)), 90)
    placed = Image.new("L", image.size, 0)
    placed.paste(letters, (round(title_x * scale), round(((top + bottom) / 2) * scale - letters.height / 2)))
    paint(image, NAVY_DEEP, placed.filter(ImageFilter.GaussianBlur(scale)), 160)
    paint(image, WHITE, placed, 245)
    return image


def pane(picture, frame):
    """A pane of dark glass ruled with a grid, cut at its top left and bottom
    right corners, in a hairline (bright with the focus, frame 1), with
    steel brackets hugging its right side's ends."""
    image = picture.blank()
    outer = picture.box
    # (the glass inside the picture's shape, the brackets in the margin round it)
    left, top, right, bottom = outer[0] + 1, outer[1] + PANE_MARGIN, outer[2] - PANE_MARGIN, outer[3] - PANE_MARGIN
    cut = min(CHAMFER, (bottom - top) / 5, (right - left) / 5)
    shape = chamfered(left, top, right, bottom, cut)
    fill = picture.shape(shape)
    paint(image, NAVY, fill, 200)
    lines = picture.blank()
    grid(lines, (left, top, right, bottom), picture.scale)
    image.alpha_composite(Image.merge("RGBA", (*lines.split()[:3], ImageChops.multiply(lines.split()[3], fill))))
    paint(image, ICE if frame else EDGE, picture.shape(shape, outline=0.8), 240 if frame else 170)
    reach, span = min(36, (right - left) * 0.3), min(28, (bottom - top) * 0.3)
    corner_cut = cut + PANE_MARGIN * 0.4
    corners = ImageChops.lighter(
        picture.shape(corner_bracket(outer[2], outer[1], reach, span, corner_cut, left=True)),
        picture.shape(corner_bracket(outer[2], outer[3], reach, span, corner_cut, up=True, left=True)))
    steel(image, corners, picture.scale)
    return image


def bar(picture, frame, joinable=False, selection_width=None):
    """What an option or a list row stands on: a slate bar behind a faint
    bracket, or with the focus (frame 1) a lit bar behind a bright one. A
    third frame is lit too; Online Games' (joinable) is green. A selection
    list's row (selection_width) shows nothing until chosen, then lights
    that much of the row."""
    image = picture.blank()
    left, top, right, bottom = picture.box
    if selection_width:
        if frame == 0:
            return image
        left, right = 0, min(selection_width, picture.size[0])
    inner = left + 5
    gap = (bottom - top) * ROW_GAP
    top, bottom = top + gap, bottom - gap
    body = picture.shape(box(inner, top, right, bottom))
    if frame == 0:
        paint(image, SLATE, body, ramp(image.size, 110, 60, across=True))
        paint(image, EDGE, picture.shape(box(inner, top, right, top + 0.6)), 40)
        thin_bracket(image, picture.size, left, top, bottom, picture.scale, STEEL, 110)
        return image
    tint = GREEN if joinable else ICE
    paint(image, tint, body, ramp(image.size, 150, 60, across=True))
    paint(image, WHITE, picture.shape(box(inner, top, right, top + 0.8)), 110)
    thin_bracket(image, picture.size, left, top - 0.5, bottom + 0.5, picture.scale, WHITE, 255, glow=150)
    return image


def tab(picture, frame):
    """The heading of a game list's column: a blue band over a bright hairline."""
    image = picture.blank()
    left, top, right, bottom = picture.box
    area = picture.shape(box(left, top, right, bottom))
    strength = 200 if frame else 120
    lay(image, vertical(image.size, [(0.0, BLUE), (1.0, BLUE_DARK)]), area.point(lambda v: v * strength // 255))
    paint(image, ICE, picture.shape(box(left, bottom - 1, right, bottom)), 220)
    return image


def arrow(picture):
    """An arrow, its blue made the look's ice (the red ones stay red)."""
    pixels = picture.pixels.copy()
    blue = pixels[:, :, 2] > pixels[:, :, 0]
    pixels[blue, 0:3] = ICE
    return Image.fromarray(pixels, "RGBA")


def three_part_box(picture, part):
    """A part of a box the game puts together across the screen (the pause
    menus'): a gridded pane, its left end cut at the top with a steel
    bracket, its right end cut at the bottom."""
    image = picture.blank()
    outer_left, top, right, bottom = picture.box
    left = outer_left + PANE_MARGIN if part == "left" else 0
    if part != "right":
        right = picture.size[0]
    cut = min(CHAMFER, (bottom - top) / 4)
    shape = chamfered(left, top, right, bottom, cut, {"left": "tl", "right": "br"}.get(part, ""))
    fill = picture.shape(shape)
    paint(image, NAVY, fill, 215)
    lines = picture.blank()
    grid(lines, (left, top, right, bottom), picture.scale, fade=False, strength=22)
    image.alpha_composite(Image.merge("RGBA", (*lines.split()[:3], ImageChops.multiply(lines.split()[3], fill))))
    if part == "left":
        edges = [[(right, top), (left + cut, top), (left, top + cut), (left, bottom), (right, bottom)]]
    elif part == "right":
        edges = [[(left, top), (right, top), (right, bottom - cut), (right - cut, bottom), (left, bottom)]]
    else:
        edges = [[(left, top), (right, top)], [(left, bottom), (right, bottom)]]
    for points in edges:
        paint(image, EDGE, picture.shape(points, outline=0.8, closed=False), 190)
    if part == "left":
        steel(image, picture.shape(bracket_shape(outer_left, top + cut, bottom, reach=PANE_MARGIN + 6)),
              picture.scale)
    return image


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    letters = title_letters()
    pictures = {
        "panel.png": backdrop(),
        "item_0.png": item(False),
        "item_1.png": item(True),
        "rule.png": rule(),
        "title_0.png": title(letters),
    }
    for frame in range(TITLE_SHINE_FRAMES):
        pictures[f"title_{frame + 1}.png"] = title(letters, frame / (TITLE_SHINE_FRAMES - 1))
    for name, image in pictures.items():
        image.save(OUT / name, optimize=True)
        print(f"{name}: {image.size[0]}x{image.size[1]}")


if __name__ == "__main__":
    main()
