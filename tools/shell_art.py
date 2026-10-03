#!/usr/bin/env python3
"""Draws the pictures of the left-hand menus (port/assets/menus/shell):

    python tools/shell_art.py

The look is Halo: Reach's: almost nothing. Plain text over the scene, the
chosen item on a strip of clear glass behind a white tick, hairlines for
edges. A picture is drawn SCALE times its size in the menus' 640x480 screen,
so it stays sharp in a large window. Needs Pillow. tools/shell_skin.py draws
the other screens' pictures with what is here.
"""

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus" / "shell" / "art"
SCALE = 3
SMOOTH = 4              # shapes are drawn this many times larger, then shrunk

WHITE = (255, 255, 255)
SHADE = (6, 8, 12)      # darkens the scene behind text

# sizes in the 640x480 screen (shell/bitmaps.xml gives the same)
PANEL = (346, 480)      # 106 of it lies left of the 4:3 screen, for wide windows
ITEM = (300, 22)
RULE = (420, 2)

TICK = 1.5              # the width of the tick at the left of the chosen item


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


def paint(image, color, mask, strength=None):
    """Lays the color over the image through the mask, and through strength
    (a ramp, or one value of 255) if given."""
    if isinstance(strength, int):
        mask = mask.point(lambda value: value * strength // 255)
    elif strength is not None:
        mask = ImageChops.multiply(mask, strength)
    image.alpha_composite(Image.merge("RGBA", [Image.new("L", image.size, part) for part in color] + [mask]))


def bar(image, size, area, focused, scale=SCALE, tint=WHITE):
    """What an item stands on: nothing, or with the focus a strip of clear
    glass that fades to the right, behind a tick."""
    if not focused:
        return
    left, top, right, bottom = area
    paint(image, tint, polygon(size, box(left, top, right, bottom), scale=scale),
          ramp(image.size, 62, 0, across=True))
    paint(image, tint, polygon(size, box(left, top, left + TICK, bottom), scale=scale))


def panel():
    """A shade over the left of the scene, so the text on it reads."""
    image = new(PANEL)
    paint(image, SHADE, ramp(image.size, 150, 0, across=True))
    return image


def item(focused):
    image = new(ITEM)
    bar(image, ITEM, (0, 0, ITEM[0], ITEM[1]), focused)
    return image


def rule():
    """A hairline that fades out to the right."""
    image = new(RULE)
    line = polygon(RULE, box(0, 0.5, RULE[0], 1.5))
    paint(image, WHITE, line, ramp(image.size, 120, 0, across=True))
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
