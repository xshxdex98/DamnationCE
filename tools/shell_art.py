#!/usr/bin/env python3
"""Draws the pictures of the left-hand menus (port/assets/menus/shell):

    python tools/shell_art.py [--title-font Halo.ttf]

The look is Halo: Reach's: almost nothing. Plain text over the scene, the
chosen item on a strip of clear glass behind a white tick, hairlines for
edges. A picture is drawn SCALE times its size in the menus' 640x480 screen,
so it stays sharp in a large window (menu_drawing.py). Needs Pillow.
tools/shell_skin.py draws the other screens' pictures with what is here.

The title, OpenCE, is set in a typeface given with --title-font (the
pictures are kept; the font is not part of the repository) in glass
letters, as a resting frame and then TITLE_SHINE_FRAMES frames of a band of
light sliding across them, which the menus show every so often (the port's
"port title shine", menu_functions.c).
"""

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

from menu_drawing import SCALE, box, new, paint, polygon, ramp

OUT = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus" / "shell" / "art"

WHITE = (255, 255, 255)
SHADE = (6, 8, 12)      # darkens the scene behind text

# sizes in the 640x480 screen (shell/bitmaps.xml gives the same)
PANEL = (346, 480)      # 106 of it lies left of the 4:3 screen, for wide windows
ITEM = (300, 22)
RULE = (420, 2)
TITLE = (330, 48)
TITLE_TEXT = "OpenCE"
TITLE_SHINE_FRAMES = 16
TITLE_SHINE_WIDTH = 46  # the band of light, across, in the menus' units

TICK = 1.5              # the width of the tick at the left of the chosen item


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


def title_letters(font_path):
    """The title's letters as a mask, as wide as the picture allows and
    centered in its height."""
    width, height = TITLE[0] * SCALE, TITLE[1] * SCALE
    margin = 6 * SCALE
    probe = ImageFont.truetype(str(font_path), 200)
    left, top, right, bottom = probe.getbbox(TITLE_TEXT)
    font = ImageFont.truetype(str(font_path), round(200 * (width - 2 * margin) / (right - left)))
    left, top, right, bottom = font.getbbox(TITLE_TEXT)
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).text((margin - left, (height - (bottom - top)) // 2 - top), TITLE_TEXT, font=font, fill=255)
    return mask


def title(letters, shine=None):
    """The title in glass: a soft shadow and glow behind letters lit from
    above with a bright edge; shine, from 0 to 1, a band of light that far
    across them."""
    image = new(TITLE)
    size = image.size
    paint(image, SHADE, letters.filter(ImageFilter.GaussianBlur(3 * SCALE)), 150)
    paint(image, WHITE, letters.filter(ImageFilter.GaussianBlur(5 * SCALE)), 50)
    paint(image, WHITE, letters, ramp(size, 215, 125))
    edge = ImageChops.subtract(letters.filter(ImageFilter.MaxFilter(3)), letters.filter(ImageFilter.MinFilter(3)))
    paint(image, WHITE, edge, 140)
    if shine is not None:
        # (a slanted band, brightest in its middle, over the letters and a
        # little bloom around them)
        band = Image.new("L", size, 0)
        draw = ImageDraw.Draw(band)
        travel = size[0] + 2 * TITLE_SHINE_WIDTH * SCALE
        middle = -TITLE_SHINE_WIDTH * SCALE + shine * travel
        half = TITLE_SHINE_WIDTH * SCALE / 2
        slant = size[1] * 0.6
        for step in range(24):
            spread = half * (1 - step / 24)
            draw.polygon([(middle - spread + slant, 0), (middle + spread + slant, 0),
                          (middle + spread - slant, size[1]), (middle - spread - slant, size[1])],
                         fill=round(255 * (step + 1) / 24))
        band = band.filter(ImageFilter.GaussianBlur(2 * SCALE))
        paint(image, WHITE, ImageChops.multiply(band, letters))
        paint(image, WHITE, ImageChops.multiply(band, letters.filter(ImageFilter.GaussianBlur(4 * SCALE))), 130)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--title-font", help="the typeface to set the title in (its pictures are kept without it)")
    arguments = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    pictures = {
        "panel.png": panel(),
        "item_0.png": item(False),
        "item_1.png": item(True),
        "rule.png": rule(),
    }
    if arguments.title_font:
        letters = title_letters(arguments.title_font)
        pictures["title_0.png"] = title(letters)
        for frame in range(TITLE_SHINE_FRAMES):
            pictures[f"title_{frame + 1}.png"] = title(letters, frame / (TITLE_SHINE_FRAMES - 1))
    for name, image in pictures.items():
        image.save(OUT / name)
        print(f"{name}: {image.size[0]}x{image.size[1]}")


if __name__ == "__main__":
    main()
