/*
OVERLAY_SCREENS.C

Helpers shared by the screens drawn over the menus with the overlay
(overlay_screens.h).
*/

#ifdef HALO_GAME_BROWSER

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cutscene/cinematics.h"
#include "interface/ui_widget.h"
#include "bitmaps/bitmap_group.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"

#include "interface/ui_widget_instance.h"

#include "custom_edition_maps.h"
#include "halo_menus.h"
#include "overlay_screens.h"
#include "../src/ui_overlay.h"

#include <string.h>

/* ---------- constants */

enum
{
	/* a held direction repeats after this many milliseconds, then this often */
	REPEAT_DELAY = 350,
	REPEAT_PERIOD = 90,

	BUTTON_GAP = 8,
	BUTTON_PADDING = 10,
	BUTTON_TEXT_SIZE = 10,
};

/* The menus' level pictures: one frame per Xbox level, in the game's level
order (ui_widget_event_handler_functions.c), then one for unknown levels.
Each picture fills the top left 140x114 of its frame. */
#define LEVEL_PICTURES "ui\\shell\\bitmaps\\mp_map_grafix"
#define LEVEL_PICTURE_WIDTH 140
#define LEVEL_PICTURE_HEIGHT 114

/* the Xbox's multiplayer levels, in that order: their files and display
names */
static char const *const xbox_levels[][2] =
{
	{ "beavercreek", "Battle Creek" }, { "sidewinder", "Sidewinder" }, { "damnation", "Damnation" },
	{ "ratrace", "Rat Race" }, { "prisoner", "Prisoner" }, { "hangemhigh", "Hang 'Em High" },
	{ "chillout", "Chill Out" }, { "carousel", "Derelict" }, { "boardingaction", "Boarding Action" },
	{ "bloodgulch", "Blood Gulch" }, { "wizard", "Wizard" }, { "putput", "Chiron TL-34" }, { "longest", "Longest" },
};
#define UNKNOWN_LEVEL_FRAME NUMBEROF(xbox_levels)

static struct overlay_palette const glassed_palette =
{
	TRUE, FALSE, 0x06080C8C, 0x06080C8C, 0xFFFFFF5A, 0xFFFFFFD7, 0x06080C78, 0xFFFFFF46, 0xFFFFFF1A, 0xB4B8BCFF,
	0xFFFFFF3E, 0xFFFFFF14, 0xD2D6DAFF, 0x8C9096FF, 0xA8ACB0FF, 0xD2D6DAFF, 0x06080CE6, 0.0f,
};
static struct overlay_palette const vanilla_palette =
{
	FALSE, FALSE, 0x0B1830FF, 0x03070FFF, 0x2A62C8FF, 0x3D8BFFFF, 0x081530F0, 0x2F6DD0FF, 0x123266FF, 0x7FB0FFFF,
	0x2052B0FF, 0x16294AFF, 0xE6EEFCFF, 0x8FA6C8FF, 0x4AA3FFFF, 0x4AA3FFFF, 0x0A1A36F8, 6.0f,
};
/* (tools/cairo_art.py's palette) */
static struct overlay_palette const cairo_palette =
{
	TRUE, TRUE, 0x0A1C3AEB, 0x050D1DF2, 0x64A4E8AA, 0xFFFFFFF5, 0x0A1C3AC8, 0x64A4E8B4, 0x266CCAFF, 0x8EA4C4FF,
	0x96B9E688, 0x425C8466, 0xC8D6EAFF, 0x8EA4C4FF, 0xA8C4E8FF, 0xC8D6EAFF, 0x0A1C3AF8, 0.0f,
};

/* Cairo's frame, in the measures tools/cairo_art.py draws the menus' in
(HEADER_TOP, TOP_LINE, HEADER_TITLE_X, HEADER_TITLE_GAP...) */
enum
{
	CAIRO_HEADER_TOP = 30,
	CAIRO_TOP_LINE = 58,
	CAIRO_TITLE_X = 44,
	CAIRO_TITLE_SIZE = 20,
	CAIRO_TITLE_GAP = 26,
	CAIRO_RULER_STEP = 6,
	CAIRO_PANEL_CUT = 8,
	CAIRO_GRID = 8,
	CAIRO_BRACKET = 3,
	CAIRO_BRACKET_REACH = 30,
	CAIRO_BRACKET_SPAN = 22,
	CAIRO_BUTTON_CUT = 4,
};
#define CAIRO_BAND_TOP 0x266CCAF0
#define CAIRO_BAND_BOTTOM 0x0D3270F0
#define CAIRO_ICE 0xCEE6FFE6
#define CAIRO_TICK 0x64A4E850
#define CAIRO_GRID_LINE 0x568ED41C
#define CAIRO_BAR 0x0D3270A0
#define CAIRO_STEEL_LIGHT 0xE8F0F8FF
#define CAIRO_STEEL 0x96ACCAFF
#define CAIRO_STEEL_DARK 0x3A5074FF
#define CAIRO_ROW_EDGE 0x64A4E828
#define CAIRO_LIT_EDGE 0xFFFFFF70
#define CAIRO_LIT_GLOW 0xCEE6FF30

/* ---------- private code */

/* the Xbox level of a file's name, else NONE */
static short xbox_level(char const *file_name)
{
	short index;

	for (index = 0; index < NUMBEROF(xbox_levels); index++)
	{
		if (!csstrcmp(file_name, xbox_levels[index][0]))
			return index;
	}
	return NONE;
}

/* ---------- public code */

struct overlay_palette const *overlay_palette_current(
	void)
{
	switch (halo_menus_theme())
	{
	case HALO_MENU_THEME_VANILLA:
		return &vanilla_palette;
	case HALO_MENU_THEME_CAIRO:
		return &cairo_palette;
	default:
		return &glassed_palette;
	}
}

/* ---------- the parts every screen has */

/* where Cairo's header band ends, for the subtitle after it */
static float cairo_header_end;

/* the window's left edge, a little beyond, in the 640x480 layout */
static float screen_left(
	void)
{
	return -(float)((halo_screen_width() - 640) / 2 + 2);
}

/* Cairo's frame: deep navy over the scene; the title on a blue header band
that steps down at its end to a line across the screen, a ruler's ticks
under it; the buttons' bar at the foot. */
static void cairo_frame(
	char const *title)
{
	struct overlay_palette const *palette = overlay_palette_current();
	float left = screen_left(), width = 640.0f - 2.0f * left;
	float band_height = CAIRO_TOP_LINE - CAIRO_HEADER_TOP;
	float band_end = CAIRO_TITLE_X + ui_overlay_text_width(UI_FONT_BOLD, CAIRO_TITLE_SIZE, title) + CAIRO_TITLE_GAP +
		band_height;
	float const step[4] = { 0.0f, band_height, 0.0f, 0.0f };
	float x;

	ui_overlay_gradient(left, 0, width, 480, 0, palette->backdrop, palette->backdrop_bottom);
	ui_overlay_chamfered(left, CAIRO_HEADER_TOP, band_end - left, band_height, step, CAIRO_BAND_TOP, CAIRO_BAND_BOTTOM);
	ui_overlay_chamfered_outline(left, CAIRO_HEADER_TOP, band_end - left, band_height, step, 1.0f, CAIRO_ICE);
	ui_overlay_rect(left, CAIRO_TOP_LINE, width, 1.1f, 0, CAIRO_ICE);
	for (x = left + CAIRO_RULER_STEP; x < left + width; x += CAIRO_RULER_STEP)
		ui_overlay_rect(x, CAIRO_TOP_LINE + 3, 0.4f, 2.0f, 0, CAIRO_TICK);
	ui_overlay_rect(left, OVERLAY_FRAME_BOTTOM, width, 480 - OVERLAY_FRAME_BOTTOM, 0, CAIRO_BAR);
	ui_overlay_rect(left, OVERLAY_FRAME_BOTTOM, width, 1.0f, 0, palette->rule);
	ui_overlay_text(UI_FONT_BOLD, CAIRO_TITLE_SIZE, CAIRO_TITLE_X, CAIRO_HEADER_TOP + (band_height - CAIRO_TITLE_SIZE) / 2,
		UI_ALIGN_LEFT, palette->title, title);
	cairo_header_end = band_end;
}

void overlay_screen_frame(
	char const *title,
	float title_x,
	float title_y,
	float title_size)
{
	struct overlay_palette const *palette = overlay_palette_current();
	float left = screen_left(), width = 640.0f - 2.0f * left;

	if (palette->framed)
	{
		cairo_frame(title);
		return;
	}
	if (palette->own_screens)
	{
		/* Glassed: a darkened band over the scene, between hairlines */
		ui_overlay_rect(left, OVERLAY_FRAME_TOP, width, OVERLAY_FRAME_BOTTOM - OVERLAY_FRAME_TOP, 0, palette->backdrop);
		ui_overlay_rect(left, OVERLAY_FRAME_TOP, width, 0.75f, 0, palette->rule);
	}
	else
	{
		/* Vanilla: the whole screen, in the Xbox's blues */
		ui_overlay_gradient(left, 0, width, 480, 0, palette->backdrop, palette->backdrop_bottom);
		ui_overlay_rect(left, OVERLAY_FRAME_TOP, width, 1.0f, 0, palette->rule);
	}
	ui_overlay_rect(left, OVERLAY_FRAME_BOTTOM - 0.75f, width, 0.75f, 0, palette->rule);
	ui_overlay_text(UI_FONT_BOLD, title_size, title_x, title_y, UI_ALIGN_LEFT, palette->title, title);
}

void overlay_screen_subtitle(
	char const *text,
	float x,
	float y)
{
	struct overlay_palette const *palette = overlay_palette_current();

	if (palette->framed)
	{
		x = cairo_header_end + 10.0f;
		y = CAIRO_HEADER_TOP + 12.0f;
	}
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, x, y, UI_ALIGN_LEFT, palette->dim, text);
}

/* A steel corner piece at a panel's corner (x, y): an L whose arms run
across (1 right, -1 left) and down (1 down, -1 up) from it, lit at the top
and shaded at the foot, its outer corner cut. */
static void cairo_bracket(
	float x,
	float y,
	float across,
	float down)
{
	/* (the cut corner, in ui_overlay_chamfered's order) */
	short corner = down > 0 ? (across > 0 ? 0 : 1) : (across > 0 ? 3 : 2);
	float cuts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float arm_x = across > 0 ? x : x - CAIRO_BRACKET_REACH;
	float arm_y = down > 0 ? y : y - CAIRO_BRACKET;
	float bar_x = across > 0 ? x : x - CAIRO_BRACKET;
	float bar_y = down > 0 ? y : y - CAIRO_BRACKET_SPAN;

	cuts[corner] = CAIRO_BRACKET * 0.8f;
	ui_overlay_chamfered(arm_x, arm_y, CAIRO_BRACKET_REACH, CAIRO_BRACKET, cuts, CAIRO_STEEL_LIGHT, CAIRO_STEEL);
	ui_overlay_chamfered(bar_x, bar_y, CAIRO_BRACKET, CAIRO_BRACKET_SPAN, cuts, CAIRO_STEEL, CAIRO_STEEL_DARK);
}

/* Cairo's panel: dark glass cut at its top left and bottom right corners,
ruled with a fine grid, in a hairline, steel brackets hugging its right
side's ends. */
static void cairo_panel(
	float x,
	float y,
	float width,
	float height)
{
	struct overlay_palette const *palette = overlay_palette_current();
	float cut = MIN((float)CAIRO_PANEL_CUT, MIN(width, height) / 5);
	float const cuts[4] = { cut, 0.0f, cut, 0.0f };
	float line;

	ui_overlay_chamfered(x, y, width, height, cuts, palette->panel, palette->panel);
	/* (the grid's lines stop at the cut corners) */
	for (line = x + CAIRO_GRID; line < x + width; line += CAIRO_GRID)
	{
		float top = y + MAX(0.0f, cut - (line - x));
		float bottom = y + height - MAX(0.0f, cut - (x + width - line));

		ui_overlay_rect(line, top, 0.4f, bottom - top, 0, CAIRO_GRID_LINE);
	}
	for (line = y + CAIRO_GRID; line < y + height; line += CAIRO_GRID)
	{
		float from = x + MAX(0.0f, cut - (line - y));
		float to = x + width - MAX(0.0f, cut - (y + height - line));

		ui_overlay_rect(from, line, to - from, 0.4f, 0, CAIRO_GRID_LINE);
	}
	ui_overlay_chamfered_outline(x, y, width, height, cuts, 0.8f, palette->panel_edge);
	cairo_bracket(x + width + 2, y - 2, -1, 1);
	cairo_bracket(x + width + 2, y + height + 2, -1, -1);
}

void overlay_panel(
	float x,
	float y,
	float width,
	float height)
{
	struct overlay_palette const *palette = overlay_palette_current();

	if (palette->framed)
		cairo_panel(x, y, width, height);
	else
		ui_overlay_rect(x, y, width, height, palette->radius, palette->panel);
}

/* the bracket, [, beside a Cairo row: bright on the chosen one */
static void cairo_row_bracket(
	float x,
	float y,
	float height,
	unsigned int color)
{
	ui_overlay_rect(x, y, 1.0f, height, 0, color);
	ui_overlay_rect(x, y, 3.0f, 1.0f, 0, color);
	ui_overlay_rect(x, y + height - 1.0f, 3.0f, 1.0f, 0, color);
}

void overlay_row(
	float x,
	float y,
	float width,
	float height,
	boolean chosen,
	boolean striped)
{
	struct overlay_palette const *palette = overlay_palette_current();

	if (palette->framed)
	{
		/* a slate bar, a little clear above and below, behind a bracket */
		float gap = height * 0.12f;

		y += gap;
		height -= 2 * gap;
		if (chosen)
			ui_overlay_rect(x - 1.5f, y - 1.5f, 4.5f, height + 3.0f, 2.0f, CAIRO_LIT_GLOW);
		ui_overlay_rect(x + 5, y, width - 5, height, 0, chosen ? palette->row_selected : palette->row_rule);
		ui_overlay_rect(x + 5, y, width - 5, 0.6f, 0, chosen ? CAIRO_LIT_EDGE : CAIRO_ROW_EDGE);
		cairo_row_bracket(x, y, height, chosen ? 0xFFFFFFFF : CAIRO_STEEL);
		return;
	}
	if (chosen)
	{
		ui_overlay_rect(x, y, width, height, palette->radius / 2, palette->row_selected);
		/* (Glassed's tick down its left) */
		if (palette->own_screens)
			ui_overlay_rect(x, y, 1.5f, height, 0, 0xFFFFFFFF);
	}
	else if (striped)
	{
		ui_overlay_rect(x, y, width, height, 0, palette->row_rule);
	}
}

void overlay_text_fitted(
	int font,
	float size,
	float x,
	float y,
	float width,
	unsigned int color,
	char const *text)
{
	char fitted[128];
	size_t length;

	snprintf(fitted, sizeof(fitted), "%s", text);
	length = strlen(fitted);
	if (ui_overlay_text_width(font, size, fitted) > width)
	{
		/* drop whole UTF-8 characters until it fits with the ellipsis */
		while (length > 0)
		{
			do
				length--;
			while (length > 0 && (fitted[length] & 0xC0) == 0x80);
			snprintf(fitted + length, sizeof(fitted) - length, "\xE2\x80\xA6");
			if (ui_overlay_text_width(font, size, fitted) <= width)
				break;
		}
	}
	ui_overlay_text(font, size, x, y, UI_ALIGN_LEFT, color, fitted);
}

char const *overlay_xbox_level_name(
	short level)
{
	return level >= 0 && level < NUMBEROF(xbox_levels) ? xbox_levels[level][1] : NULL;
}

char const *overlay_xbox_map_name(
	char const *file_name)
{
	short level = xbox_level(file_name);

	return level != NONE ? xbox_levels[level][1] : NULL;
}

void overlay_map_name(
	char const *map_name,
	char *out,
	long size)
{
	char const *file_name = tag_name_strip_path(map_name);
	char const *xbox_name = overlay_xbox_map_name(file_name);
	short display_index = custom_edition_maps_display_index(map_name);
	wchar_t const *custom_name = display_index != NONE ? custom_edition_maps_name(display_index) : NULL;

	if (xbox_name)
		snprintf(out, (size_t)size, "%s", xbox_name);
	else if (custom_name)
		overlay_utf8((unsigned short const *)custom_name, size, out, size);
	else
		snprintf(out, (size_t)size, "%s", file_name);
}

boolean overlay_repeat_step(
	struct overlay_repeat *repeat,
	boolean held)
{
	unsigned long now = system_milliseconds();

	if (!held)
	{
		repeat->held = FALSE;
		return FALSE;
	}
	if (!repeat->held)
	{
		repeat->held = TRUE;
		repeat->next_time = now + REPEAT_DELAY;
		return TRUE;
	}
	if (now < repeat->next_time)
		return FALSE;
	repeat->next_time = now + REPEAT_PERIOD;
	return TRUE;
}

void overlay_utf8(
	unsigned short const *text,
	long length,
	char *out,
	long size)
{
	long used = 0, index;

	for (index = 0; text && index < length && text[index] && used < size - 4; index++)
	{
		unsigned int character = text[index];

		if (character < 0x80)
			out[used++] = (char)character;
		else if (character < 0x800)
		{
			out[used++] = (char)(0xC0 | (character >> 6));
			out[used++] = (char)(0x80 | (character & 0x3F));
		}
		else
		{
			out[used++] = (char)(0xE0 | (character >> 12));
			out[used++] = (char)(0x80 | ((character >> 6) & 0x3F));
			out[used++] = (char)(0x80 | (character & 0x3F));
		}
	}
	out[used] = 0;
}

static float button_width(
	char const *label)
{
	return BUTTON_PADDING + ui_overlay_text_width(UI_FONT_BOLD, BUTTON_TEXT_SIZE, label) + BUTTON_PADDING;
}

float overlay_buttons_width(
	char const *const *labels,
	short count)
{
	float width = 0.0f;
	short index;

	for (index = 0; index < count; index++)
		width += button_width(labels[index]) + (index ? BUTTON_GAP : 0);
	return width;
}

void overlay_button_colors_get(
	struct overlay_button_colors *colors)
{
	struct overlay_palette const *palette = overlay_palette_current();

	colors->fill = palette->panel;
	colors->fill_lit = palette->row_selected;
	colors->edge = palette->panel_edge;
	colors->text = palette->prompt;
	colors->text_lit = palette->title;
	colors->text_disabled = palette->dim;
	colors->radius = palette->radius;
}

void overlay_buttons_draw(
	char const *const *labels,
	short count,
	float x,
	float y,
	short hovered,
	unsigned long disabled,
	struct overlay_button_colors const *colors)
{
	short index;

	for (index = 0; index < count; index++)
	{
		float width = button_width(labels[index]);
		boolean usable = !TEST_FLAG(disabled, index);

		overlay_button_draw(labels[index], x, y, width, OVERLAY_BUTTON_HEIGHT, usable && index == hovered, usable,
			colors);
		x += width + BUTTON_GAP;
	}
}

void overlay_button_draw(
	char const *label,
	float x,
	float y,
	float width,
	float height,
	boolean lit,
	boolean usable,
	struct overlay_button_colors const *colors)
{
	unsigned int fill = lit ? colors->fill_lit : colors->fill;

	if (overlay_palette_current()->framed)
	{
		/* Cairo's: cut at two corners, and lit behind a bracket */
		float const cuts[4] = { CAIRO_BUTTON_CUT, 0.0f, CAIRO_BUTTON_CUT, 0.0f };

		ui_overlay_chamfered(x, y, width, height, cuts, fill, fill);
		ui_overlay_chamfered_outline(x, y, width, height, cuts, 0.75f, colors->edge);
		if (lit)
			cairo_row_bracket(x - 4, y, height, 0xFFFFFFFF);
	}
	else
	{
		ui_overlay_rect(x, y, width, height, colors->radius, fill);
		ui_overlay_outline(x, y, width, height, colors->radius, 0.75f, colors->edge);
	}
	ui_overlay_text(UI_FONT_BOLD, BUTTON_TEXT_SIZE, x + width / 2, y + (height - BUTTON_TEXT_SIZE) / 2 - 1,
		UI_ALIGN_CENTER, !usable ? colors->text_disabled : lit ? colors->text_lit : colors->text, label);
}

short overlay_button_at(
	char const *const *labels,
	short count,
	float x,
	float y,
	short point_x,
	short point_y)
{
	short index;

	if (point_y < y || point_y >= y + OVERLAY_BUTTON_HEIGHT)
		return NONE;
	for (index = 0; index < count; index++)
	{
		float width = button_width(labels[index]);

		if (point_x >= x && point_x < x + width)
			return index;
		x += width + BUTTON_GAP;
	}
	return NONE;
}

short overlay_map_display_index(
	char const *map_name)
{
	short level = xbox_level(tag_name_strip_path(map_name));
	short custom = custom_edition_maps_display_index(map_name);

	if (level != NONE)
		return level;
	return custom != NONE ? custom : UNKNOWN_LEVEL_FRAME;
}

void overlay_map_picture(
	short display_index,
	float x,
	float y,
	float width,
	float height)
{
	long pictures = tag_loaded('bitm', LEVEL_PICTURES);
	short frame = display_index;
	struct bitmap_data *bitmap;
	rectangle2d bounds, art;

	ui_overlay_cutout(x, y, width, height);
	bounds.x0 = (short)x;
	bounds.y0 = (short)y;
	bounds.x1 = (short)(x + width);
	bounds.y1 = (short)(y + height);
	draw_quad(&bounds, 0xFF0A0C10);
	if (pictures == NONE)
		return;
	/* A Custom Edition map's own picture fills the whole rectangle. A map
	without one gets the unknown level's frame. A stock campaign level gets
	its frame from the campaign menu's pictures, cropped like the rest. */
	bitmap = custom_edition_maps_picture(pictures, &frame);
	if (bitmap && custom_edition_maps_campaign_level(display_index) == NONE)
	{
		draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, FALSE);
		return;
	}
	if (!bitmap)
		bitmap = bitmap_group_get_bitmap_from_sequence(pictures, 0, frame);
	if (!bitmap)
		return;
	art.x0 = 0;
	art.y0 = 0;
	art.x1 = (short)MIN(LEVEL_PICTURE_WIDTH, bitmap->width);
	art.y1 = (short)MIN(LEVEL_PICTURE_HEIGHT, bitmap->height);
	draw_bitmap_in_rect(bitmap, &bounds, &art, NULL, 0xFFFFFFFF, NULL, FALSE);
}

/* size of the stock menu list rows (main_menu/new_select/list_item_*), as
this client's themes lay them out: clear of the description pane
(tools/shell_skin.py's SELECTION_ROW_WIDTH) */
enum
{
	LIT_ROW_WIDTH = 360,
	LIT_ROW_HEIGHT = 28,
};

struct widget_instance *ui_widget_port_top(void);

void overlay_lit_row_render(
	void)
{
	struct overlay_palette const *palette = overlay_palette_current();
	struct widget_instance *top = ui_widget_port_top();
	struct widget_instance *widget;

	/* only the gametype lists (Multiplayer and the playlist editor), which
	use the stock menu rows */
	if (!palette->own_screens || !top || !top->name || strcmp(top->name, "gametype_select_screen"))
		return;
	/* find the focused row; widget offsets are already screen positions */
	for (widget = top; widget->focused_child; widget = widget->focused_child)
		;
	while (widget && widget->name && strncmp(widget->name, "list_item_", 10))
		widget = widget->parent;
	if (!widget || !widget->visible)
		return;
	overlay_row(widget->horizontal_offset, widget->vertical_offset, LIT_ROW_WIDTH, LIT_ROW_HEIGHT, TRUE, FALSE);
}

#endif
