/*
MAP_SCREEN.C

The multiplayer map picker (configure.py --game-browser, as the overlay it
draws on): a screen of its own over the menus, drawn and driven by code as
Online Games is (browser_screen.c), in place of the PC menus' map list.

It opens on two categories, VANILLA (the Xbox's 13 levels) and CUSTOM (the
Custom Edition maps in the maps folder, custom_edition_maps.c); a category
opens its maps. Y turns the maps between a list, with the chosen map's
picture and description beside it, and a grid of cards, each map's picture
over its name. A picks the map, as the PC menus' list does, and opens the
game types that follow it; B goes back a step.

The Map screen's widget runs "port map select" as it is made (menu_tags.c,
the menus' skin): that opens this, which stays up over it until a map is
picked or B leaves both.
*/

#ifdef HALO_GAME_BROWSER

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cutscene/cinematics.h"
#include "interface/event_manager.h"
#include "interface/ui_widget.h"
#include "rasterizer/rasterizer.h"
#include "bitmaps/bitmap_group.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "input/input.h"

#include "custom_edition_maps.h"
#include "../src/ui_overlay.h"
#include "halo_ui_pointer.h"

#include <stdio.h>
#include <string.h>

/* ---------- constants */

enum
{
	/* (event_manager.c's event types, which it keeps to itself) */
	MAP_EVENT_LEFT_STICK = 1,
	MAP_EVENT_BUTTON = 3,

	/* the screen takes no A this soon after it opens */
	OPEN_SETTLE = 600,
	/* a held direction moves again after this long, and then this often */
	REPEAT_DELAY = 350,
	REPEAT_PERIOD = 90,

	NUMBER_OF_CATEGORIES = 2,
	MAXIMUM_LEVELS = 160,
};

enum
{
	VIEW_LIST,
	VIEW_GRID,
};

/* the layout, in the menus' 640x480 */
enum
{
	GLASS_TOP = 66, GLASS_BOTTOM = 446,
	/* the categories and the list: rows down the left */
	ROW_X = 37, ROW_Y = 80, ROW_WIDTH = 300, ROW_HEIGHT = 22, LIST_ROWS = 15,
	/* the list's chosen map, at the right */
	PREVIEW_X = 362, PREVIEW_Y = 80, PREVIEW_WIDTH = 240, PREVIEW_HEIGHT = 196,
	/* the grid's cards */
	GRID_X = 37, GRID_Y = 80, GRID_COLUMNS = 4, GRID_ROWS = 3, CARD_WIDTH = 132, CARD_PICTURE = 107,
	CARD_HEIGHT = 124, CARD_GAP_X = 13, CARD_GAP_Y = 0,
};

/* the menus' colors (browser_screen.c's): dark glass, hairlines, white for what is chosen */
enum
{
	COLOR_GLASS = 0x06080C8C,
	COLOR_RULE = 0xFFFFFF5A,
	COLOR_TITLE = 0xFFFFFFD7,
	COLOR_CHOSEN = 0xFFFFFF3E,
	COLOR_TICK = 0xFFFFFFFF,
	COLOR_TEXT = 0xD2D6DAFF,
	COLOR_DIM = 0x8C9096FF,
	COLOR_EDGE = 0xFFFFFF46,
};

/* the Xbox levels' pictures and their names, in the game's order of them
(ui_widget_event_handler_functions.c: the menus' mp_map_grafix frames) */
#define LEVEL_PICTURES "ui\\shell\\bitmaps\\mp_map_grafix"
#define LEVEL_DESCRIPTIONS "pc\\main_menu\\multiplayer_type_select\\mp_map_select\\map_data"
#define GAMETYPES_SCREEN "pc\\main_menu\\multiplayer_type_select\\connected\\gametype_select_screen_wrapper"
static char const *const xbox_level_names[] =
{
	"Battle Creek", "Sidewinder", "Damnation", "Rat Race", "Prisoner", "Hang 'Em High", "Chill Out",
	"Derelict", "Boarding Action", "Blood Gulch", "Wizard", "Chiron TL-34", "Longest",
};
static char const *const category_names[NUMBER_OF_CATEGORIES] = { "VANILLA", "CUSTOM" };

/* (interface/: the multiplayer levels, the map chosen as the PC menus'
list chooses it, and the screens opened and left) */
char **ui_widget_port_multiplayer_levels(short *count, short *xbox_count);
boolean ui_widget_port_multiplayer_level_choose(char const *map_name);
boolean ui_widget_port_open_from_top(char const *name);
void ui_widget_port_go_back_from_top(void);

/* ---------- globals */

static struct
{
	boolean active;
	/* NONE while the categories are shown */
	short category;
	short view;
	/* the shown category's levels, by their index in the level list */
	short levels[MAXIMUM_LEVELS];
	short count;
	short selected;
	short first;
	/* the categories' row chosen, kept while a category is open */
	short category_selected;
	char **level_names;
	short level_count;
	short xbox_count;
	unsigned long opened_time;
	boolean held;
	unsigned long repeat_time;
} map_screen = { FALSE, NONE, VIEW_LIST };

/* ---------- private code */

static void utf8_of(wchar_t const *text, char *out, long size)
{
	long used = 0;

	for (; text && *text && used < size - 4; text++)
	{
		unsigned int character = (unsigned int)*text & 0xFFFF;

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

/* a level's name in the menus, and its description (empty if none) */
static void level_text(short level, char *name, char *description, long size)
{
	short display = custom_edition_maps_level_display_index(level);

	description[0] = 0;
	if (level < map_screen.xbox_count)
	{
		long strings = tag_loaded('ustr', LEVEL_DESCRIPTIONS);

		csstrncpy(name, level < NUMBEROF(xbox_level_names) ? xbox_level_names[level] : "?", (size_t)size - 1);
		name[size - 1] = 0;
		if (strings != NONE)
			utf8_of(unicode_string_list_get_string(strings, level), description, size);
		return;
	}
	utf8_of(custom_edition_maps_name(display), name, size);
	utf8_of(custom_edition_maps_description(display), description, size);
}

/* a level's picture in a place of the screen: an Xbox level's frame of the
menus' pictures, or a Custom Edition map's own (the unknown level's frame
for one without) */
static void level_picture(short level, short x0, short y0, short x1, short y1)
{
	long pictures = tag_loaded('bitm', LEVEL_PICTURES);
	short frame = custom_edition_maps_level_display_index(level);
	struct bitmap_data *bitmap = pictures == NONE ? NULL : custom_edition_maps_picture(pictures, &frame);
	rectangle2d bounds, art;

	ui_overlay_cutout(x0, y0, (float)(x1 - x0), (float)(y1 - y0));
	bounds.x0 = x0;
	bounds.y0 = y0;
	bounds.x1 = x1;
	bounds.y1 = y1;
	draw_quad(&bounds, 0xFF0A0C10);
	if (bitmap)
	{
		draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, FALSE);
		return;
	}
	bitmap = pictures == NONE ? NULL : bitmap_group_get_bitmap_from_sequence(pictures, 0, frame);
	if (!bitmap)
		return;
	/* (the picture fills the bitmap's top left; the rest is margin) */
	art.x0 = 0;
	art.y0 = 0;
	art.x1 = (short)MIN(140, bitmap->width);
	art.y1 = (short)MIN(114, bitmap->height);
	draw_bitmap_in_rect(bitmap, &bounds, &art, NULL, 0xFFFFFFFF, NULL, FALSE);
}

/* the level list's levels in a category: the Xbox levels, or the maps after them */
static short category_levels(short category, short *levels)
{
	short count = 0, level;

	for (level = 0; level < map_screen.level_count && count < MAXIMUM_LEVELS; level++)
	{
		if ((level < map_screen.xbox_count) == (category == 0))
			levels[count++] = level;
	}
	return count;
}

static void category_open(short category)
{
	map_screen.category = category;
	map_screen.count = category_levels(category, map_screen.levels);
	map_screen.selected = 0;
	map_screen.first = 0;
}

static short page_size(void)
{
	return map_screen.view == VIEW_GRID ? GRID_COLUMNS * GRID_ROWS : LIST_ROWS;
}

/* the first row or card shown, keeping the selection on the page */
static void keep_in_view(void)
{
	short page = page_size();
	short step = map_screen.view == VIEW_GRID ? GRID_COLUMNS : 1;

	if (map_screen.selected < map_screen.first)
		map_screen.first = (short)(map_screen.selected - map_screen.selected % step);
	while (map_screen.selected >= map_screen.first + page)
		map_screen.first = (short)(map_screen.first + step);
}

static void pick(void)
{
	if (system_milliseconds() - map_screen.opened_time < OPEN_SETTLE)
		return;
	if (map_screen.category == NONE)
	{
		category_open(map_screen.category_selected);
		return;
	}
	if (!map_screen.count ||
		!ui_widget_port_multiplayer_level_choose(map_screen.level_names[map_screen.levels[map_screen.selected]]))
	{
		return;
	}
	map_screen.active = FALSE;
	ui_widget_port_open_from_top(GAMETYPES_SCREEN);
}

static void back(void)
{
	if (map_screen.category != NONE)
	{
		map_screen.category = NONE;
		return;
	}
	map_screen.active = FALSE;
	ui_widget_port_go_back_from_top();
}

/* a move: up and down by one (a grid's row), left and right a grid's card
or a list's page */
static void move(short dx, short dy)
{
	short *selected = map_screen.category == NONE ? &map_screen.category_selected : &map_screen.selected;
	short count = map_screen.category == NONE ? NUMBER_OF_CATEGORIES : map_screen.count;
	short step;

	if (map_screen.category == NONE || map_screen.view == VIEW_LIST)
		step = (short)(dy + dx * (map_screen.category == NONE ? 0 : LIST_ROWS));
	else
		step = (short)(dx + dy * GRID_COLUMNS);
	*selected = (short)PIN(*selected + step, 0, MAX(0, count - 1));
	keep_in_view();
}

/* a direction's steps this frame: one when it is first pressed, then, held,
one every REPEAT_PERIOD after REPEAT_DELAY */
static boolean repeated(boolean held)
{
	unsigned long now = system_milliseconds();

	if (!held)
	{
		map_screen.held = FALSE;
		return FALSE;
	}
	if (!map_screen.held)
	{
		map_screen.held = TRUE;
		map_screen.repeat_time = now + REPEAT_DELAY;
		return TRUE;
	}
	if (now < map_screen.repeat_time)
		return FALSE;
	map_screen.repeat_time = now + REPEAT_PERIOD;
	return TRUE;
}

/* the row or card at a point of the 640x480 layout (its index in what is
shown), or NONE */
static short item_at(short x, short y)
{
	if (map_screen.category == NONE || map_screen.view == VIEW_LIST)
	{
		short row = (short)((y - ROW_Y) / ROW_HEIGHT);
		short rows = map_screen.category == NONE ? NUMBER_OF_CATEGORIES : LIST_ROWS;

		if (x < ROW_X || x >= ROW_X + ROW_WIDTH || y < ROW_Y || row >= rows)
			return NONE;
		return map_screen.category == NONE ? row : (short)(map_screen.first + row);
	}
	{
		short column = (short)((x - GRID_X) / (CARD_WIDTH + CARD_GAP_X));
		short row = (short)((y - GRID_Y) / (CARD_HEIGHT + CARD_GAP_Y));

		if (x < GRID_X || y < GRID_Y || column >= GRID_COLUMNS || row >= GRID_ROWS ||
			(x - GRID_X) % (CARD_WIDTH + CARD_GAP_X) >= CARD_WIDTH)
		{
			return NONE;
		}
		return (short)(map_screen.first + row * GRID_COLUMNS + column);
	}
}

static void chosen_row(float x, float y, float width, float height)
{
	ui_overlay_rect(x, y, width, height, 0, COLOR_CHOSEN);
	ui_overlay_rect(x, y, 1.5f, height, 0, COLOR_TICK);
}

static float prompt(int button, char const *words, float x)
{
	x += ui_overlay_button(button, 15.0f, x, 455.0f, 0xFFFFFFFF) + 3.0f;
	return x + ui_overlay_text(UI_FONT_BOLD, 12.0f, x, 456.5f, UI_ALIGN_LEFT, COLOR_TEXT, words) + 20.0f;
}

static void render_categories(void)
{
	short category, levels[MAXIMUM_LEVELS];
	char text[64];

	for (category = 0; category < NUMBER_OF_CATEGORIES; category++)
	{
		float y = (float)(ROW_Y + category * ROW_HEIGHT);
		short count = category_levels(category, levels);

		if (category == map_screen.category_selected)
			chosen_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, ROW_X + 10, y + 4, UI_ALIGN_LEFT, COLOR_TEXT, category_names[category]);
		snprintf(text, sizeof(text), "%d", count);
		ui_overlay_text(UI_FONT_REGULAR, 11.0f, ROW_X + ROW_WIDTH - 10, y + 5, UI_ALIGN_RIGHT, COLOR_DIM, text);
	}
	if (!category_levels(1, levels))
	{
		ui_overlay_text(UI_FONT_REGULAR, 10.0f, ROW_X + 10, ROW_Y + NUMBER_OF_CATEGORIES * ROW_HEIGHT + 14,
			UI_ALIGN_LEFT, COLOR_DIM, "Put Custom Edition maps in the maps folder to play them here.");
	}
}

static void render_list(void)
{
	short row;
	char name[96], description[512];

	for (row = 0; row < LIST_ROWS && map_screen.first + row < map_screen.count; row++)
	{
		float y = (float)(ROW_Y + row * ROW_HEIGHT);
		short index = (short)(map_screen.first + row);

		if (index == map_screen.selected)
			chosen_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
		level_text(map_screen.levels[index], name, description, sizeof(name));
		ui_overlay_text(UI_FONT_BOLD, 11.0f, ROW_X + 10, y + 5, UI_ALIGN_LEFT, COLOR_TEXT, name);
	}
	if (!map_screen.count)
		return;
	/* the chosen map, at the right */
	level_picture(map_screen.levels[map_screen.selected], PREVIEW_X, PREVIEW_Y, PREVIEW_X + PREVIEW_WIDTH,
		PREVIEW_Y + PREVIEW_HEIGHT);
	ui_overlay_outline(PREVIEW_X, PREVIEW_Y, PREVIEW_WIDTH, PREVIEW_HEIGHT, 0, 0.75f, COLOR_EDGE);
	level_text(map_screen.levels[map_screen.selected], name, description, sizeof(description));
	ui_overlay_text(UI_FONT_BOLD, 15.0f, PREVIEW_X, PREVIEW_Y + PREVIEW_HEIGHT + 8, UI_ALIGN_LEFT, COLOR_TITLE, name);
	{
		/* (the description's lines, as written) */
		char *line = description;
		float y = PREVIEW_Y + PREVIEW_HEIGHT + 30;

		while (line && *line && y < GLASS_BOTTOM - 12)
		{
			char *end = strchr(line, '\n');

			if (end)
				*end = 0;
			ui_overlay_text(UI_FONT_REGULAR, 10.0f, PREVIEW_X, y, UI_ALIGN_LEFT, COLOR_DIM, line);
			y += 13;
			line = end ? end + 1 : NULL;
		}
	}
}

static void render_grid(void)
{
	short slot;
	char name[96], description[512];

	for (slot = 0; slot < GRID_COLUMNS * GRID_ROWS && map_screen.first + slot < map_screen.count; slot++)
	{
		short index = (short)(map_screen.first + slot);
		short x = (short)(GRID_X + (slot % GRID_COLUMNS) * (CARD_WIDTH + CARD_GAP_X));
		short y = (short)(GRID_Y + (slot / GRID_COLUMNS) * (CARD_HEIGHT + CARD_GAP_Y));

		level_picture(map_screen.levels[index], x, y, (short)(x + CARD_WIDTH), (short)(y + CARD_PICTURE));
		if (index == map_screen.selected)
		{
			ui_overlay_outline(x, y, CARD_WIDTH, CARD_PICTURE, 0, 1.5f, COLOR_TICK);
			chosen_row(x, (float)(y + CARD_PICTURE), CARD_WIDTH, 15);
		}
		else
			ui_overlay_outline(x, y, CARD_WIDTH, CARD_PICTURE, 0, 0.75f, COLOR_EDGE);
		level_text(map_screen.levels[index], name, description, sizeof(name));
		ui_overlay_text(UI_FONT_BOLD, 10.0f, x + 6, y + CARD_PICTURE + 2, UI_ALIGN_LEFT,
			index == map_screen.selected ? COLOR_TITLE : COLOR_TEXT, name);
	}
}

/* ---------- public code */

boolean map_screen_active(void)
{
	return map_screen.active;
}

/* "port map select": opened over the Map screen; FALSE (its own list then)
without the overlay */
boolean map_screen_open(void)
{
	if (!ui_overlay_available())
		return FALSE;
	map_screen.level_names = ui_widget_port_multiplayer_levels(&map_screen.level_count, &map_screen.xbox_count);
	map_screen.active = TRUE;
	map_screen.opened_time = system_milliseconds();
	map_screen.held = FALSE;
	/* (a category open before, the game types backed out of: still open) */
	if (map_screen.category != NONE)
	{
		short selected = map_screen.selected;

		category_open(map_screen.category);
		map_screen.selected = (short)PIN(selected, 0, MAX(0, map_screen.count - 1));
		keep_in_view();
	}
	/* (the menu's A, still queued, is not a pick) */
	event_manager_flush();
	return TRUE;
}

/* the mouse: what is under it is chosen, a click picks it, the wheel moves
and the right button goes back */
void map_screen_pointer(struct halo_ui_pointer const *pointer)
{
	short item;
	short count = map_screen.category == NONE ? NUMBER_OF_CATEGORIES : map_screen.count;
	short *selected = map_screen.category == NONE ? &map_screen.category_selected : &map_screen.selected;

	if (pointer->wheel_steps)
		move(0, (short)(pointer->wheel_steps > 0 ? -1 : 1));
	item = pointer->moved ? item_at(pointer->x, pointer->y) : NONE;
	if (item != NONE && item < count)
		*selected = item;
	item = pointer->left_clicks ? item_at(pointer->click_x, pointer->click_y) : NONE;
	if (item != NONE && item < count)
	{
		*selected = item;
		pick();
	}
	if (pointer->right_clicks)
		back();
}

void map_screen_process(void)
{
	struct event_record event;
	short dx = 0, dy = 0;

	while (map_screen.active && get_next_event(&event, NONE))
	{
		if (event.type == MAP_EVENT_LEFT_STICK)
		{
			dy = event.data.stick.y == SHORT_MAX ? -1 : event.data.stick.y == SHORT_MIN ? 1 : dy;
			dx = event.data.stick.x == SHORT_MIN ? -1 : event.data.stick.x == SHORT_MAX ? 1 : dx;
		}
		else if (event.type == MAP_EVENT_BUTTON)
		{
			switch (event.data.button.index)
			{
			case _gamepad_binary_button_dpad_up: dy = -1; break;
			case _gamepad_binary_button_dpad_down: dy = 1; break;
			case _gamepad_binary_button_dpad_left: dx = -1; break;
			case _gamepad_binary_button_dpad_right: dx = 1; break;
			case _gamepad_analog_button_a:
			case _gamepad_binary_button_start:
				pick();
				break;
			case _gamepad_analog_button_y:
				map_screen.view = (short)(map_screen.view == VIEW_LIST ? VIEW_GRID : VIEW_LIST);
				map_screen.first = 0;
				keep_in_view();
				break;
			case _gamepad_analog_button_b:
				back();
				break;
			default: break;
			}
		}
	}
	if (repeated(dx || dy))
		move(dx, dy);
	/* (the widgets behind take nothing while this is up) */
	event_manager_flush();
}

void map_screen_render(void)
{
	float margin = (float)((halo_screen_width() - 640) / 2 + 2);
	float x;

	if (!ui_overlay_available())
		return;
	ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, GLASS_BOTTOM - GLASS_TOP, 0, COLOR_GLASS);
	ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, 0.75f, 0, COLOR_RULE);
	ui_overlay_rect(-margin, GLASS_BOTTOM - 0.75f, 640 + 2 * margin, 0.75f, 0, COLOR_RULE);
	ui_overlay_text(UI_FONT_BOLD, 30.0f, 37, 17, UI_ALIGN_LEFT, COLOR_TITLE,
		map_screen.category == NONE ? "SELECT MAP" : category_names[map_screen.category]);

	if (map_screen.category == NONE)
		render_categories();
	else if (map_screen.view == VIEW_GRID)
		render_grid();
	else
		render_list();

	x = 37;
	x = prompt(UI_BUTTON_A, "=SELECT", x);
	x = prompt(UI_BUTTON_B, "=BACK", x);
	if (map_screen.category != NONE)
		prompt(UI_BUTTON_Y, map_screen.view == VIEW_LIST ? "=GRID VIEW" : "=LIST VIEW", x);
}

#endif
