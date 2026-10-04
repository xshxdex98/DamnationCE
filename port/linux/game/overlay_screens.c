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

/* ---------- public code */

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

#endif
