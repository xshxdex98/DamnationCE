/*
MAP_SCREEN.C

The map picker of a game being set up (configure.py --game-browser, as the
overlay it draws on): a screen of its own over the menus, drawn and driven
by code as Online Games is (browser_screen.c), in place of the PC menus' map
list, in either theme (display.theme: Glassed's glass over the scene, or
Vanilla's blues).

Hosting a game over the network, it opens on its kinds: COOPERATIVE, the
campaign with every player in it (a network game on a campaign map, which no
game engine runs: game.c, players.c), and PVP. COOPERATIVE opens its modes,
CAMPAIGN alone for now; CAMPAIGN opens the campaign's levels, at the
difficulty X steps through, and A on one makes it the game's
(ui_widget_port_cooperative_level_choose) and opens Server Setup (its name,
players and listing; then the lobby, as a PvP game's). PVP opens two
categories, VANILLA (the Xbox's 13 levels and Halo PC's own six) and CUSTOM
(the other Custom Edition maps in the maps folder, custom_edition_maps.c); a
category opens its maps, and A on one picks it, as the PC menus' list does,
and opens the game types that follow it. Y turns a list of levels or maps
between a list, with the chosen one's picture and description beside it,
and a grid of cards, each picture over its name. B goes back a step. Every
time it opens it starts again from its first step.

It opens over two screens. The PC menus' Map screen runs "port map select"
as it is made: a map picked there is chosen as its list would choose it,
and the game types after it open. The Xbox's map list (Online Games' Create
Game, split screen, a game's next map) opens it as it is made
(multiplayer_level_list_initialize): a map picked there becomes the list's
choice, and the list is given an A, so whatever it does next it does; a co-op
level opens Server Setup in its place. B on its first step leaves the screen
under it too.
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
	/* (event_manager_post_button's buttons) */
	BUTTON_A = 0,

	/* the screen takes no A this soon after it opens */
	OPEN_SETTLE = 600,

	NUMBER_OF_CATEGORIES = 2,
	/* (the Xbox's 13 and custom_edition_maps.c's most) */
	MAXIMUM_LEVELS = 16 + 1024,
};

/* the screen's steps */
enum
{
	/* (hosting over the network) COOPERATIVE or PVP */
	STEP_KINDS,
	/* (COOPERATIVE) its modes */
	STEP_COOPERATIVE_MODES,
	/* (CAMPAIGN) the campaign's levels */
	STEP_CAMPAIGN_LEVELS,
	/* (PVP, or the Xbox's map list) VANILLA or CUSTOM */
	STEP_CATEGORIES,
	/* (a category) its maps */
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
	GLASS_TOP = 66, GLASS_BOTTOM = 446,
	/* the steps' rows, and a list's: down the left */
	ROW_X = 37, ROW_Y = 80, ROW_WIDTH = 300, ROW_HEIGHT = 22, LIST_ROWS = 15,
	/* the chosen level or map, or a row's description, at the right */
	PREVIEW_X = 362, PREVIEW_Y = 80, PREVIEW_WIDTH = 240, PREVIEW_HEIGHT = 196,
	/* the grid's cards */
	GRID_X = 37, GRID_Y = 80, GRID_COLUMNS = 4, GRID_ROWS = 3, CARD_WIDTH = 132, CARD_PICTURE = 107,
	CARD_HEIGHT = 124, CARD_GAP_X = 13, CARD_GAP_Y = 0,
};

/* the Xbox levels' descriptions and names, in the game's order of them
(ui_widget_event_handler_functions.c), and the screens opened next */
#define LEVEL_DESCRIPTIONS "pc\\main_menu\\multiplayer_type_select\\mp_map_select\\map_data"
#define GAMETYPES_SCREEN "pc\\main_menu\\multiplayer_type_select\\connected\\gametype_select_screen_wrapper"
#define SERVER_SETUP_SCREEN "pc\\main_menu\\multiplayer_type_select\\server_settings\\server_settings_screen"
static char const *const xbox_level_names[] =
{
	"Battle Creek", "Sidewinder", "Damnation", "Rat Race", "Prisoner", "Hang 'Em High", "Chill Out",
	"Derelict", "Boarding Action", "Blood Gulch", "Wizard", "Chiron TL-34", "Longest",
};
static char const *const category_names[NUMBER_OF_CATEGORIES] = { "VANILLA", "CUSTOM" };
static char const *const difficulty_names[NUMBER_OF_GAME_DIFFICULTY_LEVELS] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

/* the rows of the kinds' and the cooperative modes' steps, with what each
is, shown beside them */
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

/* the screen's colors (0xRRGGBBAA) in the menus' themes (browser_screen.c's
palettes): Glassed's dark glass, hairlines and white for what is chosen,
and Vanilla's blues on a screen of its own */
struct map_palette
{
	boolean glassed;
	unsigned int backdrop, backdrop_bottom, rule, title, chosen, tick, text, dim, edge;
};
static struct map_palette const glassed_palette =
{
	TRUE, 0x06080C8C, 0x06080C8C, 0xFFFFFF5A, 0xFFFFFFD7, 0xFFFFFF3E, 0xFFFFFFFF, 0xD2D6DAFF, 0x8C9096FF, 0xFFFFFF46,
};
static struct map_palette const vanilla_palette =
{
	FALSE, 0x0B1830FF, 0x03070FFF, 0x2A62C8FF, 0x3D8BFFFF, 0x2052B0FF, 0x7FB0FFFF, 0xE6EEFCFF, 0x8FA6C8FF, 0x2F6DD0FF,
};
/* the theme's, set as each frame is drawn */
static struct map_palette const *palette = &glassed_palette;

#define COLOR_RULE (palette->rule)
#define COLOR_TITLE (palette->title)
#define COLOR_CHOSEN (palette->chosen)
#define COLOR_TICK (palette->tick)
#define COLOR_TEXT (palette->text)
#define COLOR_DIM (palette->dim)
#define COLOR_EDGE (palette->edge)

/* (interface/: the multiplayer levels, the map chosen as the PC menus'
list chooses it, a co-op game's level, and the screens opened and left) */
char **ui_widget_port_multiplayer_levels(short *count, short *xbox_count);
boolean ui_widget_port_multiplayer_level_choose(char const *map_name);
boolean ui_widget_port_cooperative_level_choose(short level, short difficulty);
boolean ui_widget_port_open_from_top(char const *name);
void event_manager_post_button(short controller_index, short button_index);
char const *config_string(char const *name);
void ui_widget_port_go_back_from_top(void);

/* ---------- structures */

/* a level or map of a list: its index in the multiplayer level list (a
map), or the campaign level, and its display index (custom_edition_maps.h) */
struct map_entry
{
	short level;
	short display;
};

/* ---------- globals */

static struct
{
	boolean active;
	short step;
	short view;
	/* the shown list's levels or maps */
	struct map_entry entries[MAXIMUM_LEVELS];
	short count;
	short selected;
	short first;
	/* the row chosen on each step of rows, kept while a later step is open */
	short kind_selected;
	short cooperative_selected;
	short category_selected;
	/* the open category, and the campaign's difficulty */
	short category;
	short difficulty;
	/* hosting over the network: the kinds come first */
	boolean hosting;
	char **level_names;
	short level_count;
	/* the Xbox's map list it is open over, which picks, or NULL */
	struct widget_instance *xbox_list;
	short xbox_count;
	unsigned long opened_time;
	struct overlay_repeat repeat;
} map_screen = { FALSE, STEP_CATEGORIES, VIEW_LIST };

/* ---------- private code */

/* a string, 0-terminated, as UTF-8 */
static void utf8_of(wchar_t const *text, char *out, long size)
{
	overlay_utf8(text, size, out, size);
}

static boolean step_is_list(void)
{
	return map_screen.step == STEP_CAMPAIGN_LEVELS || map_screen.step == STEP_MAPS;
}

/* a step of rows: its rows' count, and the one chosen */
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
	default: return &map_screen.category_selected;
	}
}

/* a level's or map's name in the menus, and its description (empty if none) */
static void entry_text(struct map_entry const *entry, char *name, char *description, long size)
{
	description[0] = 0;
	if (map_screen.step == STEP_MAPS && entry->level < map_screen.xbox_count)
	{
		long strings = tag_loaded('ustr', LEVEL_DESCRIPTIONS);

		csstrncpy(name, entry->level < NUMBEROF(xbox_level_names) ? xbox_level_names[entry->level] : "?",
			(size_t)size - 1);
		name[size - 1] = 0;
		if (strings != NONE)
			utf8_of(unicode_string_list_get_string(strings, entry->level), description, size);
		return;
	}
	utf8_of(custom_edition_maps_name(entry->display), name, size);
	utf8_of(custom_edition_maps_description(entry->display), description, size);
}

/* whether a level is one of the stock ones: the Xbox's, or Halo PC's own */
static boolean level_vanilla(short level)
{
	return level < map_screen.xbox_count || custom_edition_maps_stock(custom_edition_maps_level_display_index(level));
}

/* the level list's levels in a category: the stock ones, or the rest */
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

static void list_open(short step)
{
	map_screen.step = step;
	map_screen.selected = 0;
	map_screen.first = 0;
}

static void category_open(short category)
{
	map_screen.category = category;
	map_screen.count = category_levels(category, map_screen.entries);
	list_open(STEP_MAPS);
}

static void campaign_open(void)
{
	short level;

	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
	{
		map_screen.entries[level].level = level;
		map_screen.entries[level].display = custom_edition_maps_display_index(main_get_solo_level_name(level));
	}
	map_screen.count = NUMBER_OF_SINGLE_PLAYER_LEVELS;
	list_open(STEP_CAMPAIGN_LEVELS);
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

static void leave(void)
{
	map_screen.active = FALSE;
	map_screen.xbox_list = NULL;
	ui_widget_port_go_back_from_top();
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
		campaign_open();
		return;
	case STEP_CATEGORIES:
		category_open(map_screen.category_selected);
		return;
	case STEP_CAMPAIGN_LEVELS:
		if (!ui_widget_port_cooperative_level_choose(map_screen.entries[map_screen.selected].level,
			map_screen.difficulty))
		{
			return;
		}
		/* (the Xbox's list, if it is open over one, picks nothing: Server Setup
		takes its place) */
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
		/* (the list's A: its own "multiplayer level select", and what it opens) */
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

/* a move: up and down by one (a grid's row), left and right a grid's card
or a list's page */
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

/* the row or card at a point of the 640x480 layout (its index in what is
shown), or NONE */
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

static void chosen_row(float x, float y, float width, float height)
{
	ui_overlay_rect(x, y, width, height, 0, COLOR_CHOSEN);
	ui_overlay_rect(x, y, 1.5f, height, 0, COLOR_TICK);
}

/* lines of text, as written, down from y */
static void render_lines(char *text, float x, float y, float size, unsigned int color)
{
	char *line = text;

	while (line && *line && y < GLASS_BOTTOM - 12)
	{
		char *end = strchr(line, '\n');

		if (end)
			*end = 0;
		ui_overlay_text(UI_FONT_REGULAR, size, x, y, UI_ALIGN_LEFT, color, line);
		y += size + 3;
		line = end ? end + 1 : NULL;
	}
}

/* the kinds' or the cooperative modes' rows, the chosen one's description beside them */
static void render_step_rows(struct step_row const *rows, short count, short selected)
{
	char description[128];
	short row;

	for (row = 0; row < count; row++)
	{
		float y = (float)(ROW_Y + row * ROW_HEIGHT);

		if (row == selected)
			chosen_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, ROW_X + 10, y + 4, UI_ALIGN_LEFT, COLOR_TEXT, rows[row].name);
	}
	ui_overlay_text(UI_FONT_BOLD, 15.0f, PREVIEW_X, PREVIEW_Y, UI_ALIGN_LEFT, COLOR_TITLE, rows[selected].name);
	csstrncpy(description, rows[selected].description, sizeof(description) - 1);
	description[sizeof(description) - 1] = 0;
	render_lines(description, PREVIEW_X, PREVIEW_Y + 24, 11.0f, COLOR_DIM);
}

static void render_categories(void)
{
	struct map_entry entries[MAXIMUM_LEVELS];
	short category;
	char text[64];

	for (category = 0; category < NUMBER_OF_CATEGORIES; category++)
	{
		float y = (float)(ROW_Y + category * ROW_HEIGHT);
		short count = category_levels(category, entries);

		if (category == map_screen.category_selected)
			chosen_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, ROW_X + 10, y + 4, UI_ALIGN_LEFT, COLOR_TEXT, category_names[category]);
		snprintf(text, sizeof(text), "%d", count);
		ui_overlay_text(UI_FONT_REGULAR, 11.0f, ROW_X + ROW_WIDTH - 10, y + 5, UI_ALIGN_RIGHT, COLOR_DIM, text);
	}
	if (!category_levels(1, entries))
	{
		ui_overlay_text(UI_FONT_REGULAR, 10.0f, ROW_X + 10, ROW_Y + NUMBER_OF_CATEGORIES * ROW_HEIGHT + 14,
			UI_ALIGN_LEFT, COLOR_DIM, "Put Custom Edition maps in the maps folder to play them here.");
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

		if (index == map_screen.selected)
			chosen_row(ROW_X, y, ROW_WIDTH, ROW_HEIGHT);
		entry_text(&map_screen.entries[index], name, description, sizeof(name));
		ui_overlay_text(UI_FONT_BOLD, 11.0f, ROW_X + 10, y + 5, UI_ALIGN_LEFT, COLOR_TEXT, name);
	}
	if (!map_screen.count)
		return;
	/* the chosen level or map, at the right */
	chosen = &map_screen.entries[map_screen.selected];
	overlay_map_picture(chosen->display, PREVIEW_X, PREVIEW_Y, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	ui_overlay_outline(PREVIEW_X, PREVIEW_Y, PREVIEW_WIDTH, PREVIEW_HEIGHT, 0, 0.75f, COLOR_EDGE);
	entry_text(chosen, name, description, sizeof(description));
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
			chosen_row(x, (float)(y + CARD_PICTURE), CARD_WIDTH, 15);
		}
		else
			ui_overlay_outline(x, y, CARD_WIDTH, CARD_PICTURE, 0, 0.75f, COLOR_EDGE);
		entry_text(&map_screen.entries[index], name, description, sizeof(name));
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
	case STEP_CAMPAIGN_LEVELS: return "CAMPAIGN";
	case STEP_CATEGORIES: return map_screen.hosting ? "PVP" : "SELECT MAP";
	default: return category_names[map_screen.category];
	}
}

/* ---------- public code */

boolean map_screen_active(void)
{
	return map_screen.active;
}

/* "port map select": opened over the Map screen, from its first step; FALSE
(its own list then) without the overlay */
boolean map_screen_open(void)
{
	if (!ui_overlay_available())
		return FALSE;
	map_screen.xbox_list = NULL;
	map_screen.level_names = ui_widget_port_multiplayer_levels(&map_screen.level_count, &map_screen.xbox_count);
	map_screen.active = TRUE;
	map_screen.opened_time = system_milliseconds();
	map_screen.repeat.held = FALSE;
	/* (a game set up before, backed out of, is not where this one starts) */
	map_screen.hosting = global_network_game_server_get() != NULL && !network_game_is_splitscreen_local();
	map_screen.step = map_screen.hosting ? STEP_KINDS : STEP_CATEGORIES;
	map_screen.kind_selected = 0;
	map_screen.cooperative_selected = 0;
	map_screen.category_selected = 0;
	map_screen.selected = 0;
	map_screen.first = 0;
	map_screen.difficulty = (short)PIN(main_get_difficulty(), 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1);
	/* (the menu's A, still queued, is not a pick) */
	event_manager_flush();
	return TRUE;
}

/* (multiplayer_level_list_initialize) the Xbox's map list made: this opened
over it, to pick through it; from its first step, as over the Map screen
(Online Games' Create Game opens that list: the kinds; split screen: the
categories) */
boolean map_screen_open_over_list(struct widget_instance *list)
{
	if (!map_screen_open())
		return FALSE;
	map_screen.xbox_list = list;
	return TRUE;
}

/* the mouse: what is under it is chosen, a click picks it, the wheel moves
and the right button goes back */
void map_screen_pointer(struct halo_ui_pointer const *pointer)
{
	short *selected = step_is_list() ? &map_screen.selected : step_row_selected();
	short count = step_is_list() ? map_screen.count : step_row_count();
	short item;

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
			case _gamepad_analog_button_a:
			case _gamepad_binary_button_start:
				pick();
				break;
			case _gamepad_analog_button_x:
				if (map_screen.step == STEP_CAMPAIGN_LEVELS)
					map_screen.difficulty = (short)((map_screen.difficulty + 1) % NUMBER_OF_GAME_DIFFICULTY_LEVELS);
				break;
			case _gamepad_analog_button_y:
				if (step_is_list())
				{
					map_screen.view = (short)(map_screen.view == VIEW_LIST ? VIEW_GRID : VIEW_LIST);
					map_screen.first = 0;
					keep_in_view();
				}
				break;
			case _gamepad_analog_button_b:
				back();
				break;
			default: break;
			}
		}
	}
	if (overlay_repeat_step(&map_screen.repeat, dx || dy))
		move(dx, dy);
	/* (the widgets behind take nothing while this is up; once a map is
	picked, the A given to the Xbox's list goes through) */
	if (map_screen.active)
		event_manager_flush();
}

void map_screen_render(void)
{
	float margin = (float)((halo_screen_width() - 640) / 2 + 2);
	float x;

	if (!ui_overlay_available())
		return;
	/* the screen, its widescreen margins too: Glassed's glass over the
	scene, or Vanilla's screen of its own */
	palette = strcmp(config_string("display.theme"), "vanilla") ? &glassed_palette : &vanilla_palette;
	if (palette->glassed)
		ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, GLASS_BOTTOM - GLASS_TOP, 0, palette->backdrop);
	else
		ui_overlay_gradient(-margin, 0, 640 + 2 * margin, 480, 0, palette->backdrop, palette->backdrop_bottom);
	ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, 0.75f, 0, COLOR_RULE);
	ui_overlay_rect(-margin, GLASS_BOTTOM - 0.75f, 640 + 2 * margin, 0.75f, 0, COLOR_RULE);
	ui_overlay_text(UI_FONT_BOLD, 30.0f, 37, 17, UI_ALIGN_LEFT, COLOR_TITLE, step_title());
	if (map_screen.step == STEP_CAMPAIGN_LEVELS)
	{
		char text[32];

		snprintf(text, sizeof(text), "DIFFICULTY: %s", difficulty_names[map_screen.difficulty]);
		ui_overlay_text(UI_FONT_BOLD, 13.0f, 603, 30, UI_ALIGN_RIGHT, COLOR_TEXT, text);
	}

	switch (map_screen.step)
	{
	case STEP_KINDS:
		render_step_rows(kind_rows, NUMBEROF(kind_rows), map_screen.kind_selected);
		break;
	case STEP_COOPERATIVE_MODES:
		render_step_rows(cooperative_rows, NUMBEROF(cooperative_rows), map_screen.cooperative_selected);
		break;
	case STEP_CATEGORIES:
		render_categories();
		break;
	default:
		if (map_screen.view == VIEW_GRID)
			render_grid();
		else
			render_list();
		break;
	}

	x = 37;
	x = overlay_prompt(UI_BUTTON_A, "=SELECT", x, COLOR_TEXT);
	x = overlay_prompt(UI_BUTTON_B, "=BACK", x, COLOR_TEXT);
	if (step_is_list())
		x = overlay_prompt(UI_BUTTON_Y, map_screen.view == VIEW_LIST ? "=GRID VIEW" : "=LIST VIEW", x, COLOR_TEXT);
	if (map_screen.step == STEP_CAMPAIGN_LEVELS)
		overlay_prompt(UI_BUTTON_X, "=DIFFICULTY", x, COLOR_TEXT);
}

#endif
