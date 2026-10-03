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
from shell_art import BAND, BAND_HEIGHT, BRIGHT, DEEP, LINE, NAVY, SLATE, SOLID, box, glow, paint, path, polygon, ramp

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
SKIN = MENUS / "skin"

GREEN = (104, 190, 140)     # the game list's third frame: a game to join
VISIBLE = 8                 # alpha above which a texel is part of a picture's shape
SCREEN_HEIGHT = 480
# the bands across a screen: under its header, and over its buttons' foot
STRIP_BAND_TOP, STRIP_BAND_BOTTOM = 62, 446

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
TEXT_COLORS = {"#FF2896FF": "#FFE6F0FF", "#FF0080FF": "#FF9DB9E6"}


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
    """A dark pane edged with light, its top right corner cut; frame 1 has the focus."""
    image = picture.blank()
    left, top, right, bottom = picture.box
    cut = min(14, (right - left) // 6, (bottom - top) // 6)
    shape = [(left, top), (right - cut, top), (right, top + cut), (right, bottom), (left, bottom)]
    inside = picture.shape(shape)
    paint(image, DEEP, inside, SOLID)
    paint(image, NAVY, inside, ramp(image.size, 120, 0))
    edge = picture.shape(shape, outline=1.2)
    if frame:
        paint(image, LINE, glow(edge, 2.0, picture.scale), 220)
    paint(image, BRIGHT if frame else LINE, edge)
    return image


def bar(picture, frame, name):
    """The bar behind an option or a list item: dim, lit with the focus
    (frame 1), or as a third frame lit; the game list's third is green."""
    image = picture.blank()
    tint = GREEN if frame == 2 and name == "server_item_bkds" else SLATE
    shell_art.bar(image, picture.size, picture.box, frame > 0, picture.scale, tint)
    return image


def tab(picture, frame):
    """The heading of a column of the game list: a block of the bands' blue."""
    image = picture.blank()
    left, top, right, bottom = picture.box
    inside = picture.shape(box(left, top, right, bottom))
    paint(image, DEEP, inside, SOLID)
    paint(image, BAND, inside, 255 if frame else 150)
    line = picture.shape(path([(left, bottom - 0.5), (right, bottom - 0.5)]), outline=1.0)
    paint(image, BRIGHT if frame else LINE, line)
    return image


def strip(picture):
    """What a whole screen stands on: navy between two bands of blue, dark
    beyond them. The game repeats the strip across the screen, so a band
    here has no step; the screen's header draws one."""
    image = picture.blank()
    width = picture.size[0]
    paint(image, DEEP, picture.shape(box(0, 0, width, SCREEN_HEIGHT)), SOLID)
    middle = picture.shape(box(0, STRIP_BAND_TOP, width, STRIP_BAND_BOTTOM))
    paint(image, NAVY, middle, ramp(image.size, 255, 150))
    for y in (STRIP_BAND_TOP, STRIP_BAND_BOTTOM):
        paint(image, BAND, picture.shape(box(0, y, width, y + BAND_HEIGHT)))
        paint(image, LINE, picture.shape(path([(0, y + 0.5), (width, y + 0.5)]), outline=1.0))
    return image


def header(picture):
    """A screen's heading: its letters bright, with their glow, over a line
    that steps up past them and fades to the right."""
    image = picture.blank()
    # the letters are the picture's brightest green; the glow around them is not
    green = picture.pixels[:, :, 1].astype(np.float32) * picture.pixels[:, :, 3] / 255.0
    brightest = float(np.percentile(green[green > 0], 99))
    letters = np.clip((green - 0.6 * brightest) / (0.3 * brightest), 0.0, 1.0)
    mask = Image.fromarray((letters * 255).astype(np.uint8), "L")
    paint(image, LINE, glow(mask, 1.5, picture.scale), 200)
    paint(image, BRIGHT, mask)

    rows, columns = np.nonzero(letters > 0.5)
    left, right = columns.min() / picture.scale, (columns.max() + 1) / picture.scale
    under = min((rows.max() + 1) / picture.scale + 5, picture.size[1] - 2)
    step = shell_art.BAND_STEP
    rule = [(left, under), (right + 10, under), (right + 10 + step, under - step), (picture.size[0], under - step)]
    fade = Image.new("L", image.size, 0)
    start = round(left * picture.scale)
    fade.paste(ramp((image.size[0] - start, image.size[1]), 255, 0, across=True), (start, 0))
    paint(image, LINE, picture.shape(path(rule), outline=1.4), fade)
    return image


def arrow(picture):
    """An arrow, its blue made the look's bright (the red ones stay red)."""
    pixels = picture.pixels.copy()
    blue = pixels[:, :, 2] > pixels[:, :, 0]
    pixels[blue, 0:3] = BRIGHT
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
