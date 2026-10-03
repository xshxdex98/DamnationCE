#!/usr/bin/env python3
"""Draws the pictures of the left-hand menus (port/assets/menus/shell):

    python tools/shell_art.py

The look is glass: panes that tint the scene behind them rather than hide
it, lit along their top edges, with one corner cut. A picture is drawn SCALE
times its size in the menus' 640x480 screen, so it stays sharp in a large
window. Needs Pillow.
"""

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus" / "shell" / "art"
SCALE = 3
SMOOTH = 4              # shapes are drawn this many times larger, then shrunk

SHADE = (5, 11, 20)     # behind the glass, so text reads over a bright scene
GLASS = (132, 184, 224)
LIGHT = (214, 236, 255)
AMBER = (255, 176, 64)

# sizes in the 640x480 screen (shell/bitmaps.xml gives the same)
PANEL = (346, 480)      # 106 of it lies left of the 4:3 screen, for wide windows
ITEM = (184, 30)
RULE = (168, 2)

# the pane of the panel, in the panel
PANE = (112, 56, 320, 440)
PANE_CUT = 20
ITEM_CUT = 9


def new(size):
    return Image.new("RGBA", (size[0] * SCALE, size[1] * SCALE), (0, 0, 0, 0))


def ramp(size, start, end, across=False):
    """An image of size (pixels) whose values run from start to end, down it or across it."""
    image = Image.linear_gradient("L")
    if across:
        image = image.transpose(Image.Transpose.ROTATE_90)
    image = image.resize(size, Image.Resampling.BILINEAR)
    return image.point(lambda value: round(start + (end - start) * value / 255))


def polygon(size, points, outline=0.0):
    """The polygon (in units of the 640x480 screen) as a mask of an image of
    size (units): filled, or its outline that many units wide."""
    factor = SCALE * SMOOTH
    mask = Image.new("L", (size[0] * factor, size[1] * factor), 0)
    scaled = [(x * factor, y * factor) for x, y in points]
    draw = ImageDraw.Draw(mask)
    if outline:
        draw.line(scaled + scaled[:1], fill=255, width=round(outline * factor), joint="curve")
    else:
        draw.polygon(scaled, fill=255)
    return mask.resize((size[0] * SCALE, size[1] * SCALE), Image.Resampling.LANCZOS)


def paint(image, color, mask, strength=None):
    """Lays the color over the image through the mask, and through strength (a ramp) if given."""
    if strength is not None:
        mask = ImageChops.multiply(mask, strength)
    image.alpha_composite(Image.merge("RGBA", [Image.new("L", image.size, part) for part in color] + [mask]))


def cut_corner_box(left, top, right, bottom, cut, corner):
    """A box with one corner cut: 'top right' or 'bottom right'."""
    if corner == "top right":
        return [(left, top), (right - cut, top), (right, top + cut), (right, bottom), (left, bottom)]
    return [(left, top), (right, top), (right, bottom - cut), (right - cut, bottom), (left, bottom)]


def panel():
    """What the menu stands on: a shade over the left of the scene, and on it
    a pane of glass with its top right corner cut."""
    image = new(PANEL)
    size = image.size
    paint(image, SHADE, ramp(size, 190, 0, across=True))

    left, top, right, bottom = PANE
    shape = cut_corner_box(left, top, right, bottom, PANE_CUT, "top right")
    inside = polygon(PANEL, shape)
    paint(image, SHADE, inside, ramp(size, 90, 125))
    paint(image, GLASS, inside, ramp(size, 62, 14))

    # the light on the glass: a sheen down from its top edge, and its edges
    sheen = Image.new("L", size, 0)
    sheen.paste(ramp((size[0], 90 * SCALE), 46, 0), (0, top * SCALE))
    paint(image, LIGHT, inside, sheen)
    paint(image, LIGHT, polygon(PANEL, shape, outline=1.0), ramp(size, 150, 40))
    top_edge = [(left, top), (right - PANE_CUT, top), (right, top + PANE_CUT)]
    edge = polygon(PANEL, top_edge + top_edge[-2::-1], outline=1.4)
    paint(image, LIGHT, edge, ramp(size, 255, 255))
    return image


def item(focused):
    """A menu item's background, its bottom right corner cut: a faint pane,
    or with the focus a lit one behind an amber tick."""
    image = new(ITEM)
    size = image.size
    shape = cut_corner_box(0, 0, ITEM[0], ITEM[1], ITEM_CUT, "bottom right")
    inside = polygon(ITEM, shape)
    if not focused:
        paint(image, GLASS, inside, ramp(size, 30, 6, across=True))
        return image

    paint(image, GLASS, inside, ramp(size, 150, 40, across=True))
    sheen = Image.new("L", size, 0)
    sheen.paste(ramp((size[0], size[1] // 2), 70, 10), (0, 0))
    paint(image, LIGHT, inside, sheen)
    paint(image, LIGHT, polygon(ITEM, shape, outline=1.0), ramp(size, 170, 30, across=True))
    paint(image, LIGHT, polygon(ITEM, [(0, 0.5), (ITEM[0], 0.5)], outline=1.0), ramp(size, 255, 60, across=True))
    paint(image, AMBER, polygon(ITEM, [(0, 0), (3, 0), (3, ITEM[1]), (0, ITEM[1])]))
    return image


def rule():
    """A line under the title: an amber dash, then light fading to the right."""
    image = new(RULE)
    size = image.size
    paint(image, LIGHT, Image.new("L", size, 255), ramp(size, 200, 0, across=True))
    paint(image, AMBER, polygon(RULE, [(0, 0), (14, 0), (14, RULE[1]), (0, RULE[1])]))
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
