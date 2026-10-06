#!/usr/bin/env python3
"""Draws a screen of the menus (port/assets/menus) roughly as the game does,
to look a layout over without running the game:

    python tools/menu_preview.py <widget name> <out.png> [focused child index]
        [--theme glassed|vanilla|cairo] [--backdrop <picture of the scene>]

Every widget's picture is drawn where the game draws it (its first frame;
its second for the list item with the focus), and its text in a stand-in
font, with "Text" where the game fills the text in. Boxes outline widgets
with neither, so overlaps show. A theme's layer (skin/<theme>/) is read in
place of the files it replaces, as the game reads it. Needs Pillow.
"""

import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
SCALE = 2
WIDTH, HEIGHT = 854, 480            # a 16:9 window, in the menus' units
LEFT = (WIDTH - 640) // 2           # where the 640 columns begin
FONT_SIZES = {"ui\\large_ui": 14, "ui\\small_ui": 10}
BACKDROP = (58, 50, 52, 255)


THEME = "glassed"


def menu_file(name):
    """A file of the menus: the theme's copy, if its layer has one."""
    layered = MENUS / "skin" / THEME / name
    return layered if layered.exists() else MENUS / name


class Menus:
    def __init__(self):
        self.widgets, self.bitmaps, self.strings = {}, {}, {}
        for name in json.loads((MENUS / "menus.json").read_text())["files"]:
            if not name.endswith(".xml") or name.startswith("skin/") or (THEME == "vanilla" and name.startswith("shell/")):
                continue
            for element in ET.parse(menu_file(name)).getroot():
                if element.tag == "widget":
                    self.widgets[element.get("name")] = element
                elif element.tag == "bitmap":
                    self.bitmaps[element.get("name")] = list(element.iter("frame"))
                elif element.tag == "strings":
                    self.strings[element.get("name")] = [s.get("text", "") for s in element.iter("string")]


def bounds(widget):
    """(left, top, width, height) of a widget, before its parent's offset."""
    if widget.get("bounds"):
        top, left, bottom, right = (int(value) for value in widget.get("bounds").split())
        return left, top, right - left, bottom - top
    return (int(widget.get("left", 0)), int(widget.get("top", 0)),
            int(widget.get("width", 0)), int(widget.get("height", 0)))


def color_of(text):
    value = int(text.lstrip("#"), 16)
    alpha = value >> 24 if len(text.lstrip("#")) == 8 else 255
    return (value >> 16 & 255, value >> 8 & 255, value & 255, alpha)


def draw_widget(menus, image, name, x, y, focused, focus_index, depth=0):
    widget = menus.widgets.get(name)
    if widget is None:      # a tag of the map
        return
    left, top, width, height = bounds(widget)
    x, y = x + left, y + top
    canvas = ImageDraw.Draw(image, "RGBA")
    frames = menus.bitmaps.get(widget.get("bitmap"))
    drawn = False
    if frames:
        frame = frames[1 if focused and len(frames) > 1 else 0]
        if frame.get("png"):
            picture = Image.open(menu_file(frame.get("png"))).convert("RGBA")
            w, h = int(frame.get("width")), int(frame.get("height"))
            picture = picture.resize((w * SCALE, h * SCALE), Image.Resampling.LANCZOS)
            if w <= 16 and width >= 640:     # a strip: repeated across the window
                for column in range(0, WIDTH, w):
                    image.alpha_composite(picture.crop((0, 0, w * SCALE, min(h, height) * SCALE)), (column * SCALE, y * SCALE))
            else:
                picture = picture.crop((0, 0, min(w, width) * SCALE, min(h, height) * SCALE))
                image.alpha_composite(picture, ((LEFT + x) * SCALE, (y) * SCALE))
            drawn = True
    if widget.get("type") == "text" or widget.get("text") or widget.get("string_list"):
        words = widget.get("text")
        if words is None and widget.get("string_list") in menus.strings:
            strings = menus.strings[widget.get("string_list")]
            index = int(widget.get("string_index", 0))
            words = strings[index] if index < len(strings) else None
        words = (words or "Text").replace("\\n", "\n")
        font = ImageFont.truetype("arial.ttf", FONT_SIZES.get(widget.get("font"), 12) * SCALE)
        tx, ty = x + int(widget.get("text_x", 0)), y + int(widget.get("text_y", 0))
        fill = color_of(widget.get("color", "#FFFFFFFF"))
        if fill[3] == 0:
            fill = fill[:3] + (255,)
        text_width = canvas.textlength(words.split("\n")[0], font=font) / SCALE
        if widget.get("align") == "center":
            tx += (width - text_width) / 2
        elif widget.get("align") == "right":
            tx += width - text_width
        canvas.text(((LEFT + tx) * SCALE, ty * SCALE), words, font=font, fill=fill)
        if text_width > width > 0 and widget.get("align") != "center":
            canvas.rectangle([(LEFT + x) * SCALE, y * SCALE, (LEFT + x + width) * SCALE, (y + height) * SCALE],
                             outline=(255, 60, 60, 255))    # text wider than its widget
        drawn = True
    if not drawn and 0 < width < 640:
        canvas.rectangle([(LEFT + x) * SCALE, y * SCALE, (LEFT + x + width) * SCALE, (y + height) * SCALE],
                         outline=(255, 255, 0, 70))
    is_list = widget.get("type") == "column_list"
    for index, child in enumerate(widget.findall("child")):
        draw_widget(menus, image, child.get("widget"), x - left + int(child.get("x", 0)), y - top + int(child.get("y", 0)),
                    is_list and index == focus_index and depth < 3, focus_index, depth + 1)
    if widget.get("description"):
        draw_widget(menus, image, widget.get("description"), 0, 0, False, focus_index, depth + 1)


def main():
    global THEME
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("widget")
    parser.add_argument("out")
    parser.add_argument("focus", nargs="?", type=int, default=0, help="the focused child of the screen's list")
    parser.add_argument("--theme", default="glassed")
    parser.add_argument("--backdrop", help="a picture of the scene behind the menus")
    arguments = parser.parse_args()
    THEME = arguments.theme
    if arguments.backdrop:
        image = Image.open(arguments.backdrop).convert("RGBA").resize((WIDTH * SCALE, HEIGHT * SCALE))
    else:
        image = Image.new("RGBA", (WIDTH * SCALE, HEIGHT * SCALE), BACKDROP)
        scene = ImageDraw.Draw(image)
        scene.ellipse([300 * SCALE, 240 * SCALE, 1100 * SCALE, 1040 * SCALE], fill=(150, 120, 80, 255))
    draw_widget(Menus(), image, arguments.widget, 0, 0, False, arguments.focus)
    image.convert("RGB").save(arguments.out)


if __name__ == "__main__":
    main()
