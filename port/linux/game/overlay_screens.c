/*
OVERLAY_SCREENS.C

What the screens the overlay draws over the menus share (overlay_screens.h).
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
	/* a held direction moves again after this long, and then this often */
	REPEAT_DELAY = 350,
	REPEAT_PERIOD = 90,

	/* the prompts' buttons and words, and the foot of the screen they are on */
	PROMPT_BUTTON_SIZE = 15,
	PROMPT_TEXT_SIZE = 12,
	PROMPT_Y = 455,
	PROMPT_GAP = 20,
};

/* the menus' level pictures: one frame for each Xbox level, in the game's
order of them (ui_widget_event_handler_functions.c), then the unknown
level's; the picture fills each frame's top left */
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

float overlay_prompt(
	int button,
	char const *words,
	float x,
	unsigned int color)
{
	x += ui_overlay_button(button, PROMPT_BUTTON_SIZE, x, PROMPT_Y, 0xFFFFFFFF) + 3.0f;
	return x + ui_overlay_text(UI_FONT_BOLD, PROMPT_TEXT_SIZE, x, PROMPT_Y + 1.5f, UI_ALIGN_LEFT, color, words) +
		PROMPT_GAP;
}

float overlay_prompt_width(
	int button,
	char const *words)
{
	return ui_overlay_button_width(button, PROMPT_BUTTON_SIZE) + 3.0f +
		ui_overlay_text_width(UI_FONT_BOLD, PROMPT_TEXT_SIZE, words) + PROMPT_GAP;
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
	/* (a Custom Edition map's own picture, drawn over the whole place; one
	without has the unknown level's frame) */
	bitmap = custom_edition_maps_picture(pictures, &frame);
	if (bitmap)
	{
		draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, FALSE);
		return;
	}
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
