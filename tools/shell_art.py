#!/usr/bin/env python3
"""Draws the pictures of the left-hand menus (port/assets/menus/shell):

    python tools/shell_art.py

The look is Halo 2's: navy panels between bands of blue with a step in
them, and slate bars ruled with fine lines behind a bright bracket. A
picture is drawn SCALE times its size in the menus' 640x480 screen, so it
stays sharp in a large window. Needs Pillow. tools/shell_skin.py draws the
other screens' pictures with what is here.
"""

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus" / "shell" / "art"
SCALE = 3
SMOOTH = 4              # shapes are drawn this many times larger, then shrunk

DEEP = (17, 27, 52)     # the panels' ends, beyond the bands
NAVY = (30, 50, 88)     # the panels
BAND = (40, 86, 142)    # the bands across a panel
LINE = (84, 150, 222)   # the bands' and panes' edges
SLATE = (126, 146, 184)  # the bars
BRIGHT = (222, 238, 255)  # brackets and letters
SOLID = 240             # how much of the scene a panel hides (of 255)

# sizes in the 640x480 screen (shell/bitmaps.xml gives the same)
PANEL = (346, 480)      # 106 of it lies left of the 4:3 screen, for wide windows
ITEM = (184, 30)
RULE = (168, 2)

PANEL_RIGHT = 322       # the panel's right edge, in the panel
BAND_TOP, BAND_BOTTOM = 50, 436
BAND_HEIGHT, BAND_STEP = 9, 7


def new(size, scale=SCALE):
    return Image.new("RGBA", (size[0] * scale, size[1] * scale), (0, 0, 0, 0))


def ramp(size, start, end, across=False):
    """An image of size (pixels) whose values run from start to end, down it or across it."""
    image = Image.linear_gradient("L")
    if across:
        image = image.transpose(Image.Transpose.ROTATE_90)
    image = image.resize(size, Image.Resampling.BILINEAR)
    return image.point(lambda value: round(start + (end - start) * value / 255))


def polygon(size, points, outline=0.0, scale=SCALE):
    """The polygon (in units of the 640x480 screen) as a mask of an image of
    size (units), scale pixels to a unit: filled, or its outline that many
    units wide."""
    factor = scale * SMOOTH
    mask = Image.new("L", (size[0] * factor, size[1] * factor), 0)
    scaled = [(x * factor, y * factor) for x, y in points]
    draw = ImageDraw.Draw(mask)
    if outline:
        draw.line(scaled + scaled[:1], fill=255, width=round(outline * factor), joint="curve")
    else:
        draw.polygon(scaled, fill=255)
    return mask.resize((size[0] * scale, size[1] * scale), Image.Resampling.LANCZOS)


def box(left, top, right, bottom):
    return [(left, top), (right, top), (right, bottom), (left, bottom)]


def path(points):
    """An open line's points, as the closed outline polygon() draws: there and back."""
    return points + points[-2:0:-1]


def paint(image, color, mask, strength=None):
    """Lays the color over the image through the mask, and through strength
    (a ramp, or one value of 255) if given."""
    if isinstance(strength, int):
        mask = mask.point(lambda value: value * strength // 255)
    elif strength is not None:
        mask = ImageChops.multiply(mask, strength)
    image.alpha_composite(Image.merge("RGBA", [Image.new("L", image.size, part) for part in color] + [mask]))


def glow(mask, units, scale=SCALE):
    """The mask spread out by so many units: light around a bright shape."""
    return mask.filter(ImageFilter.GaussianBlur(units * scale))


def ruled(size, scale):
    """Fine lines across an image, one every two units, as a mask."""
    lines = Image.new("L", size, 0)
    draw = ImageDraw.Draw(lines)
    for y in range(scale, size[1], 2 * scale):
        draw.rectangle([0, y, size[0], y + scale - 1], fill=255)
    return lines


def band(image, size, left, right, y, step_at, scale=SCALE, rising=True):
    """A band of blue across a panel at y, with a step up (or down) at
    step_at, and a line of light along its upper edge."""
    step = -BAND_STEP if rising else BAND_STEP
    upper = [(left, y), (step_at, y), (step_at + BAND_STEP, y + step), (right, y + step)]
    lower = [(x, edge + BAND_HEIGHT) for x, edge in reversed(upper)]
    paint(image, BAND, polygon(size, upper + lower, scale=scale))
    paint(image, LINE, polygon(size, path(upper), outline=1.0, scale=scale))


def bracket(image, size, left, top, bottom, scale=SCALE, strength=255):
    """A bright bracket up the left end of a bar, with its glow."""
    shape = polygon(size, path([(left + 4, top + 1), (left + 1, top + 1), (left + 1, bottom - 1),
                                (left + 4, bottom - 1)]), outline=1.6, scale=scale)
    paint(image, LINE, glow(shape, 2.0, scale), strength)
    paint(image, BRIGHT, shape, strength)


def bar(image, size, area, focused, scale=SCALE, tint=SLATE):
    """A slate bar ruled with fine lines behind a bracket: dim, or lit with the focus."""
    left, top, right, bottom = area
    inside = polygon(size, box(left, top, right, bottom), scale=scale)
    paint(image, DEEP, inside, 150 if focused else 90)
    paint(image, tint, inside, ramp(image.size, 215, 150, across=True) if focused
          else ramp(image.size, 70, 40, across=True))
    paint(image, DEEP, ImageChops.multiply(inside, ruled(image.size, scale)), 46 if focused else 30)
    if focused:
        paint(image, BRIGHT, polygon(size, path([(left, top + 0.5), (right, top + 0.5)]), outline=1.0, scale=scale), 150)
    bracket(image, size, left, top, bottom, scale, 255 if focused else 110)


def panel():
    """What the menu stands on: a navy column down the left of the screen,
    crossed by two stepped bands, with a line of light down its right edge."""
    image = new(PANEL)
    column = polygon(PANEL, box(0, 0, PANEL_RIGHT, PANEL[1]))
    paint(image, DEEP, column, SOLID)
    middle = polygon(PANEL, box(0, BAND_TOP, PANEL_RIGHT, BAND_BOTTOM + BAND_HEIGHT))
    paint(image, NAVY, middle, ramp(image.size, SOLID, 150))
    band(image, PANEL, 0, PANEL_RIGHT, BAND_TOP, 236)
    band(image, PANEL, 0, PANEL_RIGHT, BAND_BOTTOM, 150, rising=False)
    edge = polygon(PANEL, path([(PANEL_RIGHT, 0), (PANEL_RIGHT, PANEL[1])]), outline=1.2)
    paint(image, LINE, glow(edge, 2.5), 200)
    paint(image, LINE, edge)
    return image


def item(focused):
    image = new(ITEM)
    bar(image, ITEM, (0, 1, ITEM[0], ITEM[1] - 1), focused)
    return image


def rule():
    """A line under the title, fading to the right."""
    image = new(RULE)
    paint(image, LINE, Image.new("L", image.size, 255), ramp(image.size, 255, 0, across=True))
    return image


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    pictures = {
        "panel.png": panel(),
        "item_0.png": item(False),
        "item_1.png": item(True),
        "rule.png": rule(),
    }
    for name, image in pictures.items():
        image.save(OUT / name)
        print(f"{name}: {image.size[0]}x{image.size[1]}")


if __name__ == "__main__":
    main()
