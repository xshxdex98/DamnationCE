/*
MAP_SCREEN.C

The map picker used when setting up a game. It replaces the PC menus' map
list with a screen drawn by code over the menus (like Online Games,
browser_screen.c), in the Glassed theme. Vanilla keeps the stock list.

The steps, when hosting a network game:

	CREATE GAME:  COOPERATIVE | PVP
	COOPERATIVE:  CAMPAIGN (more modes, such as Firefight, can go here)
	CAMPAIGN:     VANILLA (the stock levels) | CUSTOM (Custom Edition
	              campaign maps in the maps folder)
	  a level:    picking one sets up the co-op game
	              (ui_widget_port_cooperative_level_choose) and opens
	              Server Setup, then the lobby
	PVP:          VANILLA (the Xbox's 13 levels and Halo PC's six) | CUSTOM
	  a map:      picking one chooses it as the PC list would, then the
	              game types screen opens

Split screen, and picking a game's next map, start at the PvP categories.
The screen always starts again from its first step when it opens.

Lists can be shown as rows (with the selected map's picture and
description beside them) or as a grid of pictures. Arrow keys move, A or
Enter picks, and Escape (B) goes back a step. Everything else is a button
along the bottom: BACK, the list/grid view, and the campaign difficulty.

It opens in one of two ways:
- The PC menus' Map screen runs the "port map select" handler.
- The Xbox map list (Online Games' Create Game, split screen, next map)
  opens it from multiplayer_level_list_initialize. A map picked here
  becomes the list's selection, and an A press is posted to the list so it
  carries on as usual. A co-op level opens Server Setup instead.
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
#include "game/game.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "saved games/player_profile.h"

#include "interface/ui_widget_instance.h"

#include "custom_edition_maps.h"
#include "overlay_screens.h"
#include "../src/ui_overlay.h"
#include "halo_ui_pointer.h"

#include <stdio.h>
#include <string.h>

/* ---------- constants */

enum
{
	/* event_manager_post_button's index for A */
	BUTTON_A = 0,

	/* ignore picks this many milliseconds after opening (the menu's own A) */
	OPEN_SETTLE = 600,

	NUMBER_OF_CATEGORIES = 2,
	/* the Xbox's levels plus the CE maps' */
	MAXIMUM_LEVELS = 16 + CUSTOM_EDITION_MAPS_MAXIMUM,

	MAXIMUM_BUTTONS = 3,
};

enum
{
	STEP_KINDS,
	STEP_COOPERATIVE_MODES,
	STEP_CAMPAIGN_CATEGORIES,
	STEP_CAMPAIGN_LEVELS,
	STEP_CATEGORIES,
	STEP_MAPS,
};

enum
{
	VIEW_LIST,
	VIEW_GRID,
};

/* the layout, in the menus' 640x480 */
enum
{
	/* rows down the left */
	ROW_X = 37, ROW_Y = 80, ROW_WIDTH = 300, ROW_HEIGHT = 22, LIST_ROWS = 15,
	/* the selected entry's picture and description on the right */
	PREVIEW_X = 362, PREVIEW_Y = 80, PREVIEW_WIDTH = 240, PREVIEW_HEIGHT = 196,
	GRID_X = 37, GRID_Y = 80, GRID_COLUMNS = 4, GRID_ROWS = 3, CARD_WIDTH = 132, CARD_PICTURE = 107,
	CARD_HEIGHT = 124, CARD_GAP_X = 13, CARD_GAP_Y = 0,
};

#define LEVEL_DESCRIPTIONS "pc\\main_menu\\multiplayer_type_select\\mp_map_select\\map_data"
#define GAMETYPES_SCREEN "pc\\main_menu\\multiplayer_type_select\\connected\\gametype_select_screen_wrapper"
#define SERVER_SETUP_SCREEN "pc\\main_menu\\multiplayer_type_select\\server_settings\\server_settings_screen"

/* in the game's level order (ui_widget_event_handler_functions.c) */
static char const *const xbox_level_names[] =
{
	"Battle Creek", "Sidewinder", "Damnation", "Rat Race", "Prisoner", "Hang 'Em High", "Chill Out",
	"Derelict", "Boarding Action", "Blood Gulch", "Wizard", "Chiron TL-34", "Longest",
};
static char const *const category_names[NUMBER_OF_CATEGORIES] = { "VANILLA", "CUSTOM" };
static char const *const difficulty_names[NUMBER_OF_GAME_DIFFICULTY_LEVELS] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

/* a row of the first two steps, with the description shown beside it */
struct step_row
{
	char const *name;
	char const *description;
};
static struct step_row const kind_rows[] =
{
	{ "COOPERATIVE", "Play together against the\nCovenant and the Flood." },
	{ "PVP", "Play against each other on\nthe multiplayer maps." },
};
static struct step_row const cooperative_rows[] =
{
	{ "CAMPAIGN", "The campaign's levels, with\nevery player in the game." },
};

/* colors (0xRRGGBBAA): the theme's (overlay_screens.c; the picker is this
client's screens' alone: Vanilla keeps the stock map list) */
#define PALETTE (overlay_palette_current())
#define COLOR_TITLE (PALETTE->title)
#define COLOR_CHOSEN (PALETTE->row_selected)
#define COLOR_TICK 0xFFFFFFFF
#define COLOR_TEXT (PALETTE->text)
#define COLOR_DIM (PALETTE->dim)
#define COLOR_EDGE (PALETTE->panel_edge)

/* interface/ and the platform layer */
char **ui_widget_port_multiplayer_levels(short *count, short *xbox_count);
boolean ui_widget_port_multiplayer_level_choose(char const *map_name);
boolean ui_widget_port_cooperative_level_choose(char const *map_name, short difficulty);
boolean ui_widget_port_open_from_top(char const *name);
void event_manager_post_button(short controller_index, short button_index);
void ui_widget_port_go_back_from_top(void);
/* browser_screen.c */
boolean browser_screen_take_create(void);
void browser_screen_open(void);
/* (below) */
void map_screen_go_back(void);

/* ---------- structures */

/* A list entry. level is its index in the multiplayer level list (NONE for
a campaign level); display is its display index (custom_edition_maps.h). */
struct map_entry
{
	short level;
	short display;
};

/* the button bar for the current step */
struct button_bar
{
	char const *labels[MAXIMUM_BUTTONS];
	void (*actions[MAXIMUM_BUTTONS])(void);
	short count;
	char difficulty_label[32];
};

/* ---------- globals */

static struct
{
	boolean active;
	short step;
	short view;
	/* the levels or maps of the open list */
	struct map_entry entries[MAXIMUM_LEVELS];
	short count;
	short selected;
	short first;
	/* the selected row of each step, kept while a later step is open */
	short kind_selected;
	short cooperative_selected;
	short campaign_category_selected;
	short category_selected;
	/* the open PvP category */
	short category;
	short difficulty;
	/* hosting a network game: starts at COOPERATIVE | PVP */
	boolean hosting;
	/* the hosted game was made by Online Games' CREATE GAME, which backing out
	returns to; kept as the lobby is left back to the picker, until the server
	goes (map_screen_server_disposed) */
	boolean from_online_games;
	char **level_names;
	short level_count;
	/* the Xbox map list this was opened over, or NULL */
	struct widget_instance *xbox_list;
	short xbox_count;
	unsigned long opened_time;
	struct overlay_repeat repeat;
	short button_hovered;
} map_screen = { FALSE, STEP_CATEGORIES, VIEW_LIST };

/* ---------- private code */

static void utf8_of(wchar_t const *text, char *out, long size)
{
	overlay_utf8(text, size, out, size);
}

static boolean step_is_list(void)
{
	return map_screen.step == STEP_CAMPAIGN_LEVELS || map_screen.step == STEP_MAPS;
}

static short step_row_count(void)
{
	switch (map_screen.step)
	{
	case STEP_KINDS: return NUMBEROF(kind_rows);
	case STEP_COOPERATIVE_MODES: return NUMBEROF(cooperative_rows);
	default: return NUMBER_OF_CATEGORIES;
	}
}

static short *step_row_selected(void)
{
	switch (map_screen.step)
	{
	case STEP_KINDS: return &map_screen.kind_selected;
	case STEP_COOPERATIVE_MODES: return &map_screen.cooperative_selected;
	case STEP_CAMPAIGN_CATEGORIES: return &map_screen.campaign_category_selected;
	default: return &map_screen.category_selected;
	}
}

/* an entry's display name and description (empty if it has none), each in
a buffer of its own size */
static void entry_text(struct map_entry const *entry, char *name, long name_size, char *description,
	long description_size)
{
	description[0] = 0;
	if (map_screen.step == STEP_MAPS && entry->level < map_screen.xbox_count)
	{
		long strings = tag_loaded('ustr', LEVEL_DESCRIPTIONS);

		csstrncpy(name, entry->level < NUMBEROF(xbox_level_names) ? xbox_level_names[entry->level] : "?",
			(size_t)name_size - 1);
		name[name_size - 1] = 0;
		if (strings != NONE)
			utf8_of(unicode_string_list_get_string(strings, entry->level), description, description_size);
		return;
	}
	utf8_of(custom_edition_maps_name(entry->display), name, name_size);
	utf8_of(custom_edition_maps_description(entry->display), description, description_size);
}

/* whether a multiplayer level is stock (the Xbox's or Halo PC's own) */
static boolean level_vanilla(short level)
{
	return level < map_screen.xbox_count || custom_edition_maps_stock(custom_edition_maps_level_display_index(level));
}

/* the multiplayer levels in a PvP category; returns the count */
static short category_levels(short category, struct map_entry *entries)
{
	short count = 0, level;

	for (level = 0; level < map_screen.level_count && count < MAXIMUM_LEVELS; level++)
	{
		if (level_vanilla(level) == (category == 0))
		{
			entries[count].level = level;
			entries[count].display = custom_edition_maps_level_display_index(level);
			count++;
		}
	}
	return count;
}

/* the campaign levels in a category (stock or custom); entries may be NULL
to just count them */
static short campaign_levels(short category, struct map_entry *entries)
{
	short displays[MAXIMUM_LEVELS];
	short count, index;

	if (category == 0)
	{
		for (index = 0; index < NUMBER_OF_SINGLE_PLAYER_LEVELS; index++)
			displays[index] = custom_edition_maps_display_index(main_get_solo_level_name(index));
		count = NUMBER_OF_SINGLE_PLAYER_LEVELS;
	}
	else
	{
		count = (short)MIN(custom_edition_maps_count(TRUE), MAXIMUM_LEVELS);
		for (index = 0; index < count; index++)
			displays[index] = custom_edition_maps_display_index_of(TRUE, index);
	}
	for (index = 0; index < count && entries; index++)
	{
		entries[index].level = NONE;
		entries[index].display = displays[index];
	}
	return count;
}

static void list_open(short step)
{
	map_screen.step = step;
	map_screen.selected = 0;
	map_screen.first = 0;
}

static short page_size(void)
{
	return map_screen.view == VIEW_GRID ? GRID_COLUMNS * GRID_ROWS : LIST_ROWS;
}

/* scrolls so the selection is on screen */
static void keep_in_view(void)
{
	short page = page_size();
	short step = map_screen.view == VIEW_GRID ? GRID_COLUMNS : 1;

	if (map_screen.selected < map_screen.first)
		map_screen.first = (short)(map_screen.selected - map_screen.selected % step);
	while (map_screen.selected >= map_screen.first + page)
		map_screen.first = (short)(map_screen.first + step);
}

static void leave(void)
{
	map_screen.active = FALSE;
	map_screen.xbox_list = NULL;
	map_screen_go_back();
}

static void pick(void)
{
	if (system_milliseconds() - map_screen.opened_time < OPEN_SETTLE)
		return;
	switch (map_screen.step)
	{
	case STEP_KINDS:
		map_screen.step = map_screen.kind_selected == 0 ? STEP_COOPERATIVE_MODES : STEP_CATEGORIES;
		return;
	case STEP_COOPERATIVE_MODES:
		map_screen.step = STEP_CAMPAIGN_CATEGORIES;
		return;
	case STEP_CAMPAIGN_CATEGORIES:
		map_screen.count = campaign_levels(map_screen.campaign_category_selected, map_screen.entries);
		list_open(STEP_CAMPAIGN_LEVELS);
		return;
	case STEP_CATEGORIES:
		map_screen.category = map_screen.category_selected;
		map_screen.count = category_levels(map_screen.category, map_screen.entries);
		list_open(STEP_MAPS);
		return;
	case STEP_CAMPAIGN_LEVELS:
		if (!map_screen.count ||
			!ui_widget_port_cooperative_level_choose(
				custom_edition_maps_level_name(map_screen.entries[map_screen.selected].display), map_screen.difficulty))
		{
			return;
		}
		/* Server Setup replaces the Xbox list, which picks nothing */
		map_screen.active = FALSE;
		map_screen.xbox_list = NULL;
		ui_widget_port_open_from_top(SERVER_SETUP_SCREEN);
		return;
	default:
		break;
	}
	if (!map_screen.count ||
		!ui_widget_port_multiplayer_level_choose(map_screen.level_names[map_screen.entries[map_screen.selected].level]))
	{
		return;
	}
	map_screen.active = FALSE;
	if (map_screen.xbox_list)
	{
		/* select it in the Xbox list and press A there, so the list goes on as usual */
		map_screen.xbox_list->parameters.list.selected_index = map_screen.entries[map_screen.selected].level;
		map_screen.xbox_list = NULL;
		event_manager_post_button(0, BUTTON_A);
		return;
	}
	ui_widget_port_open_from_top(GAMETYPES_SCREEN);
}

static void back(void)
{
	switch (map_screen.step)
	{
	case STEP_MAPS:
		map_screen.step = STEP_CATEGORIES;
		break;
	case STEP_CAMPAIGN_LEVELS:
		map_screen.step = STEP_CAMPAIGN_CATEGORIES;
		break;
	case STEP_CAMPAIGN_CATEGORIES:
		map_screen.step = STEP_COOPERATIVE_MODES;
		break;
	case STEP_COOPERATIVE_MODES:
		map_screen.step = STEP_KINDS;
		break;
	case STEP_CATEGORIES:
		if (map_screen.hosting)
			map_screen.step = STEP_KINDS;
		else
			leave();
		break;
	default:
		leave();
		break;
	}
}

static void toggle_view(void)
{
	map_screen.view = (short)(map_screen.view == VIEW_LIST ? VIEW_GRID : VIEW_LIST);
	map_screen.first = 0;
	keep_in_view();
}

static void next_difficulty(void)
{
	map_screen.difficulty = (short)((map_screen.difficulty + 1) % NUMBER_OF_GAME_DIFFICULTY_LEVELS);
}

/* the buttons the current step offers */
static void step_buttons(struct button_bar *bar)
{
	bar->count = 0;
	bar->labels[bar->count] = "BACK";
	bar->actions[bar->count++] = back;
	if (step_is_list())
	{
		bar->labels[bar->count] = map_screen.view == VIEW_LIST ? "GRID VIEW" : "LIST VIEW";
		bar->actions[bar->count++] = toggle_view;
	}
	if (map_screen.step == STEP_CAMPAIGN_LEVELS)
	{
		snprintf(bar->difficulty_label, sizeof(bar->difficulty_label), "DIFFICULTY: %s",
			difficulty_names[map_screen.difficulty]);
		bar->labels[bar->count] = bar->difficulty_label;
		bar->actions[bar->count++] = next_difficulty;
	}
}

/* Arrow keys: up and down move one row (or one grid row); left and right
move one card in the grid, or a page in a list. */
static void move(short dx, short dy)
{
	short *selected = step_is_list() ? &map_screen.selected : step_row_selected();
	short count = step_is_list() ? map_screen.count : step_row_count();
	short step;

	if (!step_is_list())
		step = dy;
	else if (map_screen.view == VIEW_LIST)
		step = (short)(dy + dx * LIST_ROWS);
	else
		step = (short)(dx + dy * GRID_COLUMNS);
	*selected = (short)PIN(*selected + step, 0, MAX(0, count - 1));
	keep_in_view();
}

/* the row or card index at a point of the 640x480 layout, or NONE */
static short item_at(short x, short y)
{
	if (!step_is_list() || map_screen.view == VIEW_LIST)
	{
		short row = (short)((y - ROW_Y) / ROW_HEIGHT);
		short rows = step_is_list() ? LIST_ROWS : step_row_count();

		if (x < ROW_X || x >= ROW_X + ROW_WIDTH || y < ROW_Y || row >= rows)
			return NONE;
		return step_is_list() ? (short)(map_screen.first + row) : row;
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


/* draws newline-separated text downward from y (modifies text) */
static void render_lines(char *text, float x, float y, float size, unsigned int color)
{
	char *line = text;

	while (line && *line && y < OVERLAY_FRAME_BOTTOM - 12)
	{
		char *end = strchr(line, '\n');

		if (end)
			*end = 0;
		ui_overlay_text(UI_FONT_REGULAR, size, x, y, UI_ALIGN_LEFT, color, line);
		y += size + 3;
		line = end ? end + 1 : NULL;
	}
}

static void render_step_rows(struct step_row const *rows, short count, short selected)
{
	char description[128];
	short row;

	for (row = 0; row < count; row++)
	{
		float y = (float)(ROW_Y + row * ROW_HEIGHT);

		overlay_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT, row == selected, FALSE);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, ROW_X + 10, y + 4, UI_ALIGN_LEFT, COLOR_TEXT, rows[row].name);
	}
	ui_overlay_text(UI_FONT_BOLD, 15.0f, PREVIEW_X, PREVIEW_Y, UI_ALIGN_LEFT, COLOR_TITLE, rows[selected].name);
	csstrncpy(description, rows[selected].description, sizeof(description) - 1);
	description[sizeof(description) - 1] = 0;
	render_lines(description, PREVIEW_X, PREVIEW_Y + 24, 11.0f, COLOR_DIM);
}

/* VANILLA and CUSTOM with their counts, and a hint when CUSTOM is empty */
static void render_categories(short const *counts, short selected, char const *custom_hint)
{
	short category;
	char text[64];

	for (category = 0; category < NUMBER_OF_CATEGORIES; category++)
	{
		float y = (float)(ROW_Y + category * ROW_HEIGHT);

		overlay_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT, category == selected, FALSE);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, ROW_X + 10, y + 4, UI_ALIGN_LEFT, COLOR_TEXT, category_names[category]);
		snprintf(text, sizeof(text), "%d", counts[category]);
		ui_overlay_text(UI_FONT_REGULAR, 11.0f, ROW_X + ROW_WIDTH - 10, y + 5, UI_ALIGN_RIGHT, COLOR_DIM, text);
	}
	if (!counts[1])
	{
		ui_overlay_text(UI_FONT_REGULAR, 10.0f, ROW_X + 10, ROW_Y + NUMBER_OF_CATEGORIES * ROW_HEIGHT + 14,
			UI_ALIGN_LEFT, COLOR_DIM, custom_hint);
	}
}

static void render_list(void)
{
	struct map_entry const *chosen;
	char name[96], description[512];
	short row;

	for (row = 0; row < LIST_ROWS && map_screen.first + row < map_screen.count; row++)
	{
		float y = (float)(ROW_Y + row * ROW_HEIGHT);
		short index = (short)(map_screen.first + row);

		overlay_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT, index == map_screen.selected, FALSE);
		entry_text(&map_screen.entries[index], name, sizeof(name), description, sizeof(description));
		ui_overlay_text(UI_FONT_BOLD, 11.0f, ROW_X + 10, y + 5, UI_ALIGN_LEFT, COLOR_TEXT, name);
	}
	if (!map_screen.count)
		return;
	chosen = &map_screen.entries[map_screen.selected];
	overlay_map_picture(chosen->display, PREVIEW_X, PREVIEW_Y, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	ui_overlay_outline(PREVIEW_X, PREVIEW_Y, PREVIEW_WIDTH, PREVIEW_HEIGHT, 0, 0.75f, COLOR_EDGE);
	entry_text(chosen, name, sizeof(name), description, sizeof(description));
	ui_overlay_text(UI_FONT_BOLD, 15.0f, PREVIEW_X, PREVIEW_Y + PREVIEW_HEIGHT + 8, UI_ALIGN_LEFT, COLOR_TITLE, name);
	render_lines(description, PREVIEW_X, PREVIEW_Y + PREVIEW_HEIGHT + 30, 10.0f, COLOR_DIM);
}

static void render_grid(void)
{
	char name[96], description[512];
	short slot;

	for (slot = 0; slot < GRID_COLUMNS * GRID_ROWS && map_screen.first + slot < map_screen.count; slot++)
	{
		short index = (short)(map_screen.first + slot);
		short x = (short)(GRID_X + (slot % GRID_COLUMNS) * (CARD_WIDTH + CARD_GAP_X));
		short y = (short)(GRID_Y + (slot / GRID_COLUMNS) * (CARD_HEIGHT + CARD_GAP_Y));

		overlay_map_picture(map_screen.entries[index].display, x, y, CARD_WIDTH, CARD_PICTURE);
		if (index == map_screen.selected)
		{
			ui_overlay_outline(x, y, CARD_WIDTH, CARD_PICTURE, 0, 1.5f, COLOR_TICK);
			overlay_row(x, (float)(y + CARD_PICTURE), CARD_WIDTH, 15, TRUE, FALSE);
		}
		else
			ui_overlay_outline(x, y, CARD_WIDTH, CARD_PICTURE, 0, 0.75f, COLOR_EDGE);
		entry_text(&map_screen.entries[index], name, sizeof(name), description, sizeof(description));
		ui_overlay_text(UI_FONT_BOLD, 10.0f, x + 6, y + CARD_PICTURE + 2, UI_ALIGN_LEFT,
			index == map_screen.selected ? COLOR_TITLE : COLOR_TEXT, name);
	}
}

static char const *step_title(void)
{
	switch (map_screen.step)
	{
	case STEP_KINDS: return "CREATE GAME";
	case STEP_COOPERATIVE_MODES: return "COOPERATIVE";
	case STEP_CAMPAIGN_CATEGORIES: return "CAMPAIGN";
	case STEP_CAMPAIGN_LEVELS: return map_screen.campaign_category_selected ? "CUSTOM CAMPAIGN" : "CAMPAIGN";
	case STEP_CATEGORIES: return map_screen.hosting ? "PVP" : "SELECT MAP";
	default: return category_names[map_screen.category];
	}
}

/* ---------- public code */

/* network_game_server_dispose: the hosted game is gone */
void map_screen_server_disposed(void)
{
	map_screen.from_online_games = FALSE;
}

/* A map list opening (this picker, or Vanilla's stock one: menu_functions.c)
notes whether Online Games' CREATE GAME made the hosted game. */
void map_screen_note_online_games(void)
{
	if (browser_screen_take_create())
		map_screen.from_online_games = TRUE;
}

/* Backs out of a map list: to the screen before it, and on to Online Games
if that made the game. */
void map_screen_go_back(void)
{
	/* (read first: going back ends the hosted game, which clears it) */
	boolean to_online_games = map_screen.from_online_games;

	map_screen.from_online_games = FALSE;
	ui_widget_port_go_back_from_top();
	if (to_online_games)
		browser_screen_open();
}

boolean map_screen_active(void)
{
	return map_screen.active;
}

/* Opens the picker at its first step ("port map select"). Returns FALSE
without the overlay, or in Vanilla, and the menus' own list is used instead. */
boolean map_screen_open(void)
{
	if (!ui_overlay_available() || !overlay_palette_current()->own_screens)
		return FALSE;
	map_screen.xbox_list = NULL;
	map_screen.level_names = ui_widget_port_multiplayer_levels(&map_screen.level_count, &map_screen.xbox_count);
	map_screen.active = TRUE;
	map_screen.opened_time = system_milliseconds();
	map_screen.repeat.held = FALSE;
	map_screen.button_hovered = NONE;
	map_screen.hosting = global_network_game_server_get() != NULL && !network_game_is_splitscreen_local();
	map_screen_note_online_games();
	map_screen.step = map_screen.hosting ? STEP_KINDS : STEP_CATEGORIES;
	map_screen.kind_selected = 0;
	map_screen.cooperative_selected = 0;
	map_screen.campaign_category_selected = 0;
	map_screen.category_selected = 0;
	map_screen.selected = 0;
	map_screen.first = 0;
	map_screen.difficulty = (short)PIN(main_get_difficulty(), 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1);
	/* drop the menu's A that opened us, still queued */
	event_manager_flush();
	return TRUE;
}

/* multiplayer_level_list_initialize: opens the picker over the Xbox map list */
boolean map_screen_open_over_list(struct widget_instance *list)
{
	if (!map_screen_open())
		return FALSE;
	map_screen.xbox_list = list;
	return TRUE;
}

/* The mouse: hovering selects, clicking picks or presses a button, and the
wheel scrolls. */
void map_screen_pointer(struct halo_ui_pointer const *pointer)
{
	short *selected = step_is_list() ? &map_screen.selected : step_row_selected();
	short count = step_is_list() ? map_screen.count : step_row_count();
	struct button_bar bar;
	short item;

	step_buttons(&bar);
	if (pointer->moved)
	{
		map_screen.button_hovered = overlay_button_at(bar.labels, bar.count, (float)ROW_X, OVERLAY_BUTTON_Y,
			pointer->x, pointer->y);
	}
	if (pointer->left_clicks)
	{
		short button = overlay_button_at(bar.labels, bar.count, (float)ROW_X, OVERLAY_BUTTON_Y,
			pointer->click_x, pointer->click_y);

		if (button != NONE)
		{
			bar.actions[button]();
			return;
		}
	}
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
}

void map_screen_process(void)
{
	struct event_record event;
	short dx = 0, dy = 0;

	while (map_screen.active && get_next_event(&event, NONE))
	{
		if (event.type == OVERLAY_EVENT_LEFT_STICK)
		{
			dy = event.data.stick.y == SHORT_MAX ? -1 : event.data.stick.y == SHORT_MIN ? 1 : dy;
			dx = event.data.stick.x == SHORT_MIN ? -1 : event.data.stick.x == SHORT_MAX ? 1 : dx;
		}
		else if (event.type == OVERLAY_EVENT_BUTTON)
		{
			switch (event.data.button.index)
			{
			case _gamepad_binary_button_dpad_up: dy = -1; break;
			case _gamepad_binary_button_dpad_down: dy = 1; break;
			case _gamepad_binary_button_dpad_left: dx = -1; break;
			case _gamepad_binary_button_dpad_right: dx = 1; break;
			case _gamepad_analog_button_a: pick(); break;
			case _gamepad_analog_button_b: back(); break;
			default: break;
			}
		}
	}
	if (overlay_repeat_step(&map_screen.repeat, dx || dy))
		move(dx, dy);
	/* The menus behind get no input while this is open. Once a map is
	picked, the A posted to the Xbox list must get through. */
	if (map_screen.active)
		event_manager_flush();
}

void map_screen_render(void)
{
	struct overlay_button_colors colors;
	struct button_bar bar;

	if (!ui_overlay_available())
		return;
	overlay_screen_frame(step_title(), 37, 17, 30.0f);

	switch (map_screen.step)
	{
	case STEP_KINDS:
		render_step_rows(kind_rows, NUMBEROF(kind_rows), map_screen.kind_selected);
		break;
	case STEP_COOPERATIVE_MODES:
		render_step_rows(cooperative_rows, NUMBEROF(cooperative_rows), map_screen.cooperative_selected);
		break;
	case STEP_CAMPAIGN_CATEGORIES:
	{
		short counts[NUMBER_OF_CATEGORIES];

		counts[0] = campaign_levels(0, NULL);
		counts[1] = campaign_levels(1, NULL);
		render_categories(counts, map_screen.campaign_category_selected,
			"Put Custom Edition campaign maps in the maps folder to play them here.");
		break;
	}
	case STEP_CATEGORIES:
	{
		struct map_entry entries[MAXIMUM_LEVELS];
		short counts[NUMBER_OF_CATEGORIES];

		counts[0] = category_levels(0, entries);
		counts[1] = category_levels(1, entries);
		render_categories(counts, map_screen.category_selected,
			"Put Custom Edition maps in the maps folder to play them here.");
		break;
	}
	default:
		if (map_screen.view == VIEW_GRID)
			render_grid();
		else
			render_list();
		break;
	}

	step_buttons(&bar);
	colors.fill = PALETTE->panel;
	colors.fill_lit = COLOR_CHOSEN;
	colors.edge = COLOR_EDGE;
	colors.text = COLOR_TEXT;
	colors.text_lit = COLOR_TITLE;
	colors.text_disabled = COLOR_DIM;
	colors.radius = PALETTE->radius;
	overlay_buttons_draw(bar.labels, bar.count, (float)ROW_X, OVERLAY_BUTTON_Y, map_screen.button_hovered, 0,
		&colors);
}

#endif
