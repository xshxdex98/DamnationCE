/*
BROWSER_SCREEN.C

The in-game server browser (configure.py
--game-browser): every game on the game list (port/linux/src/browser.c),
on a screen of its own over the menus, as the game's virtual keyboard is
(interface/virtual_keyboard.c): drawn and driven by code, not a widget of
the user interface's tags.

X on the System Link screen opens it (ui_widget.c; the list screen marks
when it is up, ui_widget_game_data_input_functions.c). Up and down pick a
game, left and right turn the page, A joins it through its invite, as a web
page's Join or an invite link would, and B goes back. Once the invite's host
answers, its game shows in the System Link list through the tunnel, to be
picked there as any.
*/

#ifdef HALO_GAME_BROWSER

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cseries/errors.h"
#include "cutscene/cinematics.h"
#include "input/input.h"
#include "interface/event_manager.h"
#include "interface/interface.h"
#include "rasterizer/rasterizer.h"
#include "text/draw_string.h"
#include "bitmaps/bitmap_group.h"
#include "interface/ui_widget.h"
#include "tag_files/tag_groups.h"
#include "game/game.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "saved games/saved_game_files.h"
#include "networking/network_game_globals.h"
#include "../src/browser.h"
#include "../src/ui_overlay.h"
#include "halo_ui_pointer.h"
#include "overlay_screens.h"

/* ---------- constants */

enum
{
	/* the games shown at a time (left and right turn the page) */
	ROWS_PER_PAGE = 7,
	STATUS_DURATION = 6000,
	/* a picked game's host answers this soon, or it is given up on */
	CONNECT_TIMEOUT = 15000,
	/* the screen takes no A this soon after it opens */
	OPEN_SETTLE = 600,
};

/* ui_widget.c owns the same private enum (virtual_keyboard.c keeps a copy) */
enum
{
	_ui_audio_feedback_none,
	_ui_audio_feedback_cursor,
};

/* the game's engines, short (as players say them) to fit the column */
static char const *const engine_names[] =
{
	"", "CTF", "Slayer", "Oddball", "King", "Race",
};

/* the multiplayer maps' names in the menus */
static char const *const map_names[][2] =
{
	{ "beavercreek", "Battle Creek" }, { "bloodgulch", "Blood Gulch" }, { "boardingaction", "Boarding Action" },
	{ "carousel", "Derelict" }, { "chillout", "Chill Out" }, { "damnation", "Damnation" },
	{ "hangemhigh", "Hang 'Em High" }, { "longest", "Longest" }, { "prisoner", "Prisoner" },
	{ "putput", "Chiron TL-34" }, { "ratrace", "Rat Race" }, { "sidewinder", "Sidewinder" }, { "wizard", "Wizard" },
};


/* the list's orders (LT and RT, or the shoulders, step through them) */
enum
{
	SORT_PLAYERS,
	SORT_NAME,
	SORT_MAP,
	SORT_TYPE,

	NUMBER_OF_SORTS
};

/* ---------- globals */

static struct
{
	boolean active;
	short selected;
	short count;
	struct browser_game games[BROWSER_MAXIMUM_GAMES];
	char status[96];
	unsigned long status_time;
	short sort;
	/* a game picked: its invite, while its host's game is waited for */
	boolean connecting;
	char connecting_invite[BROWSER_INVITE_LENGTH + 1];
	char connecting_name[64];
	unsigned long connecting_time;
	/* when the screen opened (the menu's A that opened it picks nothing) */
	unsigned long opened_time;
	struct overlay_repeat repeat;
} browser_screen;

/* ---------- private code */

static void set_status(
	char const *text)
{
	csstrncpy(browser_screen.status, text, sizeof(browser_screen.status) - 1);
	browser_screen.status[sizeof(browser_screen.status) - 1] = 0;
	browser_screen.status_time = system_milliseconds();
}

static char const *map_display_name(
	char const *path)
{
	char const *base = path;
	char const *cursor;
	long index;

	for (cursor = path; *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			base = cursor + 1;
	}
	for (index = 0; index < NUMBEROF(map_names); index++)
	{
		if (!csstrcmp(base, map_names[index][0]))
			return map_names[index][1];
	}
	return base;
}

/* (network_client_manager.c: the game whose host's identifier the invite
starts with, joined once it is advertised) */
long network_game_client_join_invite_host(char const *invite);
boolean create_global_network_game_client(void);
void game_connection_set(short connection);
/* (interface/: the network, as System Link's list starts it, and a game of
this machine's, as its Y makes one) */
boolean ui_online_games_start_network(void);
void ui_online_games_stop_network(void);
/* (the platform layer: the menus' theme) */
char const *config_string(char const *name);
boolean ui_widget_online_games_create_game(void);

/* the first player in the game to be joined or made, with the profile
System Link's Start would pick: the one last used, else the first saved.
(A profile's index is the saved game files' (saved_game_files.c), its valid
bit set: 0 is none, and player_profile_get made of it a profile named for
whichever saved file came first, a game type on a new install.) None saved,
the player keeps the profile it has */
static void join_first_player(
	void)
{
	long profile_index = player_ui_get_player1_last_used_profile_index();
	struct player_profile profile;

	player_ui_local_player_joined_multiplayer_game(0);
	if (profile_index == NONE || !TEST_FLAG(profile_index, _saved_game_file_index_valid_bit))
	{
		long profile_indices[100];
		word profile_count = NUMBEROF(profile_indices);

		player_profiles_enumerate_available_to_local_player_index(0, &profile_count, profile_indices, FALSE);
		profile_index = profile_count ? profile_indices[0] : NONE;
	}
	if (profile_index != NONE && TEST_FLAG(profile_index, _saved_game_file_index_valid_bit) &&
		player_profile_get(profile_index, &profile))
	{
		player_ui_set_active_player_profile(0, profile_index, &profile);
	}
}

/* a game picked: its invite joined (the tunnel to its host), then its game
joined once advertised through it (browser_screen_process) */
static void join_selected(
	void)
{
	struct browser_game const *game;

	if (browser_screen.selected < 0 || browser_screen.selected >= browser_screen.count)
		return;
	game = &browser_screen.games[browser_screen.selected];
	if (!game->open)
	{
		set_status("That game is not accepting players.");
		return;
	}
	/* a network client searching, as System Link's (the advertisement comes
	to it: browser_screen_open started it) */
	if (!global_network_game_client_get())
	{
		if (!create_global_network_game_client())
		{
			set_status("Could not start the network.");
			return;
		}
		game_connection_set(_game_connection_network_client);
	}
	join_first_player();
	if (!browser_join(game->invite))
	{
		set_status("Internet play is off (network.online in config.toml).");
		return;
	}
	browser_screen.connecting = TRUE;
	csstrncpy(browser_screen.connecting_invite, game->invite, sizeof(browser_screen.connecting_invite) - 1);
	browser_screen.connecting_invite[sizeof(browser_screen.connecting_invite) - 1] = 0;
	overlay_utf8(game->name, NUMBEROF(game->name), browser_screen.connecting_name, sizeof(browser_screen.connecting_name));
	browser_screen.connecting_time = system_milliseconds();
}

/* the picked game's host: its game joined once it is advertised, and its
lobby opened */
static void wait_for_host(
	void)
{
	long joined = network_game_client_join_invite_host(browser_screen.connecting_invite);

	if (joined > 0)
	{
		browser_screen.connecting = FALSE;
		browser_screen.active = FALSE;
		ui_widgets_close_all();
		ui_widget_load_by_name_or_tag(
			"ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame\\connected_pregame_screen",
			NONE, NULL, NONE, NONE, NONE, NONE);
	}
	else if (joined < 0)
	{
		browser_screen.connecting = FALSE;
		set_status("That game can't be joined from this version.");
	}
	else if (system_milliseconds() - browser_screen.connecting_time > CONNECT_TIMEOUT)
	{
		browser_screen.connecting = FALSE;
		set_status("The host did not answer.");
	}
}

static long compare_names(
	unsigned short const *a,
	unsigned short const *b)
{
	long index;

	for (index = 0; index < BROWSER_NAME_LENGTH; index++)
	{
		unsigned short x = a[index] >= 'a' && a[index] <= 'z' ? (unsigned short)(a[index] - 32) : a[index];
		unsigned short y = b[index] >= 'a' && b[index] <= 'z' ? (unsigned short)(b[index] - 32) : b[index];

		if (x != y || !x)
			return (long)x - (long)y;
	}
	return 0;
}

static long compare_games(
	struct browser_game const *a,
	struct browser_game const *b)
{
	long order;

	/* (closed games last, whatever the order) */
	if (a->open != b->open)
		return a->open ? -1 : 1;
	switch (browser_screen.sort)
	{
	case SORT_NAME: order = compare_names(a->name, b->name); break;
	case SORT_MAP: order = strcmp(map_display_name(a->map), map_display_name(b->map)); break;
	case SORT_TYPE: order = (long)a->engine * 2 + a->teams - ((long)b->engine * 2 + b->teams); break;
	default: order = (long)b->players - (long)a->players; break;
	}
	return order ? order : compare_names(a->name, b->name);
}

/* the game list's games, in the screen's order; the selection stays on its
game */
static void fetch_games(
	void)
{
	char invite[BROWSER_INVITE_LENGTH + 1];
	short index, other;

	invite[0] = 0;
	if (browser_screen.selected >= 0 && browser_screen.selected < browser_screen.count)
		csstrncpy(invite, browser_screen.games[browser_screen.selected].invite, sizeof(invite) - 1);
	invite[sizeof(invite) - 1] = 0;
	browser_screen.count = (short)browser_get_games(browser_screen.games, BROWSER_MAXIMUM_GAMES);
	/* (insertion: a few dozen games) */
	for (index = 1; index < browser_screen.count; index++)
	{
		struct browser_game game = browser_screen.games[index];

		for (other = index; other > 0 && compare_games(&browser_screen.games[other - 1], &game) > 0; other--)
			browser_screen.games[other] = browser_screen.games[other - 1];
		browser_screen.games[other] = game;
	}
	for (index = 0; invite[0] && index < browser_screen.count; index++)
	{
		if (!strcmp(browser_screen.games[index].invite, invite))
			browser_screen.selected = index;
	}
}

/* B: back to the menu, the search for games ended */
static void leave(
	void)
{
	browser_screen.active = FALSE;
	ui_online_games_stop_network();
}

/* ---------- public code */

boolean browser_screen_active(
	void)
{
	return browser_screen.active;
}

/* the screen opened (the Multiplayer menu's ONLINE GAMES, interface/ui_widget.c) */
void browser_screen_open(
	void)
{
	browser_screen.active = TRUE;
	browser_screen.selected = 0;
	browser_screen.status[0] = 0;
	browser_screen.connecting = FALSE;
	browser_screen.opened_time = system_milliseconds();
	/* (the menu's A, still queued, is not a pick) */
	event_manager_flush();
	/* the network searching, as System Link's list starts it: a game left
	behind (a lobby backed out of) ended */
	if (!ui_online_games_start_network())
		set_status("Could not start the network.");
	fetch_games();
}

/* Y: a game of this machine's, as System Link's Y makes one (the new game's
map chosen next; once it starts, the game list lists it) */
static void create_game(
	void)
{
	join_first_player();
	if (ui_widget_online_games_create_game())
		browser_screen.active = FALSE;
	else
		set_status("Could not create a game.");
}

void browser_screen_process(
	void)
{
	struct event_record event;
	short move = 0;

	fetch_games();
	if (browser_screen.connecting)
		wait_for_host();
	while (browser_screen.active && get_next_event(&event, NONE))
	{
		if (event.type == OVERLAY_EVENT_LEFT_STICK)
		{
			if (event.data.stick.y == SHORT_MAX)
				move = -1;
			else if (event.data.stick.y == SHORT_MIN)
				move = 1;
			else if (event.data.stick.x == SHORT_MIN)
				move = -ROWS_PER_PAGE;
			else if (event.data.stick.x == SHORT_MAX)
				move = ROWS_PER_PAGE;
		}
		else if (event.type == OVERLAY_EVENT_BUTTON)
		{
			switch (event.data.button.index)
			{
			case _gamepad_binary_button_dpad_up: move = -1; break;
			case _gamepad_binary_button_dpad_down: move = 1; break;
			case _gamepad_binary_button_dpad_left: move = -ROWS_PER_PAGE; break;
			case _gamepad_binary_button_dpad_right: move = ROWS_PER_PAGE; break;
			case _gamepad_analog_button_a:
				if (!browser_screen.connecting && system_milliseconds() - browser_screen.opened_time > OPEN_SETTLE)
					join_selected();
				break;
			case _gamepad_binary_button_start:
				browser_open_profile();
				set_status("Opening your profile in the web browser");
				break;
			case _gamepad_analog_button_x:
				fetch_games();
				set_status("Refreshed");
				break;
			case _gamepad_analog_button_y:
				if (!browser_screen.connecting && system_milliseconds() - browser_screen.opened_time > OPEN_SETTLE)
					create_game();
				break;
			case _gamepad_analog_button_left_trigger:
			case _gamepad_analog_button_white:
				browser_screen.sort = (short)((browser_screen.sort + NUMBER_OF_SORTS - 1) % NUMBER_OF_SORTS);
				fetch_games();
				break;
			case _gamepad_analog_button_right_trigger:
			case _gamepad_analog_button_black:
				browser_screen.sort = (short)((browser_screen.sort + 1) % NUMBER_OF_SORTS);
				fetch_games();
				break;
			case _gamepad_analog_button_b:
				/* (B while a host is waited for: the wait given up) */
				if (browser_screen.connecting)
					browser_screen.connecting = FALSE;
				else
					leave();
				break;
			default: break;
			}
		}
	}
	if (overlay_repeat_step(&browser_screen.repeat, move != 0))
	{
		browser_screen.selected = (short)PIN(browser_screen.selected + move, 0,
			MAX(0, browser_screen.count - 1));
	}
	if (browser_screen.selected >= browser_screen.count)
		browser_screen.selected = (short)MAX(0, browser_screen.count - 1);
	/* (the widgets behind take nothing while the browser is up) */
	event_manager_flush();
}

/* ---------- drawing: the Online Games screen (the overlay, ui_overlay.c) */

/* the screen's colors (0xRRGGBBAA) in the menus' themes (display.theme):
Glassed's dark glass over the scene, hairlines and white for what is
chosen, and Vanilla's blues on a screen of its own */
struct browser_palette
{
	boolean glassed;
	unsigned int backdrop, backdrop_bottom, rule, title, panel, panel_edge, panel_head, head, row_selected, row_rule;
	unsigned int text, dim, label, prompt, connecting;
	float radius;
};
static struct browser_palette const glassed_palette =
{
	TRUE, 0x06080C8C, 0x06080C8C, 0xFFFFFF5A, 0xFFFFFFD7, 0x06080C78, 0xFFFFFF46, 0xFFFFFF1A, 0xB4B8BCFF, 0xFFFFFF3E,
	0xFFFFFF14, 0xD2D6DAFF, 0x8C9096FF, 0xA8ACB0FF, 0xD2D6DAFF, 0x06080CE6, 0.0f,
};
static struct browser_palette const vanilla_palette =
{
	FALSE, 0x0B1830FF, 0x03070FFF, 0x2A62C8FF, 0x3D8BFFFF, 0x081530F0, 0x2F6DD0FF, 0x123266FF, 0x7FB0FFFF, 0x2052B0FF,
	0x16294AFF, 0xE6EEFCFF, 0x8FA6C8FF, 0x4AA3FFFF, 0x4AA3FFFF, 0x0A1A36F8, 6.0f,
};
/* the theme's, set as each frame is drawn */
static struct browser_palette const *palette = &glassed_palette;

#define COLOR_RULE (palette->rule)
#define COLOR_TITLE (palette->title)
#define COLOR_PANEL (palette->panel)
#define COLOR_PANEL_EDGE (palette->panel_edge)
#define COLOR_PANEL_HEAD (palette->panel_head)
#define COLOR_HEAD (palette->head)
#define COLOR_ROW_SELECTED (palette->row_selected)
#define COLOR_ROW_RULE (palette->row_rule)
#define COLOR_TEXT (palette->text)
#define COLOR_DIM (palette->dim)
#define COLOR_LABEL (palette->label)
#define COLOR_PROMPT (palette->prompt)
/* (the same in both) */
enum
{
	/* the roster's players of each team */
	COLOR_RED_TEAM = 0xFF6B6BFF,
	COLOR_BLUE_TEAM = 0x6BB0FFFF,
	COLOR_CLOSED = 0xF08A4BFF,
};

/* the layout, in the menus' 640x480: a card for each game down the left,
the chosen game's column at the right */
enum
{
	/* (the glass: from under the title to over the buttons, as the other screens') */
	GLASS_TOP = 66, GLASS_BOTTOM = 446,
	/* the cards: a picture of the map, the name over its map and rules, and
	how full it is */
	LIST_X = 37, LIST_Y = 78, LIST_WIDTH = 368, CARD_HEIGHT = 44,
	CARD_PICTURE_WIDTH = 42, CARD_PICTURE_HEIGHT = 34, FULLNESS_WIDTH = 56,
	/* the chosen game */
	DETAIL_X = 420, DETAIL_Y = 78, DETAIL_WIDTH = 183, DETAIL_PICTURE_HEIGHT = 149,
	ROSTER_COLUMNS = 2, ROSTER_ROWS = 8,
	/* the sort tabs, at the header's right */
	TABS_RIGHT = 603, TABS_Y = 44,
};

/* the row of the list at a point of the 640x480 layout, or NONE */
static short row_at(
	short x,
	short y)
{
	short row = (short)((y - LIST_Y) / CARD_HEIGHT);

	if (x < LIST_X || x >= LIST_X + LIST_WIDTH || y < LIST_Y || row >= ROWS_PER_PAGE)
		return NONE;
	return row;
}

/* the mouse: the row under it is the selected one, a click there joins it,
the wheel turns the page and the right button goes back */
void browser_screen_pointer(
	struct halo_ui_pointer const *pointer)
{
	short page_first = (short)(browser_screen.selected - browser_screen.selected % ROWS_PER_PAGE);
	short row;

	if (browser_screen.connecting)
		return;
	if (pointer->wheel_steps)
	{
		browser_screen.selected = (short)PIN(browser_screen.selected + (pointer->wheel_steps > 0 ? -1 : 1),
			0, MAX(0, browser_screen.count - 1));
	}
	row = pointer->moved ? row_at(pointer->x, pointer->y) : NONE;
	if (row != NONE && page_first + row < browser_screen.count)
		browser_screen.selected = (short)(page_first + row);
	row = pointer->left_clicks ? row_at(pointer->click_x, pointer->click_y) : NONE;
	if (row != NONE && page_first + row < browser_screen.count &&
		system_milliseconds() - browser_screen.opened_time > OPEN_SETTLE)
	{
		browser_screen.selected = (short)(page_first + row);
		join_selected();
	}
	if (pointer->right_clicks)
		leave();
}

static char const *const sort_names[NUMBER_OF_SORTS] = { "PLAYERS", "NAME", "MAP", "TYPE" };

static char const *type_name(
	struct browser_game const *game,
	char *text,
	long size)
{
	char const *engine = game->engine >= 0 && game->engine < NUMBEROF(engine_names) && engine_names[game->engine][0] ?
		engine_names[game->engine] : "Game";

	/* (Capture the Flag is played in teams alone) */
	snprintf(text, (size_t)size, "%s%s", game->teams && game->engine != 1 ? "Team " : "", engine);
	return text;
}

/* a game's map's picture, edged */
static void map_picture(
	char const *map,
	float x,
	float y,
	float width,
	float height)
{
	overlay_map_picture(overlay_map_display_index(map), x, y, width, height);
	ui_overlay_outline(x, y, width, height, 0, 0.75f, COLOR_PANEL_EDGE);
}

/* the header: the title, how many are playing, and the sort tabs (the
order shown bright and underlined) */
static void render_header(
	long players)
{
	char text[96];
	float x = TABS_RIGHT;
	short sort;

	ui_overlay_text(UI_FONT_BOLD, 30.0f, 37, 15, UI_ALIGN_LEFT, COLOR_TITLE, "ONLINE");
	snprintf(text, sizeof(text), "%d %s  \xC2\xB7  %ld %s", browser_screen.count, browser_screen.count == 1 ? "GAME" : "GAMES",
		players, players == 1 ? "PLAYER" : "PLAYERS");
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, 39, 50, UI_ALIGN_LEFT, COLOR_DIM, text);
	for (sort = NUMBER_OF_SORTS - 1; sort >= 0; sort--)
	{
		boolean shown = sort == browser_screen.sort;
		float width = ui_overlay_text_width(UI_FONT_BOLD, 9.0f, sort_names[sort]);

		x -= width;
		ui_overlay_text(UI_FONT_BOLD, 9.0f, x, TABS_Y, UI_ALIGN_LEFT, shown ? COLOR_TITLE : COLOR_DIM, sort_names[sort]);
		if (shown)
			ui_overlay_rect(x, TABS_Y + 13, width, 1.0f, 0, COLOR_TITLE);
		x -= 14;
	}
}

/* a game's card: its map's picture, its name over its map and rules, and how
full it is, as a number over a bar */
static void render_card(
	struct browser_game const *game,
	float y,
	boolean chosen)
{
	char name[64], line[96], rules[32];
	unsigned int color = game->open ? COLOR_TEXT : COLOR_DIM;
	float right = LIST_X + LIST_WIDTH - 10;
	float filled = game->maximum_players > 0 ? (float)game->players / (float)game->maximum_players : 0.0f;

	if (chosen)
	{
		ui_overlay_rect(LIST_X, y + 1, LIST_WIDTH, CARD_HEIGHT - 2, palette->radius, COLOR_ROW_SELECTED);
		if (palette->glassed)
			ui_overlay_rect(LIST_X, y + 1, 1.5f, CARD_HEIGHT - 2, 0, 0xFFFFFFFF);
	}
	map_picture(game->map, LIST_X + 7, y + 5, CARD_PICTURE_WIDTH, CARD_PICTURE_HEIGHT);
	overlay_utf8(game->name, NUMBEROF(game->name), name, sizeof(name));
	ui_overlay_text(UI_FONT_BOLD, 11.0f, LIST_X + 58, y + 7, UI_ALIGN_LEFT, chosen ? COLOR_TITLE : color, name);
	snprintf(line, sizeof(line), "%s  \xC2\xB7  %s", map_display_name(game->map), type_name(game, rules, sizeof(rules)));
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, LIST_X + 58, y + 24, UI_ALIGN_LEFT, COLOR_DIM, line);

	snprintf(line, sizeof(line), "%d/%d", game->players, game->maximum_players);
	ui_overlay_text(UI_FONT_BOLD, 11.0f, right, y + 7, UI_ALIGN_RIGHT, game->open ? color : COLOR_CLOSED, line);
	ui_overlay_rect(right - FULLNESS_WIDTH, y + 27, FULLNESS_WIDTH, 2.0f, 0, COLOR_ROW_RULE);
	ui_overlay_rect(right - FULLNESS_WIDTH, y + 27, FULLNESS_WIDTH * MIN(filled, 1.0f), 2.0f, 0,
		game->open ? COLOR_TEXT : COLOR_CLOSED);
}

/* the chosen game's column: its map, its settings and who is in it (the
host's roster, when it sends one; the last place says how many more) */
static void render_details(
	struct browser_game const *game)
{
	char name[64], text[64];
	float y = DETAIL_Y + DETAIL_PICTURE_HEIGHT + 8;
	long index;

	ui_overlay_rect(DETAIL_X - 6, DETAIL_Y - 4, DETAIL_WIDTH + 12, GLASS_BOTTOM - DETAIL_Y - 6, palette->radius, COLOR_PANEL);
	if (!game)
		return;
	map_picture(game->map, DETAIL_X, DETAIL_Y, DETAIL_WIDTH, DETAIL_PICTURE_HEIGHT);
	overlay_utf8(game->name, NUMBEROF(game->name), name, sizeof(name));
	ui_overlay_text(UI_FONT_BOLD, 13.0f, DETAIL_X, y, UI_ALIGN_LEFT, COLOR_TITLE, name);
	y += 20;
#define DETAIL_LINE(label, value) \
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, DETAIL_X, y, UI_ALIGN_LEFT, COLOR_DIM, label); \
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, DETAIL_X + DETAIL_WIDTH, y, UI_ALIGN_RIGHT, COLOR_TEXT, value); \
	y += 13;
	DETAIL_LINE("Status", game->open ? "Accepting players" : "In progress");
	DETAIL_LINE("Rules", type_name(game, text, sizeof(text)));
	if (game->score_limit)
	{
		snprintf(text, sizeof(text), "%d", game->score_limit);
		DETAIL_LINE("Score limit", text);
	}
#undef DETAIL_LINE

	y += 4;
	ui_overlay_rect(DETAIL_X, y, DETAIL_WIDTH, 0.75f, 0, COLOR_ROW_RULE);
	y += 5;
	if (!game->players && !game->roster_count)
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, DETAIL_X, y, UI_ALIGN_LEFT, COLOR_DIM, "No one yet");
	else if (!game->roster_count)
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, DETAIL_X, y, UI_ALIGN_LEFT, COLOR_DIM, "This host doesn't share names");
	else
	{
		long kept = game->roster_count < BROWSER_LISTED_ROSTER ? game->roster_count : BROWSER_LISTED_ROSTER;
		long places = ROSTER_COLUMNS * ROSTER_ROWS;
		long shown = game->roster_count > places ? places - 1 : kept;
		float column_width = (float)DETAIL_WIDTH / ROSTER_COLUMNS;

		for (index = 0; index < shown; index++)
		{
			struct browser_roster_player const *player = &game->roster[index];
			unsigned long color = !game->teams || player->team < 0 ? COLOR_TEXT :
				player->team == 0 ? COLOR_RED_TEAM : COLOR_BLUE_TEAM;

			overlay_utf8(player->name, NUMBEROF(player->name), name, sizeof(name));
			ui_overlay_text(UI_FONT_REGULAR, 9.0f, DETAIL_X + (index / ROSTER_ROWS) * column_width,
				y + (index % ROSTER_ROWS) * 11, UI_ALIGN_LEFT, color, name);
		}
		if (game->roster_count > shown)
		{
			snprintf(text, sizeof(text), "+%d more", game->roster_count - (int)shown);
			ui_overlay_text(UI_FONT_REGULAR, 9.0f, DETAIL_X + (shown / ROSTER_ROWS) * column_width,
				y + (shown % ROSTER_ROWS) * 11, UI_ALIGN_LEFT, COLOR_DIM, text);
		}
	}
}

void browser_screen_render(
	void)
{
	short page_first, page_count, row;
	long players = 0, index;
	char text[64];
	float x, width, margin = (float)((halo_screen_width() - 640) / 2 + 2);
	struct browser_game const *selected = browser_screen.count ? &browser_screen.games[browser_screen.selected] : NULL;

	if (!ui_overlay_available())
		return;
	for (index = 0; index < browser_screen.count; index++)
		players += browser_screen.games[index].players;

	/* the screen, its widescreen margins too: Glassed's glass over the
	scene, or Vanilla's screen of its own */
	palette = strcmp(config_string("display.theme"), "vanilla") ? &glassed_palette : &vanilla_palette;
	if (palette->glassed)
	{
		ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, GLASS_BOTTOM - GLASS_TOP, 0, palette->backdrop);
		ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, 0.75f, 0, COLOR_RULE);
	}
	else
	{
		ui_overlay_gradient(-margin, 0, 640 + 2 * margin, 480, 0, palette->backdrop, palette->backdrop_bottom);
		ui_overlay_rect(-margin, GLASS_TOP, 640 + 2 * margin, 1.0f, 0, COLOR_RULE);
	}
	render_header(players);

	/* the cards */
	page_first = (short)(browser_screen.selected - browser_screen.selected % ROWS_PER_PAGE);
	page_count = (short)MAX(1, (browser_screen.count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
	if (!palette->glassed)
		ui_overlay_rect(LIST_X - 4, LIST_Y - 4, LIST_WIDTH + 8, ROWS_PER_PAGE * CARD_HEIGHT + 8, palette->radius, COLOR_PANEL);
	if (!browser_screen.count)
	{
		ui_overlay_text(UI_FONT_BOLD, 12.0f, LIST_X + 10, LIST_Y + 20, UI_ALIGN_LEFT, COLOR_TEXT, "No games right now");
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, LIST_X + 10, LIST_Y + 38, UI_ALIGN_LEFT, COLOR_DIM,
			"Host one with Create Game, and it shows here for everyone.");
	}
	for (row = 0; row < ROWS_PER_PAGE && page_first + row < browser_screen.count; row++)
	{
		render_card(&browser_screen.games[page_first + row], (float)(LIST_Y + row * CARD_HEIGHT),
			page_first + row == browser_screen.selected);
	}
	if (page_count > 1)
	{
		snprintf(text, sizeof(text), "\xE2\x80\xB9  %d / %d  \xE2\x80\xBA", page_first / ROWS_PER_PAGE + 1, page_count);
		ui_overlay_text(UI_FONT_BOLD, 9.0f, LIST_X + LIST_WIDTH, LIST_Y + ROWS_PER_PAGE * CARD_HEIGHT + 6, UI_ALIGN_RIGHT,
			COLOR_DIM, text);
	}
	render_details(selected);

	/* the buttons */
	ui_overlay_rect(-margin, GLASS_BOTTOM - 0.75f, 640 + 2 * margin, 0.75f, 0, COLOR_RULE);
	width = overlay_prompt_width(UI_BUTTON_A, "=JOIN") + overlay_prompt_width(UI_BUTTON_B, "=BACK") +
		overlay_prompt_width(UI_BUTTON_X, "=REFRESH") + overlay_prompt_width(UI_BUTTON_Y, "=CREATE GAME") +
		overlay_prompt_width(UI_BUTTON_LEFT_TRIGGER, "") + overlay_prompt_width(UI_BUTTON_RIGHT_TRIGGER, "=SORT") - 20 - 3;
	x = 37;
	if (!palette->glassed)
		x = 320 - width / 2;
	x = overlay_prompt(UI_BUTTON_A, "=JOIN", x, COLOR_PROMPT);
	x = overlay_prompt(UI_BUTTON_B, "=BACK", x, COLOR_PROMPT);
	x = overlay_prompt(UI_BUTTON_X, "=REFRESH", x, COLOR_PROMPT);
	x = overlay_prompt(UI_BUTTON_Y, "=CREATE GAME", x, COLOR_PROMPT);
	x += ui_overlay_button(UI_BUTTON_LEFT_TRIGGER, 15.0f, x, 455.0f, 0xFFFFFFFF);
	overlay_prompt(UI_BUTTON_RIGHT_TRIGGER, "=SORT", x, COLOR_PROMPT);

	if (browser_screen.connecting)
	{
		char line[160];
		long dots = (long)((system_milliseconds() - browser_screen.connecting_time) / 400 % 4);

		snprintf(line, sizeof(line), "Connecting to %s%.*s", browser_screen.connecting_name, (int)dots, "...");
		ui_overlay_rect(170, 200, 300, 64, palette->radius, palette->connecting);
		ui_overlay_outline(170, 200, 300, 64, palette->radius, 0.75f, COLOR_PANEL_EDGE);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, 320, 212, UI_ALIGN_CENTER, 0xFFFFFFFF, line);
		x = 320 - (ui_overlay_button_width(UI_BUTTON_B, 13.0f) + ui_overlay_text_width(UI_FONT_BOLD, 10.0f, "=CANCEL")) / 2;
		x += ui_overlay_button(UI_BUTTON_B, 13.0f, x, 236, 0xFFFFFFFF) + 3;
		ui_overlay_text(UI_FONT_BOLD, 10.0f, x, 237.5f, UI_ALIGN_LEFT, COLOR_PROMPT, "=CANCEL");
	}
	else if (browser_screen.status[0] && system_milliseconds() - browser_screen.status_time < STATUS_DURATION)
		ui_overlay_text(UI_FONT_BOLD, 9.0f, TABS_RIGHT, TABS_Y + 20, UI_ALIGN_RIGHT, COLOR_CLOSED, browser_screen.status);
}

#endif
