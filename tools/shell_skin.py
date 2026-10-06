#!/usr/bin/env python3
"""Writes the menus' themes (display.theme) as layers over the PC version's
screens in port/assets/menus/ce:

    python tools/shell_skin.py [--maps FOLDER]

A layer holds files under the same paths as the ones they replace while its
theme is chosen; the game reads a layer's file in place of the original
(port/linux/src/menu_files.c), and ce/ itself is never changed. Run this
again whenever ce/ changes (tools/ce_menus.py).

skin/glassed  This client's look. The PC screens are built from a few shared
              pictures (option bars, list rows, panes, the strip behind a
              screen, headers); each is redrawn in glass where the original
              has its shape (shell_art.py draws the pieces). The screens'
              text takes the look's colors, and the WIDGET_CHANGES tables
              below fix their layout for it.
skin/cairo    Halo 2's look: the same pictures redrawn by cairo_art.py, the
              same layout fixes, and the screens' titles moved onto header
              bands that reach the window's left edge. Its main menu is the
              Glassed one (shell/) set out down the middle of the screen
              (the CAIRO_SHELL tables).
skin/vanilla  The stock main menu with a MENUS button, which opens the choice
              of theme. (The other themes' MENUS is in shell/main.xml.)

With --maps (a folder of the Xbox maps), each look's xbox/ is written too:
the menu pictures the maps carry themselves (pause menus, split screen and
lobby screens), drawn in place of the maps' bitmaps as the high-res HUD is
(port/linux/src/hud_hires.c), listed in its textures.json for
tools/embed_assets.py. The maps', levels' and game types' pictures are
enlarged for every theme, in skin/xbox. (The xbox/ folders are made from
the maps' own pictures, so they are not kept in the repository.) Needs
Pillow, NumPy and SciPy.
"""

import argparse
import copy
import json
import shutil
import xml.etree.ElementTree as ET
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

import numpy as np
from scipy import ndimage
from PIL import Image, ImageFilter

import cairo_art
import port_settings
import shell_art
from hud_assets import FORMATS, XboxMap, decode_bitmap, level0_size
from menu_drawing import box, paint, polygon, ramp
from shell_art import SHADE, WHITE

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
SKIN = MENUS / "skin"
VANILLA = SKIN / "vanilla"
LOBBY_FILE = "main_menu.multiplayer_type_select.lobby.xml"

# ---------- the pictures redrawn

VISIBLE = 8                 # alpha above which a texel is part of a picture's shape

# the PC screens' pictures redrawn, by the last part of their names
PANES = ("alert_bkd", "list_field", "sel_list_desc_bkd", "spinner_list_3_wide_item_background",
         "controller_border", "device_border", "join_game_border")
BARS = ("option_bkds", "option_bkds_sm", "option_bkds_sm2", "text_button_background", "list_item_bkd",
        "list_item_bkd_top", "sel_list_item_bkd", "sel_list_item_bkd_top", "server_item_bkds")
TABS = ("server_header_background_0", "server_header_background_1", "server_header_background_2",
        "server_header_background_3")
STRIPS = ("gradient", "gradient_big")
ARROWS = ("arrow_sm_left", "arrow_sm_right", "arrow_sm_up", "arrow_sm_down")

# the Xbox maps' pictures redrawn besides those: their panes, bars, arrows,
# and three-part boxes (the pause menus': a left end, a middle the game
# stretches, a right end)
XBOX_PANES = ("split_screen_bkd", "4way_profile_border", "pregame_field", "pregame_list_field",
              "pregame_remote_field")
XBOX_BARS = ("menu_bkds", "pregame_message_field", "pregame_message_field_split")
XBOX_BOXES = ("pausebox", "pausebox2", "helpbox")
XBOX_ARROWS = ("arrow_big_left", "arrow_big_right")
XBOX_SCALE = 4              # a redrawn picture is this many times its bitmap's size,
XBOX_LARGEST = 2048         # or fewer, to be at most this many texels
# the pictures enlarged rather than redrawn, in every theme (there are no
# larger originals)
PICTURES = ("mp_map_grafix", "sp_levels", "game_type_grafix")
PICTURE_SCALE = 3

# the version number's color in every theme, the teal of the server browser's
# notices (port/linux/game/browser_screen.c, COLOR_NOTICE)
VERSION_COLOR = "#FF3CC8C0"

# ---------- the layout fixes the looks share

# The selection lists' rows (eight screens share them: profiles, saved
# games, levels, maps, the lobby, game types, playlists, colors) are this
# wide from where they begin, clear of the description pane at x 400.
SELECTION_ROW_WIDTH = 360
SELECTION_ROWS = tuple(f"main_menu/new_select/list_item_{index}" for index in range(11))
SELECTION_BARS = ("sel_list_item_bkd", "sel_list_item_bkd_top")

# Layout changes to the screens' widgets, by widget name: attributes to set
# (None: to drop), or None for a widget whose children go (decoration the
# looks have no use for).
WIDGET_CHANGES = {
    **{name: {"width": str(SELECTION_ROW_WIDTH)} for name in SELECTION_ROWS},
    "main_menu/new_select/list_item_text": {"width": str(SELECTION_ROW_WIDTH - 24), "align": "left"},
    "main_menu/new_select/list_item_arrows": None,
    # (the scroll buttons: an arrow in the middle of the row, on no bar of their own)
    "main_menu/new_select/scroll_up_button": {"width": str(SELECTION_ROW_WIDTH - 40), "bitmap": None},
    "main_menu/new_select/scroll_down_button": {"width": str(SELECTION_ROW_WIDTH - 40), "bitmap": None},
    "main_menu/build_number": {"color": VERSION_COLOR},
}
# where a widget's children are: (parent, child) -> attributes
CHILD_CHANGES = {
    **{(name, "main_menu/new_select/list_item_text"): {"x": "12"} for name in SELECTION_ROWS},
    ("main_menu/new_select/scroll_up_button", "main_menu/new_select/scroll_up_arrow"): {"x": "152"},
    ("main_menu/new_select/scroll_down_button", "main_menu/new_select/scroll_down_arrow"): {"x": "152"},
}
# extra event handlers: the Map screen opens the map picker
# (port/linux/game/map_screen.c) over its own list
HANDLER_ADDITIONS = {
    "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_screen": [{"event": "created", "run": "port map select"}],
}
# Vanilla's: B on the Map screen's list steps back through it (menu_functions.c)
VANILLA_HANDLER_ADDITIONS = {
    "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_list_2": [
        {"event": "b", "run": "port map list back"}, {"event": "back", "run": "port map list back"}],
}

# ---------- the Glassed look

HAIRLINE = 0.8              # the width of an edge, in the menus' units
STRIP_TOP, STRIP_BOTTOM = 66, 446   # a screen's glass: under its header, over its foot
GREEN = (150, 255, 180)     # Online Games' third row frame: a game to join


class Picture:
    """One frame of a bitmap: its pixels (from its PNG, or given), its size
    in the 640x480 screen, and the box its shape fills (in units of that
    screen), if it has one."""

    def __init__(self, png, width, pixels=None):
        self.png = png
        self.pixels = np.array(Image.open(MENUS / png).convert("RGBA")) if pixels is None else pixels
        self.scale = self.pixels.shape[1] // width
        # (the size the picture has, which may be less than the frame's)
        self.size = (self.pixels.shape[1] // self.scale, self.pixels.shape[0] // self.scale)
        rows, columns = np.nonzero(self.pixels[:, :, 3] > VISIBLE)
        self.box = None
        if len(rows):
            self.box = (round(columns.min() / self.scale), round(rows.min() / self.scale),
                        round((columns.max() + 1) / self.scale), round((rows.max() + 1) / self.scale))

    def blank(self):
        return Image.new("RGBA", (self.pixels.shape[1], self.pixels.shape[0]), (0, 0, 0, 0))

    def shape(self, points, outline=0.0, closed=True):
        return polygon(self.size, points, outline, self.scale, closed)


def pane(picture, frame):
    """A pane of dark glass in a hairline; frame 1 has the focus."""
    image = picture.blank()
    shape = box(*picture.box)
    paint(image, SHADE, picture.shape(shape), 120)
    paint(image, WHITE, picture.shape(shape, outline=HAIRLINE), 170 if frame else 70)
    return image


def bar(picture, frame, joinable=False, selection_width=None):
    """What an option or a list item stands on: the faintest glass, or with
    the focus (frame 1) the left-hand menus' strip. A third frame is the
    strip too; Online Games' (joinable) is green. A selection list's row
    (selection_width) shows nothing until chosen, then the strip that wide."""
    image = picture.blank()
    area = picture.box
    if selection_width:
        if frame == 0:
            return image
        area = (0, area[1], min(selection_width, picture.size[0]), area[3])
    if frame == 0:
        paint(image, WHITE, picture.shape(box(*area)), ramp(image.size, 16, 0, across=True))
        return image
    shell_art.bar(image, picture.size, area, True, picture.scale, GREEN if joinable else WHITE)
    return image


def tab(picture, frame):
    """The heading of a column of a game list: glass over a hairline."""
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


def header(picture, letters):
    """A screen's heading: its letters alone, in clear white."""
    image = picture.blank()
    paint(image, WHITE, letters, 215)
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


# ---------- the looks

@dataclass(frozen=True)
class Look:
    """A theme's look: how it redraws each kind of picture, the colors the
    screens' text takes, where the screens' titles go (header_left: the x
    their widgets move to, or None to stay), and whether a row stands on a
    bar when it isn't chosen though the original shows none there
    (solid_rows: it takes the shape of the bitmap's other frames)."""
    name: str
    pane: Callable
    bar: Callable
    tab: Callable
    strip: Callable
    header: Callable
    arrow: Callable
    three_part_box: Callable
    text_colors: dict = field(default_factory=dict)
    header_left: Optional[int] = None
    solid_rows: bool = False

    @property
    def folder(self):
        return SKIN / self.name


GLASSED = Look("glassed", pane, bar, tab, strip, header, arrow, three_part_box,
               text_colors={"#FF2896FF": "#FFD2D6DA", "#FF0080FF": "#FFA8ACB0"})
CAIRO = Look("cairo", cairo_art.pane, cairo_art.bar, cairo_art.tab, cairo_art.strip, cairo_art.header,
             cairo_art.arrow, cairo_art.three_part_box,
             text_colors={"#FF2896FF": cairo_art.TEXT, "#FF0080FF": cairo_art.TEXT_DIM},
             header_left=-cairo_art.HEADER_LEFT_OF_SCREEN, solid_rows=True)
LOOKS = (GLASSED, CAIRO)


def header_letters(picture):
    """A screen's heading's letters as a mask: the picture's brightest green
    (the glow around them is not)."""
    green = picture.pixels[:, :, 1].astype(np.float32) * picture.pixels[:, :, 3] / 255.0
    brightest = float(np.percentile(green[green > 0], 99))
    letters = np.clip((green - 0.6 * brightest) / (0.3 * brightest), 0.0, 1.0)
    return Image.fromarray((letters * 255).astype(np.uint8), "L")


def redrawn(look, name, picture, frame):
    """A bitmap's frame redrawn in the look, or None to keep it as it is."""
    last = name.rsplit("/", 1)[-1]
    if last in ARROWS + XBOX_ARROWS:
        return look.arrow(picture)
    if picture.box is None:
        return None
    for box_name in XBOX_BOXES:
        for part in ("left", "center", "right"):
            if last == f"{box_name}_{part}":
                return look.three_part_box(picture, part)
    if last in PANES + XBOX_PANES:
        return look.pane(picture, frame)
    if last in BARS + XBOX_BARS:
        return look.bar(picture, frame, joinable=frame == 2 and last == "server_item_bkds",
                        selection_width=SELECTION_ROW_WIDTH if last in SELECTION_BARS else None)
    if last in TABS:
        return look.tab(picture, frame)
    if last in STRIPS:
        return look.strip(picture)
    if last.startswith("header_"):
        return look.header(picture, header_letters(picture))
    return None


def redrawn_frames(look, name, frames):
    """A bitmap's frames, (frame number, Picture) pairs, redrawn in the look:
    (frame number, image, or None to keep it) pairs. In a look with solid
    rows, a row's frame with next to nothing in it (a few stray texels) takes
    the shape of the largest."""
    def area(box):
        return (box[2] - box[0]) * (box[3] - box[1]) if box else 0

    shape = max((picture.box for _, picture in frames), key=area, default=None)
    for frame, picture in frames:
        if (look.solid_rows and name.rsplit("/", 1)[-1] in BARS + XBOX_BARS
                and area(picture.box) < area(shape) / 4):
            picture = copy.copy(picture)
            picture.box = shape
        yield frame, redrawn(look, name, picture, frame)


def enlarged(pixels, scale):
    """A picture enlarged smoothly, then sharpened a little against the blur."""
    image = Image.fromarray(pixels, "RGBA")
    image = image.resize((image.width * scale, image.height * scale), Image.Resampling.LANCZOS)
    return image.filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))


# ---------- the screens restyled

def add_handlers(widget, table=HANDLER_ADDITIONS):
    """Appends the widget's additions in the table after its own handlers. Returns True if it had any."""
    additions = table.get(widget.get("name"), [])
    for attributes in additions:
        handlers = widget.findall("on")
        at = list(widget).index(handlers[-1]) + 1 if handlers else 0
        widget.insert(at, ET.Element("on", attributes))
    return bool(additions)


def change_attributes(element, changes):
    for attribute, value in changes.items():
        if value is None:
            element.attrib.pop(attribute, None)
        else:
            element.set(attribute, value)


def restyle(menus, look, frame_widths):
    """Gives a file's widgets the look's text colors and layout; whether any
    changed. frame_widths: each bitmap's frames' width, by its name."""
    changed = False
    for widget in menus.iter("widget"):
        name = widget.get("name")
        if widget.get("color") in look.text_colors:
            widget.set("color", look.text_colors[widget.get("color")])
            changed = True
        if name in WIDGET_CHANGES:
            if WIDGET_CHANGES[name] is None:
                for child in widget.findall("child"):
                    widget.remove(child)
            else:
                change_attributes(widget, WIDGET_CHANGES[name])
            changed = True
        bitmap = widget.get("bitmap") or ""
        if look.header_left is not None and bitmap.rsplit("/", 1)[-1].startswith("header_"):
            # (as wide as its picture, which draws the band from the window's edge)
            change_attributes(widget, {"left": str(look.header_left), "width": str(frame_widths[bitmap])})
            changed = True
        changed |= add_handlers(widget)
        for child in widget.findall("child"):
            for attribute, value in CHILD_CHANGES.get((name, child.get("widget")), {}).items():
                child.set(attribute, value)
                changed = True
    return changed


def write_xml(tree, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(tree, space="\t")
    tree.write(path, encoding="UTF-8", xml_declaration=True)


def look_layer(look):
    """skin/<look>/ce: the PC screens' pictures redrawn, and the screens restyled."""
    if (look.folder / "ce").exists():
        shutil.rmtree(look.folder / "ce")
    pictures = 0
    frame_widths = {}
    for bitmap in ET.parse(MENUS / "ce" / "bitmaps.xml").getroot().iter("bitmap"):
        name = bitmap.get("name")
        elements = list(bitmap.iter("frame"))
        if elements:
            frame_widths[name] = int(elements[0].get("width", 0))
        frames = [(frame, Picture(element.get("png"), int(element.get("width"))))
                  for frame, element in enumerate(elements) if element.get("png")]
        for (frame, image), (_, picture) in zip(redrawn_frames(look, name, frames), frames):
            if image is not None:
                (look.folder / picture.png).parent.mkdir(parents=True, exist_ok=True)
                image.save(look.folder / picture.png)
                pictures += 1
    screens = 0
    for file in sorted((MENUS / "ce").glob("*.xml")):
        if file.name == LOBBY_FILE:
            # the lobby is the overlay's form (port_settings.py), not the stock one restyled
            tree = ET.ElementTree(ET.fromstring("\n".join(port_settings.glassed_lobby_file()).encode("utf-8")))
        else:
            tree = ET.parse(file)
        if restyle(tree.getroot(), look, frame_widths) or file.name == LOBBY_FILE:
            write_xml(tree, look.folder / "ce" / file.name)
            screens += 1
    print(f"{look.name}: {pictures} pictures, {screens} screens")


# ---------- the Cairo theme's main menu: the Glassed one (shell/) set out
# down the middle of the screen

CAIRO_SHELL_BITMAPS = {"shell/panel": cairo_art.BACKDROP, "shell/title": cairo_art.TITLE}
CAIRO_SHELL_TITLES = ("shell/subtitle", "shell/campaign_title", "shell/multiplayer_title", "shell/themes_title")
CAIRO_SHELL_WIDGETS = {
    "shell/panel": {"width": str(cairo_art.BACKDROP[0])},
    "shell/title": {"width": str(cairo_art.TITLE[0]), "height": str(cairo_art.TITLE[1])},
    **{name: {"width": "300", "align": "center", "color": cairo_art.TEXT_DIM} for name in CAIRO_SHELL_TITLES},
}
CAIRO_SHELL_ITEM = {"text_x": "0", "align": "center", "color": cairo_art.TEXT}
# where each screen's parts go, and the lists' items: across, and how much lower
CAIRO_SHELL_PLACES = {
    "shell/panel": (-cairo_art.HEADER_LEFT_OF_SCREEN, 0),
    "shell/title": ((640 - cairo_art.TITLE[0]) // 2, 112),
    **{name: (170, 176) for name in CAIRO_SHELL_TITLES},
    "shell/rule": ((640 - cairo_art.RULE[0]) // 2, 436),
}
CAIRO_SHELL_ITEM_X, CAIRO_SHELL_ITEM_LOWER = 170, 24


def cairo_shell():
    """skin/cairo/shell: the main menu's files, from shell/'s."""
    out = CAIRO.folder / "shell"
    tree = ET.parse(MENUS / "shell" / "bitmaps.xml")
    for bitmap in tree.getroot().iter("bitmap"):
        if bitmap.get("name") in CAIRO_SHELL_BITMAPS:
            width, height = CAIRO_SHELL_BITMAPS[bitmap.get("name")]
            for frame in bitmap.iter("frame"):
                change_attributes(frame, {"width": str(width), "height": str(height)})
    write_xml(tree, out / "bitmaps.xml")

    tree = ET.parse(MENUS / "shell" / "main.xml")
    for widget in tree.getroot().iter("widget"):
        name = widget.get("name")
        if name in CAIRO_SHELL_WIDGETS:
            change_attributes(widget, CAIRO_SHELL_WIDGETS[name])
        elif name.startswith("shell/item_"):
            change_attributes(widget, CAIRO_SHELL_ITEM)
        for child in widget.findall("child"):
            if child.get("widget") in CAIRO_SHELL_PLACES:
                x, y = CAIRO_SHELL_PLACES[child.get("widget")]
                change_attributes(child, {"x": str(x), "y": str(y)})
            elif child.get("widget").startswith("shell/item_"):
                change_attributes(child, {"x": str(CAIRO_SHELL_ITEM_X),
                                          "y": str(int(child.get("y")) + CAIRO_SHELL_ITEM_LOWER)})
    write_xml(tree, out / "main.xml")
    print("cairo: the main menu")


# ---------- the Vanilla layer

# The MENUS item, under the stock main menu's QUIT, and the screen it opens,
# whose theme items are set out as the main menu's. Their pictures are drawn
# as the stock items' are (stock_item_picture).
VANILLA_ITEM_X, VANILLA_ITEM_TOP, VANILLA_ITEM_SPACING = 192, 247, 36
VANILLA_MENUS_ITEM = """
<widget name="main_menu/main_menu_item_menus" type="text" width="256" height="33" bitmap="main_menu/menu_menus">
 <on event="a start" open="main_menu/themes_screen"/>
 <on event="left_mouse" run="mouse emit accept event"/>
</widget>"""
VANILLA_THEMES = ("glassed", "cairo", "vanilla")
VANILLA_THEME_ITEM = """
<widget name="main_menu/theme_{theme}" type="text" width="256" height="33" bitmap="main_menu/menu_{theme}">
 <on event="a start" run="port theme {theme}"/>
 <on event="left_mouse" run="mouse emit accept event"/>
</widget>"""
VANILLA_THEMES_SCREEN = """
<menus>
<widget name="main_menu/themes_list" type="column_list" width="640" height="480"
 flags="pass_unhandled_to_focused_child up_down_tabs_items" description="main_menu/main_menu_list_ext_desc">
 <data input="main menu fake animate"/>
</widget>
<widget name="main_menu/themes_screen" width="640" height="480" flags="pass_unhandled_to_focused_child">
 <on event="b back" back="true"/>
 <child widget="main_menu/halo_logo" y="28"/>
 <child widget="main_menu/themes_list"/>
</widget>
</menus>"""
VANILLA_ITEM_PICTURES = {"main_menu/menu_menus": "MENUS", **{f"main_menu/menu_{theme}": theme.upper()
                                                              for theme in VANILLA_THEMES}}
# the stock items' pictures (1024x256, the text centred on x 520, its capitals
# 27 to 103 high), whose glow is fitted from SETTINGS's
STOCK_ITEM = MENUS / "ce" / "shell" / "main_menu"
STOCK_ITEM_CENTRE, STOCK_ITEM_CAP_TOP, STOCK_ITEM_CAP_HEIGHT = 520, 27, 77
STOCK_ITEM_BLUE = (39, 148, 255, 119)
STOCK_ITEM_GLOW = (38, 149, 255)
TITLE_FONT = MENUS.parent / "fonts" / "OpenCE-Regular.ttf"


def stock_item_glow():
    """The stock items' glow when selected, fitted to SETTINGS's: a blur of
    the letters (its sigma) times a gain, clipped. Returns (sigma, gain)."""
    image = np.asarray(Image.open(STOCK_ITEM / "menu_settings__1.png").convert("RGBA")).astype(float) / 255
    letters = (image[..., 3] > 0.99) & (image[..., :3].min(axis=-1) > 0.99)
    around = ~ndimage.binary_dilation(letters, iterations=2) & (image[..., 3] > 0)
    best = None
    for sigma in np.arange(2.0, 30.0, 0.5):
        blur = ndimage.gaussian_filter(letters.astype(float), sigma)[around]
        gain = float((blur * image[..., 3][around]).sum() / max((blur * blur).sum(), 1e-9))
        error = float(((np.minimum(1, gain * blur) - image[..., 3][around]) ** 2).sum())
        if best is None or error < best[0]:
            best = (error, sigma, gain)
    return best[1], best[2]


def stock_item_picture(text, glow):
    """A main menu item's two pictures as the stock items are drawn: the text
    in translucent blue, and selected, white with a blue glow"""
    from PIL import ImageDraw, ImageFont
    probe = ImageFont.truetype(str(TITLE_FONT), 1000)
    left, top, right, bottom = probe.getbbox("H")
    font = ImageFont.truetype(str(TITLE_FONT), round(STOCK_ITEM_CAP_HEIGHT * 1000 / (bottom - top)))
    origin = (STOCK_ITEM_CENTRE - font.getlength(text) / 2, STOCK_ITEM_CAP_TOP - font.getbbox("H")[1])
    mask = Image.new("L", (1024, 256), 0)
    ImageDraw.Draw(mask).text(origin, text, font=font, fill=255)
    letters = np.asarray(mask).astype(float) / 255
    plain = np.zeros((256, 1024, 4), np.uint8)
    plain[..., :3] = STOCK_ITEM_BLUE[:3]
    plain[..., 3] = np.round(letters * STOCK_ITEM_BLUE[3]).astype(np.uint8)
    sigma, gain = glow
    halo = np.minimum(1, gain * ndimage.gaussian_filter(letters, sigma))
    alpha = letters + halo * (1 - letters)
    selected = np.zeros((256, 1024, 4), np.uint8)
    for channel in range(3):
        colour = 255 * letters + STOCK_ITEM_GLOW[channel] * halo * (1 - letters)
        selected[..., channel] = np.round(np.divide(colour, alpha, out=np.zeros_like(colour), where=alpha > 0))
    selected[..., 3] = np.round(alpha * 255).astype(np.uint8)
    return Image.fromarray(plain, "RGBA"), Image.fromarray(selected, "RGBA")


def vanilla_layer():
    """skin/vanilla/ce: the stock main menu, this theme's main menu (root),
    with a MENUS item under QUIT and the screen it opens, drawn as the stock
    items are."""
    if VANILLA.exists():
        shutil.rmtree(VANILLA)
    tree = ET.parse(MENUS / "ce" / "main_menu.xml")
    menus = tree.getroot()
    menus.set("root", "main_menu/main_menu")
    for widget in menus.iter("widget"):
        if widget.get("name") == "main_menu/build_number":
            widget.set("color", VERSION_COLOR)
    glow = stock_item_glow()
    for name, text in VANILLA_ITEM_PICTURES.items():
        bitmap = ET.SubElement(menus, "bitmap", {"name": name})
        for index, picture in enumerate(stock_item_picture(text, glow)):
            png = f"ce/port/{name}__{index}.png"
            (VANILLA / png).parent.mkdir(parents=True, exist_ok=True)
            picture.save(VANILLA / png, optimize=True)
            ET.SubElement(bitmap, "frame", {"png": png, "width": "256", "height": "64"})
    menus.append(ET.fromstring(VANILLA_MENUS_ITEM.strip()))
    for theme in VANILLA_THEMES:
        menus.append(ET.fromstring(VANILLA_THEME_ITEM.format(theme=theme).strip()))
    for widget in ET.fromstring(VANILLA_THEMES_SCREEN.strip()):
        if widget.get("name") == "main_menu/themes_list":
            for row, theme in enumerate(VANILLA_THEMES):
                ET.SubElement(widget, "child", {"widget": f"main_menu/theme_{theme}", "x": str(VANILLA_ITEM_X),
                                                "y": str(VANILLA_ITEM_TOP + row * VANILLA_ITEM_SPACING)})
        menus.append(widget)
    rows = next(widget for widget in menus.iter("widget") if widget.get("name") == "main_menu/main_menu_select_list")
    last = max(int(child.get("y")) for child in rows.findall("child"))
    rows.append(ET.Element("child", {"widget": "main_menu/main_menu_item_menus", "x": str(VANILLA_ITEM_X),
                                     "y": str(last + VANILLA_ITEM_SPACING)}))
    write_xml(tree, VANILLA / "ce" / "main_menu.xml")
    for file in sorted((MENUS / "ce").glob("*.xml")):
        tree = ET.parse(file)
        if any([add_handlers(widget, VANILLA_HANDLER_ADDITIONS) for widget in tree.getroot().iter("widget")]):
            write_xml(tree, VANILLA / "ce" / file.name)
    print(f"vanilla: the main menu, glow sigma {glow[0]:.1f} gain {glow[1]:.2f}")


# ---------- the maps' own pictures

def maps_layer(folder):
    """The menu pictures of the Xbox maps in folder, redrawn in each look
    (skin/<look>/xbox) or enlarged for every theme (skin/xbox), each folder
    listed in its textures.json. A picture is the same in every map that has
    it; the Xbox screens' headers are also tools/title_assets.py's, which the
    game draws in the Vanilla theme."""
    outs = [SKIN / "xbox"] + [look.folder / "xbox" for look in LOOKS]
    for out in outs:
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
    assets = {out: [] for out in outs}
    done = set()
    for path in sorted(Path(folder).glob("*.map")):
        try:
            game = XboxMap(path)
        except (zlib.error, ValueError):
            continue    # (not an Xbox map: a Custom Edition one, say)
        for group, name in sorted(game.tags):
            if group != "bitm" or not name.startswith("ui\\shell"):
                continue
            every_theme = name.rsplit("\\", 1)[-1] in PICTURES
            # (each frame redrawn or enlarged, as (folder, frame number, image))
            drawn, frames, sources = [], [], {}
            for index, bitmap in enumerate(game.bitmap_group(name)["bitmaps"]):
                if (name, index) in done:
                    continue
                done.add((name, index))
                width, height = bitmap["width"], bitmap["height"]
                scale = PICTURE_SCALE if every_theme else min(XBOX_SCALE, XBOX_LARGEST // max(width, height))
                if scale < 2:
                    continue
                try:
                    pixels = decode_bitmap(bitmap)
                except ValueError:
                    continue
                sources[index] = (bitmap, scale)
                if every_theme:
                    drawn.append((SKIN / "xbox", index, enlarged(pixels, scale)))
                else:
                    blocky = np.array(Image.fromarray(pixels, "RGBA").resize((width * scale, height * scale),
                                                                             Image.Resampling.NEAREST))
                    frames.append((index, Picture(None, width, blocky)))
            for look in LOOKS:
                drawn += [(look.folder / "xbox", index, image)
                          for index, image in redrawn_frames(look, name.replace("\\", "/"), frames)]
            for out, index, image in drawn:
                if image is None:
                    continue
                bitmap, scale = sources[index]
                asset = f"{name.replace(chr(92), '__')}__{index}".replace(" ", "_")
                image.save(out / f"{asset}.png")
                assets[out].append({"name": asset, "tag": name, "bitmap": index, "width": bitmap["width"],
                                    "height": bitmap["height"], "format": FORMATS[bitmap["format"]], "scale": scale,
                                    "crc": zlib.crc32(bitmap["pixels"][:level0_size(bitmap)])})
    for out in outs:
        (out / "textures.json").write_text(json.dumps({"assets": assets[out]}, indent=1) + "\n")
        print(f"{out.relative_to(MENUS).as_posix()}: {len(assets[out])} of the maps' pictures")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--maps", help="a folder of the Xbox maps, to redraw their own menu pictures too")
    arguments = parser.parse_args()
    for look in LOOKS:
        look_layer(look)
    cairo_shell()
    vanilla_layer()
    if arguments.maps:
        maps_layer(arguments.maps)


if __name__ == "__main__":
    main()
