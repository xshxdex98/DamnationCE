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

from shell_art import AMBER, GLASS, LIGHT, SHADE, cut_corner_box, paint, polygon, ramp

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
SKIN = MENUS / "skin"

GREEN = (120, 230, 150)     # the game list's third frame: a game to join
VISIBLE = 8                 # alpha above which a texel is part of a picture's shape

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
TEXT_COLORS = {"#FF2896FF": "#FFD6ECFF", "#FF0080FF": "#FF84B8E0"}


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


def scanlines(size, scale):
    """Fine lines across a bar, one every two units: a mask to darken it by."""
    lines = np.zeros((size[1], size[0]), dtype=np.uint8)
    lines[np.arange(size[1]) // scale % 2 == 1] = 255
    return Image.fromarray(lines, "L")


def pane(picture, frame):
    """A pane of glass with its top right corner cut; frame 1 has the focus."""
    image = picture.blank()
    size = image.size
    left, top, right, bottom = picture.box
    cut = min(20, (right - left) // 6, (bottom - top) // 6)
    shape = cut_corner_box(left, top, right, bottom, cut, "top right")
    inside = picture.shape(shape)
    paint(image, SHADE, inside, ramp(size, 150, 185))
    paint(image, GLASS, inside, ramp(size, 70 if frame else 50, 14))
    paint(image, LIGHT, picture.shape(shape, outline=1.0), ramp(size, 230 if frame else 130, 40))
    top_edge = [(left, top), (right - cut, top), (right, top + cut)]
    paint(image, LIGHT, picture.shape(top_edge + top_edge[-2::-1], outline=1.4))
    return image


def bar(picture, frame, name):
    """A bar behind an option or a list item, its bottom right corner cut:
    faint, or lit behind an amber tick with the focus (frame 1). A third
    frame is lit without the tick; the game list's is green."""
    image = picture.blank()
    size = image.size
    left, top, right, bottom = picture.box
    cut = min(9, (bottom - top) // 3)
    shape = cut_corner_box(left, top, right, bottom, cut, "bottom right")
    inside = picture.shape(shape)
    tint = GREEN if frame == 2 and name == "server_item_bkds" else GLASS
    paint(image, SHADE, inside, ramp(size, 110, 60, across=True))
    if frame == 0:
        paint(image, tint, inside, ramp(size, 44, 14, across=True))
    else:
        paint(image, tint, inside, ramp(size, 150, 50, across=True))
        paint(image, LIGHT, picture.shape(shape, outline=1.0), ramp(size, 170, 40, across=True))
        edge = [(left, top + 0.5), (right, top + 0.5)]
        paint(image, LIGHT, picture.shape(edge, outline=1.0), ramp(size, 255, 60, across=True))
    paint(image, SHADE, inside, scanlines(size, picture.scale).point(lambda value: value * 28 // 255))
    if frame == 1:
        paint(image, AMBER, picture.shape([(left, top), (left + 3, top), (left + 3, bottom), (left, bottom)]))
    return image


def tab(picture, frame):
    """The heading of a column of the game list: flat glass over a line."""
    image = picture.blank()
    size = image.size
    left, top, right, bottom = picture.box
    inside = picture.shape([(left, top), (right, top), (right, bottom), (left, bottom)])
    paint(image, SHADE, inside, ramp(size, 150, 150))
    paint(image, GLASS, inside, ramp(size, 120 if frame else 50, 30 if frame else 14))
    line = [(left, bottom - 0.5), (right, bottom - 0.5)]
    strength = 255 if frame else 140
    paint(image, AMBER if frame else LIGHT, picture.shape(line, outline=1.0), ramp(size, strength, strength))
    return image


def strip(picture):
    """What a whole screen stands on: dark glass between two lines of light,
    where the strip it replaces is. The game repeats it across the screen."""
    image = picture.blank()
    size = image.size
    left, top, right, bottom = 0, picture.box[1], picture.size[0], picture.box[3]
    inside = picture.shape([(left, top), (right, top), (right, bottom), (left, bottom)])
    paint(image, SHADE, inside, ramp(size, 205, 225))
    sheen = Image.new("L", size, 0)
    sheen.paste(ramp((size[0], 70 * picture.scale), 40, 0), (0, top * picture.scale))
    paint(image, GLASS, inside, sheen)
    for y, strength in ((top + 0.5, 235), (bottom - 0.5, 110)):
        paint(image, LIGHT, picture.shape([(left, y), (right, y)], outline=1.0), ramp(size, strength, strength))
    return image


def header(picture):
    """A screen's heading: its letters in the look's light, over a rule that
    steps up past them and fades to the right."""
    image = picture.blank()
    size = image.size
    # the letters are the picture's brightest green; the glow around them is not
    green = picture.pixels[:, :, 1].astype(np.float32) * picture.pixels[:, :, 3] / 255.0
    brightest = float(np.percentile(green[green > 0], 99))
    letters = np.clip((green - 0.6 * brightest) / (0.3 * brightest), 0.0, 1.0)
    paint(image, LIGHT, Image.fromarray((letters * 255).astype(np.uint8), "L"))

    rows, columns = np.nonzero(letters > 0.5)
    left, right = columns.min() / picture.scale, (columns.max() + 1) / picture.scale
    under = min((rows.max() + 1) / picture.scale + 5, picture.size[1] - 2)
    step = 7
    rule = [(left, under), (right + 10, under), (right + 10 + step, under - step), (picture.size[0], under - step)]
    fade = Image.new("L", size, 0)
    fade.paste(ramp((size[0] - round(left * picture.scale), size[1]), 235, 0, across=True),
               (round(left * picture.scale), 0))
    paint(image, LIGHT, picture.shape(rule + rule[-2:0:-1], outline=1.2), fade)
    paint(image, AMBER, picture.shape([(left, under - 1), (left + 14, under - 1), (left + 14, under + 1),
                                       (left, under + 1)]))
    return image


def arrow(picture):
    """An arrow, its blue made the look's light (the red ones stay red)."""
    pixels = picture.pixels.copy()
    blue = pixels[:, :, 2] > pixels[:, :, 0]
    pixels[blue, 0:3] = LIGHT
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
