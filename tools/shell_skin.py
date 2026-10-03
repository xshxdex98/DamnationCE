#!/usr/bin/env python3
"""Dresses the PC version's screens (port/assets/menus/ce) in this client's
look, the left-hand menus' (tools/shell_art.py):

    python tools/shell_skin.py [--maps FOLDER]

Those screens are built from a few shared pictures: the bars behind options
and list items, the panes behind lists, the strip behind a whole screen, and
each screen's header. This redraws them, each where the picture it replaces
has its shape, and gives the screens' text the look's color. Nothing in ce/
is changed: the new files go to port/assets/menus/skin, under the same
paths, and tools/embed_assets.py embeds a file there in place of the one it
shadows. Run it again after ce/ changes (tools/ce_menus.py).

With --maps (a folder of the Xbox maps), the screens the maps hold
themselves are redrawn too: the pause menus each map carries, and ui.map's
split screen and lobby screens. Those are drawn in place of the maps'
bitmaps, as the high-res HUD is (port/linux/src/hud_hires.c): skin/xbox
holds the pictures and textures.json, which tools/embed_assets.py embeds.
Needs Pillow and NumPy.
"""

import argparse
import json
import shutil
import sys
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

import shell_art
from shell_art import SHADE, WHITE, box, paint, polygon, ramp

sys.path.insert(0, str(Path(__file__).resolve().parent))
from hud_assets import FORMATS, XboxMap, decode_bitmap, level0_size  # noqa: E402

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

# the Xbox maps' pictures redrawn, by the last part of their names, besides
# those above: their panes, bars, and three-part boxes (the pause menus':
# the left end, the middle the game stretches, the right end)
XBOX_PANES = ("split_screen_bkd", "4way_profile_border", "pregame_field", "pregame_list_field",
              "pregame_remote_field")
XBOX_BARS = ("menu_bkds", "pregame_message_field", "pregame_message_field_split")
XBOX_BOXES = ("pausebox", "pausebox2", "helpbox")
XBOX_ARROWS = ("arrow_big_left", "arrow_big_right")
XBOX_SCALE = 4              # a picture is drawn this many times its bitmap's size,
XBOX_LARGEST = 2048         # or fewer, to be at most this many texels

# the selection lists' rows (eight screens share them: profiles, saved games,
# levels, maps, the lobby, game types, playlists, colors): this wide, from
# where they begin, clear of the description pane at x 400
SELECTION_ROW_WIDTH = 360
SELECTION_ROWS = tuple(f"main_menu/new_select/list_item_{index}" for index in range(11))
SELECTION_BARS = ("sel_list_item_bkd", "sel_list_item_bkd_top")

# changes to the screens' widgets: attributes to set (None: to drop), by
# widget name, and None for a widget whose children go (decoration the look
# has no use for)
WIDGET_CHANGES = {
    **{name: {"width": str(SELECTION_ROW_WIDTH)} for name in SELECTION_ROWS},
    "main_menu/new_select/list_item_text": {"width": str(SELECTION_ROW_WIDTH - 24), "align": "left"},
    "main_menu/new_select/list_item_arrows": None,
    # (the scroll buttons: an arrow in the middle of the row, on no bar of their own)
    "main_menu/new_select/scroll_up_button": {"width": str(SELECTION_ROW_WIDTH - 40), "bitmap": None},
    "main_menu/new_select/scroll_down_button": {"width": str(SELECTION_ROW_WIDTH - 40), "bitmap": None},
}
# changes to where a widget's children are: (parent, child) -> attributes
CHILD_CHANGES = {
    **{(name, "main_menu/new_select/list_item_text"): {"x": "12"} for name in SELECTION_ROWS},
    ("main_menu/new_select/scroll_up_button", "main_menu/new_select/scroll_up_arrow"): {"x": "152"},
    ("main_menu/new_select/scroll_down_button", "main_menu/new_select/scroll_down_arrow"): {"x": "152"},
}

# handlers added to the screens' widgets: the Map screen opens the map
# picker (port/linux/game/map_screen.c) over its own list
HANDLER_ADDITIONS = {
    "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_screen": [{"event": "created", "run": "port map select"}],
}

# the screens' text colors, and the look's
TEXT_COLORS = {"#FF2896FF": "#FFD2D6DA", "#FF0080FF": "#FFA8ACB0"}


class Picture:
    """One frame of a bitmap: its PNG, its size in the 640x480 screen, and
    the box its shape fills (in units of that screen), if it has one."""

    def __init__(self, png, width, height, pixels=None):
        self.png = png
        self.pixels = np.array(Image.open(MENUS / png).convert("RGBA")) if pixels is None else pixels
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
    area = picture.box
    if name in SELECTION_BARS:
        # (a selection list's row: nothing until chosen, then the strip the
        # whole width of the row)
        if frame == 0:
            return image
        area = (0, area[1], min(SELECTION_ROW_WIDTH, picture.size[0]), area[3])
    if frame == 0:
        paint(image, WHITE, picture.shape(box(*area)), ramp(image.size, 16, 0, across=True))
        return image
    tint = GREEN if frame == 2 and name == "server_item_bkds" else WHITE
    shell_art.bar(image, picture.size, area, True, picture.scale, tint)
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


def three_part_box(picture, part):
    """A part of a box the game puts together across the screen: dark glass
    between hairlines, closed by a hairline at its left and right ends."""
    image = picture.blank()
    left, top, right, bottom = picture.box
    if part != "left":
        left = 0
    if part != "right":
        right = picture.size[0]
    paint(image, SHADE, picture.shape(box(left, top, right, bottom)), 150)
    for y in (top, bottom - HAIRLINE):
        paint(image, WHITE, picture.shape(box(left, y, right, y + HAIRLINE)), 90)
    if part in ("left", "right"):
        x = left if part == "left" else right - HAIRLINE
        paint(image, WHITE, picture.shape(box(x, top, x + HAIRLINE, bottom)), 90)
    return image


def redrawn(name, picture, frame):
    """The picture of a bitmap's frame in the look, or None to keep it."""
    last = name.rsplit("/", 1)[-1]
    if last in ARROWS + XBOX_ARROWS:
        return arrow(picture)
    for box_name in XBOX_BOXES:
        for part in ("left", "center", "right"):
            if last == f"{box_name}_{part}" and picture.box is not None:
                return three_part_box(picture, part)
    if picture.box is None:
        return None
    if last in PANES + XBOX_PANES:
        return pane(picture, frame)
    if last in BARS + XBOX_BARS:
        return bar(picture, frame, last)
    if last in TABS:
        return tab(picture, frame)
    if last in STRIPS:
        return strip(picture)
    if last.startswith("header_"):
        return header(picture)
    return None


def restyle(menus):
    """Gives a file's widgets the look's text colors and layout; whether it changed any."""
    changed = False
    for widget in menus.iter("widget"):
        name = widget.get("name")
        if widget.get("color") in TEXT_COLORS:
            widget.set("color", TEXT_COLORS[widget.get("color")])
            changed = True
        if name in WIDGET_CHANGES:
            if WIDGET_CHANGES[name] is None:
                for child in widget.findall("child"):
                    widget.remove(child)
            else:
                for attribute, value in WIDGET_CHANGES[name].items():
                    if value is None:
                        widget.attrib.pop(attribute, None)
                    else:
                        widget.set(attribute, value)
            changed = True
        for attributes in HANDLER_ADDITIONS.get(name, []):
            handlers = widget.findall("on")
            at = list(widget).index(handlers[-1]) + 1 if handlers else 0
            widget.insert(at, ET.Element("on", attributes))
            changed = True
        for child in widget.findall("child"):
            for attribute, value in CHILD_CHANGES.get((name, child.get("widget")), {}).items():
                child.set(attribute, value)
                changed = True
    return changed


def skin_maps(folder):
    """Redraws the menu pictures of the Xbox maps in folder into skin/xbox, and
    lists them in its textures.json. (A picture is the same in every map
    that has it; its headers are tools/title_assets.py's.)"""
    out = SKIN / "xbox"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    assets, done = [], set()
    for path in sorted(Path(folder).glob("*.map")):
        game = XboxMap(path)
        for group, name in sorted(game.tags):
            last = name.rsplit("\\", 1)[-1]
            if group != "bitm" or not name.startswith("ui\\shell") or last.startswith("header_"):
                continue
            for index, bitmap in enumerate(game.bitmap_group(name)["bitmaps"]):
                if (name, index) in done:
                    continue
                done.add((name, index))
                width, height = bitmap["width"], bitmap["height"]
                scale = min(XBOX_SCALE, XBOX_LARGEST // max(width, height))
                if scale < 2:
                    continue
                try:
                    pixels = decode_bitmap(bitmap)
                except ValueError:
                    continue
                enlarged = np.array(Image.fromarray(pixels, "RGBA").resize((width * scale, height * scale),
                                                                           Image.Resampling.NEAREST))
                image = redrawn(name.replace("\\", "/"), Picture(None, width, height, enlarged), index)
                if image is None:
                    continue
                asset = f"{name.replace(chr(92), '__')}__{index}".replace(" ", "_")
                image.save(out / f"{asset}.png")
                assets.append({"name": asset, "tag": name, "bitmap": index, "width": width, "height": height,
                               "format": FORMATS[bitmap["format"]], "scale": scale,
                               "crc": zlib.crc32(bitmap["pixels"][:level0_size(bitmap)])})
    (out / "textures.json").write_text(json.dumps({"assets": assets}, indent=1) + "\n")
    print(f"{len(assets)} of the maps' pictures")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--maps", help="a folder of the Xbox maps, to redraw their own menus' pictures too")
    arguments = parser.parse_args()
    if (SKIN / "ce").exists():
        shutil.rmtree(SKIN / "ce")
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
        tree = ET.parse(file)
        if restyle(tree.getroot()):
            (SKIN / "ce").mkdir(parents=True, exist_ok=True)
            ET.indent(tree, space="\t")
            tree.write(SKIN / "ce" / file.name, encoding="UTF-8", xml_declaration=True)
            count += 1
    print(f"{count} screens restyled")
    if arguments.maps:
        skin_maps(arguments.maps)


if __name__ == "__main__":
    main()
