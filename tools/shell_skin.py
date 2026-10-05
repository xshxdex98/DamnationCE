#!/usr/bin/env python3
"""Writes the menus' two themes (display.theme) as layers over the PC
version's screens in port/assets/menus/ce:

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
skin/vanilla  The stock main menu with a MENUS button, which opens the choice
              of theme. (The Glassed theme's MENUS is in shell/main.xml.)

With --maps (a folder of the Xbox maps), skin/glassed/xbox is written too:
the menu pictures the maps carry themselves (pause menus, split screen and
lobby screens), drawn in place of the maps' bitmaps as the high-res HUD is
(port/linux/src/hud_hires.c), listed in its textures.json for
tools/embed_assets.py. The maps', levels' and game types' pictures are
enlarged there for both themes. Needs Pillow and NumPy.
"""

import argparse
import json
import shutil
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

import numpy as np
from scipy import ndimage
from PIL import Image, ImageFilter

import port_settings
import shell_art
from hud_assets import FORMATS, XboxMap, decode_bitmap, level0_size
from shell_art import SHADE, WHITE, box, paint, polygon, ramp

MENUS = Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"
GLASSED = MENUS / "skin" / "glassed"
VANILLA = MENUS / "skin" / "vanilla"
LOBBY_FILE = "main_menu.multiplayer_type_select.lobby.xml"
MAP_DOWNLOAD_FILE = "map_download.xml"

# ---------- the Glassed look

VISIBLE = 8                 # alpha above which a texel is part of a picture's shape
HAIRLINE = 0.8              # the width of an edge, in the menus' units
STRIP_TOP, STRIP_BOTTOM = 66, 446   # a screen's glass: under its header, over its foot
GREEN = (150, 255, 180)     # Online Games' third row frame: a game to join

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
# the pictures enlarged rather than redrawn, in both themes (there are no
# larger originals)
PICTURES = ("mp_map_grafix", "sp_levels", "game_type_grafix")
PICTURE_SCALE = 3

# the screens' text colors, and the look's
TEXT_COLORS = {"#FF2896FF": "#FFD2D6DA", "#FF0080FF": "#FFA8ACB0"}
# the version number's color in both themes, the teal of the server browser's
# notices (port/linux/game/browser_screen.c, COLOR_NOTICE)
VERSION_COLOR = "#FF3CC8C0"

# The selection lists' rows (eight screens share them: profiles, saved
# games, levels, maps, the lobby, game types, playlists, colors) are this
# wide from where they begin, clear of the description pane at x 400.
SELECTION_ROW_WIDTH = 360
SELECTION_ROWS = tuple(f"main_menu/new_select/list_item_{index}" for index in range(11))
SELECTION_BARS = ("sel_list_item_bkd", "sel_list_item_bkd_top")

# Layout changes to the screens' widgets, by widget name: attributes to set
# (None: to drop), or None for a widget whose children go (decoration the
# look has no use for).
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
# extra event handlers (Glassed): the Map screen opens the map picker
# (port/linux/game/map_screen.c) over its own list
HANDLER_ADDITIONS = {
    "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_screen": [{"event": "created", "run": "port map select"}],
}
# Vanilla's: B on the Map screen's list steps back through it (menu_functions.c)
VANILLA_HANDLER_ADDITIONS = {
    "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_list_2": [
        {"event": "b", "run": "port map list back"}, {"event": "back", "run": "port map list back"}],
}

# ---------- the Vanilla layer

# The MENUS item, under the stock main menu's QUIT, and the screen it opens,
# whose GLASSED and VANILLA items are set out as the main menu's. Their
# pictures are drawn as the stock items' are (stock_item_picture).
VANILLA_ITEM_X, VANILLA_ITEM_TOP, VANILLA_ITEM_SPACING = 192, 247, 36
VANILLA_MENUS_ITEM = """
<widget name="main_menu/main_menu_item_menus" type="text" width="256" height="33" bitmap="main_menu/menu_menus">
 <on event="a start" open="main_menu/themes_screen"/>
 <on event="left_mouse" run="mouse emit accept event"/>
</widget>"""
VANILLA_THEMES_SCREEN = f"""
<menus>
<widget name="main_menu/theme_glassed" type="text" width="256" height="33" bitmap="main_menu/menu_glassed">
 <on event="a start" run="port theme glassed"/>
 <on event="left_mouse" run="mouse emit accept event"/>
</widget>
<widget name="main_menu/theme_vanilla" type="text" width="256" height="33" bitmap="main_menu/menu_vanilla">
 <on event="a start" run="port theme vanilla"/>
 <on event="left_mouse" run="mouse emit accept event"/>
</widget>
<widget name="main_menu/themes_list" type="column_list" width="640" height="480"
 flags="pass_unhandled_to_focused_child up_down_tabs_items" description="main_menu/main_menu_list_ext_desc">
 <data input="main menu fake animate"/>
 <child widget="main_menu/theme_glassed" x="{VANILLA_ITEM_X}" y="{VANILLA_ITEM_TOP}"/>
 <child widget="main_menu/theme_vanilla" x="{VANILLA_ITEM_X}" y="{VANILLA_ITEM_TOP + VANILLA_ITEM_SPACING}"/>
</widget>
<widget name="main_menu/themes_screen" width="640" height="480" flags="pass_unhandled_to_focused_child">
 <on event="b back" back="true"/>
 <child widget="main_menu/halo_logo" y="28"/>
 <child widget="main_menu/themes_list"/>
</widget>
</menus>"""
VANILLA_ITEM_PICTURES = {"main_menu/menu_menus": "MENUS", "main_menu/menu_glassed": "GLASSED",
                         "main_menu/menu_vanilla": "VANILLA"}
# the stock items' pictures (1024x256, the text centred on x 520, its capitals
# 27 to 103 high), whose glow is fitted from SETTINGS's
STOCK_ITEM = MENUS / "ce" / "shell" / "main_menu"
STOCK_ITEM_CENTRE, STOCK_ITEM_CAP_TOP, STOCK_ITEM_CAP_HEIGHT = 520, 27, 77
STOCK_ITEM_BLUE = (39, 148, 255, 119)
STOCK_ITEM_GLOW = (38, 149, 255)
TITLE_FONT = MENUS.parent / "fonts" / "OpenCE-Regular.ttf"


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
    strip too; Online Games' is green."""
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


def header(picture):
    """A screen's heading: its letters alone, in clear white."""
    image = picture.blank()
    # (the letters are the picture's brightest green; the glow around them is not)
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
    """A bitmap's frame redrawn in the look, or None to keep it as it is."""
    last = name.rsplit("/", 1)[-1]
    if last in ARROWS + XBOX_ARROWS:
        return arrow(picture)
    if picture.box is None:
        return None
    for box_name in XBOX_BOXES:
        for part in ("left", "center", "right"):
            if last == f"{box_name}_{part}":
                return three_part_box(picture, part)
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


def enlarged(pixels, scale):
    """A picture enlarged smoothly, then sharpened a little against the blur."""
    image = Image.fromarray(pixels, "RGBA")
    image = image.resize((image.width * scale, image.height * scale), Image.Resampling.LANCZOS)
    return image.filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))


def add_handlers(widget, table=HANDLER_ADDITIONS):
    """Appends the widget's additions in the table after its own handlers. Returns True if it had any."""
    additions = table.get(widget.get("name"), [])
    for attributes in additions:
        handlers = widget.findall("on")
        at = list(widget).index(handlers[-1]) + 1 if handlers else 0
        widget.insert(at, ET.Element("on", attributes))
    return bool(additions)


def restyle(menus):
    """Gives a file's widgets the look's text colors and layout; whether any changed."""
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


def glassed_layer():
    """skin/glassed/ce: the PC screens' pictures redrawn, and the screens restyled."""
    if (GLASSED / "ce").exists():
        shutil.rmtree(GLASSED / "ce")
    pictures = 0
    for bitmap in ET.parse(MENUS / "ce" / "bitmaps.xml").getroot().iter("bitmap"):
        for frame, element in enumerate(bitmap.iter("frame")):
            if not element.get("png"):
                continue
            picture = Picture(element.get("png"), int(element.get("width")))
            image = redrawn(bitmap.get("name"), picture, frame)
            if image is not None:
                (GLASSED / picture.png).parent.mkdir(parents=True, exist_ok=True)
                image.save(GLASSED / picture.png)
                pictures += 1
    screens = 0
    for file in sorted((MENUS / "ce").glob("*.xml")):
        # Glassed's lobby and map download dialog are the overlay's forms
        # (port_settings.py), not the stock ones restyled
        if file.name == LOBBY_FILE:
            tree = ET.ElementTree(ET.fromstring("\n".join(port_settings.glassed_lobby_file()).encode("utf-8")))
        elif file.name == MAP_DOWNLOAD_FILE:
            tree = ET.ElementTree(ET.fromstring("\n".join(port_settings.glassed_map_download_file()).encode("utf-8")))
        else:
            tree = ET.parse(file)
        if restyle(tree.getroot()) or file.name in (LOBBY_FILE, MAP_DOWNLOAD_FILE):
            write_xml(tree, GLASSED / "ce" / file.name)
            screens += 1
    print(f"Glassed: {pictures} pictures, {screens} screens")


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
    for widget in ET.fromstring(VANILLA_THEMES_SCREEN.strip()):
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
    print(f"Vanilla: the main menu, glow sigma {glow[0]:.1f} gain {glow[1]:.2f}")


def maps_layer(folder):
    """skin/glassed/xbox: the menu pictures of the Xbox maps in folder
    redrawn (or enlarged), listed in textures.json. A picture is the same in
    every map that has it; the Xbox screens' headers are also
    tools/title_assets.py's, which the game draws in the Vanilla theme."""
    out = GLASSED / "xbox"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    assets, done = [], set()
    for path in sorted(Path(folder).glob("*.map")):
        game = XboxMap(path)
        for group, name in sorted(game.tags):
            if group != "bitm" or not name.startswith("ui\\shell"):
                continue
            last = name.rsplit("\\", 1)[-1]
            for index, bitmap in enumerate(game.bitmap_group(name)["bitmaps"]):
                if (name, index) in done:
                    continue
                done.add((name, index))
                width, height = bitmap["width"], bitmap["height"]
                both_themes = last in PICTURES
                scale = PICTURE_SCALE if both_themes else min(XBOX_SCALE, XBOX_LARGEST // max(width, height))
                if scale < 2:
                    continue
                try:
                    pixels = decode_bitmap(bitmap)
                except ValueError:
                    continue
                if both_themes:
                    image = enlarged(pixels, scale)
                else:
                    blocky = np.array(Image.fromarray(pixels, "RGBA").resize((width * scale, height * scale),
                                                                             Image.Resampling.NEAREST))
                    image = redrawn(name.replace("\\", "/"), Picture(None, width, blocky), index)
                if image is None:
                    continue
                asset = f"{name.replace(chr(92), '__')}__{index}".replace(" ", "_")
                image.save(out / f"{asset}.png")
                assets.append({"name": asset, "tag": name, "bitmap": index, "width": width, "height": height,
                               "format": FORMATS[bitmap["format"]], "scale": scale,
                               "crc": zlib.crc32(bitmap["pixels"][:level0_size(bitmap)]),
                               "glassed": not both_themes})
    (out / "textures.json").write_text(json.dumps({"assets": assets}, indent=1) + "\n")
    print(f"the maps: {len(assets)} pictures")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--maps", help="a folder of the Xbox maps, to redraw their own menu pictures too")
    arguments = parser.parse_args()
    glassed_layer()
    vanilla_layer()
    if arguments.maps:
        maps_layer(arguments.maps)


if __name__ == "__main__":
    main()
