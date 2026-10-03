#!/usr/bin/env python3
"""Draws the pictures of the left-hand menus (port/assets/menus/shell):

    python tools/shell_art.py

A picture is drawn SCALE times its size in the menus' 640x480 screen, so it
stays sharp in a large window. Needs Pillow.
"""

from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus" / "shell" / "art"
SCALE = 3

NAVY_TOP = (12, 26, 44)
NAVY_BOTTOM = (4, 9, 17)
ACCENT = (104, 204, 255)

# sizes in the 640x480 screen (shell/bitmaps.xml gives the same)
PANEL = (346, 480)      # 106 of it lies left of the 4:3 screen, for wide windows
ITEM = (184, 30)
RULE = (168, 2)


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def new(size):
    return Image.new("RGBA", (size[0] * SCALE, size[1] * SCALE), (0, 0, 0, 0))


def panel():
    """The column the menu stands on: dark, fading out to the right, with a
    thin bright line down its edge."""
    image = new(PANEL)
    width, height = image.size
    pixels = image.load()
    solid = round(width * 0.90)
    for y in range(height):
        color = lerp(NAVY_TOP, NAVY_BOTTOM, y / (height - 1))
        for x in range(width):
            fade = 1.0 if x < solid else 1.0 - (x - solid) / (width - solid)
            pixels[x, y] = color + (round(232 * fade),)
    line = solid
    for y in range(height):
        # the line is brightest a third of the way down and fades to both ends
        t = y / (height - 1)
        strength = max(0.0, 1.0 - abs(t - 0.33) / 0.67) ** 1.5
        for x in range(line, line + SCALE):
            pixels[x, y] = ACCENT + (round(60 + 195 * strength),)
    return image


def item(focused):
    """A menu item's background: nothing, or with the focus a bar that
    fades to the right behind a bright tick."""
    image = new(ITEM)
    if not focused:
        return image
    width, height = image.size
    pixels = image.load()
    for x in range(width):
        alpha = round(120 * (1.0 - x / (width - 1)) ** 1.6)
        for y in range(height):
            pixels[x, y] = ACCENT + (alpha,)
    ImageDraw.Draw(image).rectangle([0, 0, 2 * SCALE - 1, height - 1], fill=ACCENT + (255,))
    return image


def rule():
    """A line under the title, fading to the right."""
    image = new(RULE)
    width, height = image.size
    pixels = image.load()
    for x in range(width):
        alpha = round(230 * (1.0 - x / (width - 1)))
        for y in range(height):
            pixels[x, y] = ACCENT + (alpha,)
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
