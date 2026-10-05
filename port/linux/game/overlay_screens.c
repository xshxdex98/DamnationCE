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
static char const *const xbox_levels[] =
{
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout",
	"carousel", "boardingaction", "bloodgulch", "wizard", "putput", "longest",
};
#define UNKNOWN_LEVEL_FRAME NUMBEROF(xbox_levels)

/* the Xbox multiplayer levels' display names */
static char const *const xbox_map_names[][2] =
{
	{ "beavercreek", "Battle Creek" }, { "bloodgulch", "Blood Gulch" }, { "boardingaction", "Boarding Action" },
	{ "carousel", "Derelict" }, { "chillout", "Chill Out" }, { "damnation", "Damnation" },
	{ "hangemhigh", "Hang 'Em High" }, { "longest", "Longest" }, { "prisoner", "Prisoner" },
	{ "putput", "Chiron TL-34" }, { "ratrace", "Rat Race" }, { "sidewinder", "Sidewinder" }, { "wizard", "Wizard" },
};

static struct overlay_palette const glassed_palette =
{
	TRUE, 0x06080C8C, 0x06080C8C, 0xFFFFFF5A, 0xFFFFFFD7, 0x06080C78, 0xFFFFFF46, 0xFFFFFF1A, 0xB4B8BCFF, 0xFFFFFF3E,
	0xFFFFFF14, 0xD2D6DAFF, 0x8C9096FF, 0xA8ACB0FF, 0xD2D6DAFF, 0x06080CE6, 0.0f,
};
static struct overlay_palette const vanilla_palette =
{
	FALSE, 0x0B1830FF, 0x03070FFF, 0x2A62C8FF, 0x3D8BFFFF, 0x081530F0, 0x2F6DD0FF, 0x123266FF, 0x7FB0FFFF, 0x2052B0FF,
	0x16294AFF, 0xE6EEFCFF, 0x8FA6C8FF, 0x4AA3FFFF, 0x4AA3FFFF, 0x0A1A36F8, 6.0f,
};

char const *config_string(char const *name);

/* ---------- public code */

struct overlay_palette const *overlay_palette_current(
	void)
{
	return strcmp(config_string("display.theme"), "vanilla") ? &glassed_palette : &vanilla_palette;
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

char const *overlay_xbox_map_name(
	char const *file_name)
{
	short index;

	for (index = 0; index < NUMBEROF(xbox_map_names); index++)
	{
		if (!csstrcmp(file_name, xbox_map_names[index][0]))
			return xbox_map_names[index][1];
	}
	return NULL;
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
		boolean lit = usable && index == hovered;

		ui_overlay_rect(x, y, width, OVERLAY_BUTTON_HEIGHT, colors->radius, lit ? colors->fill_lit : colors->fill);
		ui_overlay_outline(x, y, width, OVERLAY_BUTTON_HEIGHT, colors->radius, 0.75f, colors->edge);
		ui_overlay_text(UI_FONT_BOLD, BUTTON_TEXT_SIZE, x + width / 2, y + 5, UI_ALIGN_CENTER,
			!usable ? colors->text_disabled : lit ? colors->text_lit : colors->text, labels[index]);
		x += width + BUTTON_GAP;
	}
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
	char const *name = tag_name_strip_path(map_name);
	short index, custom;

	for (index = 0; index < NUMBEROF(xbox_levels); index++)
	{
		if (!csstrcmp(name, xbox_levels[index]))
			return index;
	}
	custom = custom_edition_maps_display_index(map_name);
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

/* the stock menus' list rows (main_menu/new_select/list_item_*) */
enum
{
	LIT_ROW_WIDTH = 390,
	LIT_ROW_HEIGHT = 28,
};

struct widget_instance *ui_widget_port_top(void);

void overlay_lit_row_render(
	void)
{
	struct overlay_palette const *palette = overlay_palette_current();
	struct widget_instance *top = ui_widget_port_top();
	struct widget_instance *widget;

	/* (the gametype lists, Multiplayer's and the playlist editor's: their
	rows are the stock menus') */
	if (!palette->glassed || !top || !top->name || strcmp(top->name, "gametype_select_screen"))
		return;
	/* the focused row (a widget's offsets are its place on the screen) */
	for (widget = top; widget->focused_child; widget = widget->focused_child)
		;
	while (widget && widget->name && strncmp(widget->name, "list_item_", 10))
		widget = widget->parent;
	if (!widget || !widget->visible)
		return;
	ui_overlay_rect(widget->horizontal_offset, widget->vertical_offset, LIT_ROW_WIDTH, LIT_ROW_HEIGHT,
		palette->radius / 2, palette->row_selected);
	ui_overlay_rect(widget->horizontal_offset, widget->vertical_offset, 1.5f, LIT_ROW_HEIGHT, 0, 0xFFFFFFFF);
}

#endif
