#!/usr/bin/env python3
"""Dresses the PC version's screens (port/assets/menus/ce) in this client's
look, the left-hand menus' (tools/shell_art.py):

    python tools/shell_skin.py

Those screens are built from a few shared pictures: the bars behind options
and list items, the panes behind lists, the strip behind a whole screen, and
each screen's header. This redraws them, each where the picture it replaces
has its shape, and gives the screens' text the look's color. Nothing in ce/
is changed: the new files go to port/assets/menus/skin, under the same
paths, and tools/embed_assets.py embeds a file there in place of the one it
shadows. Run it again after ce/ changes (tools/ce_menus.py). Needs Pillow
and NumPy.
"""

import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np
from PIL import Image

import shell_art
from shell_art import SHADE, WHITE, box, paint, polygon, ramp

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
SKIN = MENUS / "skin"

GREEN = (150, 255, 180)     # the game list's third frame: a game to join
VISIBLE = 8                 # alpha above which a texel is part of a picture's shape
# where a screen's glass begins and ends: under its header, over its foot
STRIP_TOP, STRIP_BOTTOM = 66, 446
HAIRLINE = 0.8

# the bitmaps redrawn, by the last part of their names
PANES = ("alert_bkd", "list_field", "sel_list_desc_bkd", "spinner_list_3_wide_item_background",
         "controller_border", "device_border", "join_game_border")
BARS = ("option_bkds", "option_bkds_sm", "option_bkds_sm2", "text_button_background", "list_item_bkd",
        "list_item_bkd_top", "sel_list_item_bkd", "sel_list_item_bkd_top", "server_item_bkds")
TABS = ("server_header_background_0", "server_header_background_1", "server_header_background_2",
        "server_header_background_3")
STRIPS = ("gradient", "gradient_big")
ARROWS = ("arrow_sm_left", "arrow_sm_right", "arrow_sm_up", "arrow_sm_down")

# the screens' text colors, and the look's
TEXT_COLORS = {"#FF2896FF": "#FFD2D6DA", "#FF0080FF": "#FFA8ACB0"}


class Picture:
    """One frame of a bitmap: its PNG, its size in the 640x480 screen, and
    the box its shape fills (in units of that screen), if it has one."""

    def __init__(self, png, width, height):
        self.png = png
        self.pixels = np.array(Image.open(MENUS / png).convert("RGBA"))
        self.scale = self.pixels.shape[1] // width
        # the size the picture has, which may be less than the frame's
        self.size = (self.pixels.shape[1] // self.scale, self.pixels.shape[0] // self.scale)
        rows, columns = np.nonzero(self.pixels[:, :, 3] > VISIBLE)
        self.box = None
        if len(rows):
            self.box = (round(columns.min() / self.scale), round(rows.min() / self.scale),
                        round((columns.max() + 1) / self.scale), round((rows.max() + 1) / self.scale))

    def blank(self):
        return Image.new("RGBA", (self.pixels.shape[1], self.pixels.shape[0]), (0, 0, 0, 0))

    def shape(self, points, outline=0.0):
        return polygon(self.size, points, outline, self.scale)


def pane(picture, frame):
    """A pane of dark glass in a hairline; frame 1 has the focus."""
    image = picture.blank()
    shape = box(*picture.box)
    paint(image, SHADE, picture.shape(shape), 120)
    paint(image, WHITE, picture.shape(shape, outline=HAIRLINE), 170 if frame else 70)
    return image


def bar(picture, frame, name):
    """What an option or a list item stands on: the faintest glass, or with
    the focus (frame 1) the left-hand menus' strip. A third frame is the
    strip too; the game list's is green."""
    image = picture.blank()
    if frame == 0:
        paint(image, WHITE, picture.shape(box(*picture.box)), ramp(image.size, 16, 0, across=True))
        return image
    tint = GREEN if frame == 2 and name == "server_item_bkds" else WHITE
    shell_art.bar(image, picture.size, picture.box, True, picture.scale, tint)
    return image


def tab(picture, frame):
    """The heading of a column of the game list: glass over a hairline."""
    image = picture.blank()
    left, top, right, bottom = picture.box
    paint(image, WHITE, picture.shape(box(left, top, right, bottom)), 70 if frame else 26)
    paint(image, WHITE, picture.shape(box(left, bottom - HAIRLINE, right, bottom)), 150)
    return image


def strip(picture):
    """What a whole screen stands on: dark glass between two hairlines. The
    game repeats the strip across the screen."""
    image = picture.blank()
    width = picture.size[0]
    paint(image, SHADE, picture.shape(box(0, STRIP_TOP, width, STRIP_BOTTOM)), 140)
    for y in (STRIP_TOP, STRIP_BOTTOM - HAIRLINE):
        paint(image, WHITE, picture.shape(box(0, y, width, y + HAIRLINE)), 90)
    return image


def header(picture):
    """A screen's heading: its letters alone, in clear white."""
    image = picture.blank()
    # the letters are the picture's brightest green; the glow around them is not
    green = picture.pixels[:, :, 1].astype(np.float32) * picture.pixels[:, :, 3] / 255.0
    brightest = float(np.percentile(green[green > 0], 99))
    letters = np.clip((green - 0.6 * brightest) / (0.3 * brightest), 0.0, 1.0)
    paint(image, WHITE, Image.fromarray((letters * 255).astype(np.uint8), "L"), 215)
    return image


def arrow(picture):
    """An arrow, its blue made white (the red ones stay red)."""
    pixels = picture.pixels.copy()
    blue = pixels[:, :, 2] > pixels[:, :, 0]
    pixels[blue, 0:3] = WHITE
    return Image.fromarray(pixels, "RGBA")


def redrawn(name, picture, frame):
    """The picture of a bitmap's frame in the look, or None to keep it."""
    last = name.rsplit("/", 1)[-1]
    if last in ARROWS:
        return arrow(picture)
    if picture.box is None:
        return None
    if last in PANES:
        return pane(picture, frame)
    if last in BARS:
        return bar(picture, frame, last)
    if last in TABS:
        return tab(picture, frame)
    if last in STRIPS:
        return strip(picture)
    if last.startswith("header_"):
        return header(picture)
    return None


def main():
    if SKIN.exists():
        shutil.rmtree(SKIN)
    count = 0
    for bitmap in ET.parse(MENUS / "ce" / "bitmaps.xml").getroot().iter("bitmap"):
        for frame, element in enumerate(bitmap.iter("frame")):
            if not element.get("png"):
                continue
            picture = Picture(element.get("png"), int(element.get("width")), int(element.get("height")))
            image = redrawn(bitmap.get("name"), picture, frame)
            if image is not None:
                (SKIN / picture.png).parent.mkdir(parents=True, exist_ok=True)
                image.save(SKIN / picture.png)
                count += 1
    print(f"{count} pictures")

    count = 0
    for file in sorted((MENUS / "ce").glob("*.xml")):
        text = file.read_text(encoding="utf-8")
        colored = text
        for old, new in TEXT_COLORS.items():
            colored = colored.replace(f'color="{old}"', f'color="{new}"')
        if colored != text:
            (SKIN / "ce").mkdir(parents=True, exist_ok=True)
            (SKIN / "ce" / file.name).write_text(colored, encoding="utf-8", newline="\n")
            count += 1
    print(f"{count} screens' text colors")


if __name__ == "__main__":
    main()
