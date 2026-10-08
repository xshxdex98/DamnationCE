/*
BROWSER_SCREEN.C

The Online Games server browser (built with configure.py --game-browser).
Like the virtual keyboard (interface/virtual_keyboard.c), it is a screen
drawn and driven by code over the menus, not built from widget tags. The
Multiplayer menu's ONLINE GAMES opens it (interface/ui_widget.c).

It lists the games from two sources that share invite codes: the game list
(port/linux/src/browser.c) and the internet lobby (port/linux/src/p2p_lobby.c).
A game in both is shown once.

The games are a list with a column each for name, map, mode, players and
ping, and the selected game's details (map picture, settings, who is in it)
are on the right.

Controls: arrow keys pick a game (left and right turn the page), A or Enter
joins it, and Escape (B) goes back. Everything else is clicked: a game, the
buttons along the bottom (JOIN, CREATE GAME, REFRESH, SORT, PROFILE, BACK)
and the column headings, which sort by that column.

Joining opens a tunnel to the host through the invite; once the host's game
is advertised through it, we join and open its lobby (wait_for_host).

A game on a Custom Edition map shows that map's name and picture if this
machine has it, marked CE. If the map is missing, the game says so and
can't be joined, since the map would fail to load.
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
#include "cache/cache_files.h"
#include "tag_files/files.h"
#include "../src/browser.h"
#include "../src/p2p.h"
#include "../src/ui_overlay.h"
#include "halo_ui_pointer.h"
#include "custom_edition_maps.h"
#include "overlay_screens.h"

/* ---------- constants */

enum
{
	ROWS_PER_PAGE = 14,
	/* milliseconds a status message stays up */
	STATUS_DURATION = 6000,
	/* give up on a host that hasn't answered after this many milliseconds */
	CONNECT_TIMEOUT = 15000,
	/* ignore picks this many milliseconds after opening (the menu's own A) */
	OPEN_SETTLE = 600,
	/* the cache of map lookups (known_map) */
	MAXIMUM_KNOWN_MAPS = 64,
};

/* p2p.c's invite links are this prefix followed by the invite code */
#define INVITE_LINK_PREFIX "halo://join/"

/* known_map.kind */
enum
{
	MAP_XBOX,
	/* a Custom Edition map this machine has (Halo PC's own maps included) */
	MAP_CUSTOM_EDITION,
	/* a campaign level, so a co-op game (custom_edition_maps.h) */
	MAP_CAMPAIGN,
	/* anything else, shown by file name */
	MAP_OTHER,
};

/* game engine names, short enough for the list */
static char const *const engine_names[] =
{
	"", "CTF", "Slayer", "Oddball", "King", "Race",
};

enum
{
	SORT_PLAYERS,
	SORT_NAME,
	SORT_MAP,
	SORT_TYPE,
	SORT_PING,

	NUMBER_OF_SORTS
};

/* ---------- structures */

/* a game's map as this machine sees it */
struct known_map
{
	char path[BROWSER_MAP_LENGTH];
	short kind;
	boolean installed;
	char name[48];
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
	/* waiting for a picked game's host to answer */
	boolean connecting;
	char connecting_invite[BROWSER_INVITE_LENGTH + 1];
	char connecting_name[64];
	unsigned long connecting_time;
	unsigned long opened_time;
	struct overlay_repeat repeat;
	/* the games are fetched every frame, so map lookups are cached */
	struct known_map known_maps[MAXIMUM_KNOWN_MAPS];
	short known_map_count;
	/* the bar button under the mouse, or NONE */
	short button_hovered;
	/* whether the mouse is over the connecting box's CANCEL */
	boolean cancel_hovered;
	/* CREATE GAME opened the map picker, which comes back here if backed out of */
	boolean creating;
} browser_screen;

/* ---------- private code */

static void set_status(
	char const *text)
{
	csstrncpy(browser_screen.status, text, sizeof(browser_screen.status) - 1);
	browser_screen.status[sizeof(browser_screen.status) - 1] = 0;
	browser_screen.status_time = system_milliseconds();
}

/* the last part of a map path such as levels\test\<name>\<name> */
static char const *map_file_name(
	char const *path)
{
	char const *base = path;
	char const *cursor;

	for (cursor = path; *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			base = cursor + 1;
	}
	return base;
}

/* Looks up a game's map (cached): its display name, its kind, and whether
this machine has it. */
static struct known_map const *known_map(
	char const *path)
{
	char const *base = map_file_name(path);
	struct known_map *map;
	short display_index;
	wchar_t const *display_name;
	char file[256];
	struct file_reference reference;
	short index;

	for (index = 0; index < browser_screen.known_map_count; index++)
	{
		if (!strcmp(browser_screen.known_maps[index].path, path))
			return &browser_screen.known_maps[index];
	}
	/* cache full: start over */
	if (browser_screen.known_map_count == MAXIMUM_KNOWN_MAPS)
		browser_screen.known_map_count = 0;
	map = &browser_screen.known_maps[browser_screen.known_map_count++];
	csstrncpy(map->path, path, sizeof(map->path) - 1);
	map->path[sizeof(map->path) - 1] = 0;
	map->installed = TRUE;
	if (overlay_xbox_map_name(base))
	{
		map->kind = MAP_XBOX;
		csstrncpy(map->name, overlay_xbox_map_name(base), sizeof(map->name) - 1);
		map->name[sizeof(map->name) - 1] = 0;
		return map;
	}
	display_index = custom_edition_maps_display_index(path);
	display_name = display_index != NONE ? custom_edition_maps_name(display_index) : NULL;
	if (display_name)
	{
		map->kind = custom_edition_maps_campaign(display_index) ? MAP_CAMPAIGN : MAP_CUSTOM_EDITION;
		overlay_utf8((unsigned short const *)display_name, sizeof(map->name), map->name, sizeof(map->name));
		return map;
	}
	map->kind = MAP_OTHER;
	csstrncpy(map->name, base, sizeof(map->name) - 1);
	map->name[sizeof(map->name) - 1] = 0;
	snprintf(file, sizeof(file), "%s%s.map", cache_files_map_directory(), base);
	map->installed = file_exists(file_reference_create_from_path(&reference, file, FALSE));
	return map;
}

/* network_client_manager.c: joins the game whose host the invite names,
once it is advertised. >0 joined, <0 can't join, 0 not yet. */
long network_game_client_join_invite_host(char const *invite);
boolean create_global_network_game_client(void);
void game_connection_set(short connection);
/* interface/ */
boolean ui_online_games_start_network(void);
void ui_online_games_stop_network(void);
boolean ui_widget_online_games_create_game(void);
/* the platform layer */
char const *config_string(char const *name);
/* menu_functions.c: host an internet game, as Create Game > Internet does */
void pc_menu_host_internet(void);

/* Signs in local player 1 with the profile System Link's Start would pick:
the last used, else the first saved. A profile index needs its valid bit
(saved_game_files.c); without it player_profile_get would return a profile
named after whatever saved file came first. With no profiles saved, the
player keeps the profile it has. */
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

/* closes the screen and stops polling the internet lobby */
static void close_screen(
	void)
{
	browser_screen.active = FALSE;
	p2p_lobby_browse(FALSE);
}

/* Starts joining the selected game: opens the tunnel through its invite.
wait_for_host finishes the join. */
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
	if (!known_map(game->map)->installed)
	{
		char text[sizeof(browser_screen.status)];

		snprintf(text, sizeof(text), "You don't have %s: put %s.map in your maps folder.",
			known_map(game->map)->name, map_file_name(game->map));
		set_status(text);
		return;
	}
	/* a network client listening for the host's advertisement, as System Link has */
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

/* joins the picked game once its host advertises it, and opens its lobby */
static void wait_for_host(
	void)
{
	long joined = network_game_client_join_invite_host(browser_screen.connecting_invite);

	if (joined > 0)
	{
		browser_screen.connecting = FALSE;
		close_screen();
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

	/* closed games always go last */
	if (a->open != b->open)
		return a->open ? -1 : 1;
	switch (browser_screen.sort)
	{
	case SORT_NAME: order = compare_names(a->name, b->name); break;
	case SORT_MAP:
	{
		char a_name[sizeof(((struct known_map *)0)->name)];

		/* copy a's name: looking up b can evict a's cache entry */
		csstrcpy(a_name, known_map(a->map)->name);
		order = strcmp(a_name, known_map(b->map)->name);
		break;
	}
	case SORT_TYPE: order = (long)a->engine * 2 + a->teams - ((long)b->engine * 2 + b->teams); break;
	/* lowest first; not measured goes last */
	case SORT_PING: order = (a->ping < 0 ? SHORT_MAX : a->ping) - (long)(b->ping < 0 ? SHORT_MAX : b->ping); break;
	default: order = (long)b->players - (long)a->players; break;
	}
	return order ? order : compare_names(a->name, b->name);
}

/* Appends the internet lobby's games that aren't already listed. Both
sources use the same invite codes, so a game in both is kept once. */
static void add_lobby_games(
	void)
{
	static struct p2p_listing listings[BROWSER_MAXIMUM_GAMES];
	int listing_count = p2p_lobby_games(listings, BROWSER_MAXIMUM_GAMES);
	size_t prefix_length = strlen(INVITE_LINK_PREFIX);
	int listing_index;

	for (listing_index = 0; listing_index < listing_count && browser_screen.count < BROWSER_MAXIMUM_GAMES; listing_index++)
	{
		struct p2p_listing const *listing = &listings[listing_index];
		char const *invite = listing->invite + prefix_length;
		struct browser_game *game;
		short index;

		if (strncmp(listing->invite, INVITE_LINK_PREFIX, prefix_length) || strlen(invite) != BROWSER_INVITE_LENGTH)
			continue;
		for (index = 0; index < browser_screen.count && strcmp(browser_screen.games[index].invite, invite); index++)
		{
		}
		/* listed already: only the lobby knows the ping */
		if (index < browser_screen.count)
		{
			browser_screen.games[index].ping = listing->ping;
			continue;
		}
		game = &browser_screen.games[browser_screen.count++];
		csmemset(game, 0, sizeof(*game));
		csstrcpy(game->invite, invite);
		/* lobby names are ASCII */
		for (index = 0; index < BROWSER_NAME_LENGTH && listing->name[index]; index++)
			game->name[index] = (unsigned char)listing->name[index];
		snprintf(game->map, sizeof(game->map), "%s", listing->map);
		snprintf(game->gametype, sizeof(game->gametype), "%s", listing->gametype);
		game->engine = listing->engine_type;
		game->players = listing->player_count;
		game->maximum_players = listing->maximum_player_count;
		game->open = listing->open;
		game->teams = listing->has_teams;
		game->ping = listing->ping;
	}
}

/* refetches and sorts the games, keeping the same game selected */
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
	add_lobby_games();
	/* insertion sort: there are only a few dozen games */
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

/* back to the menu, with the network search stopped */
static void leave(
	void)
{
	close_screen();
	ui_online_games_stop_network();
}

/* ---------- public code */

boolean browser_screen_active(
	void)
{
	return browser_screen.active;
}

/* the Multiplayer menu's ONLINE GAMES (interface/ui_widget.c) */
void browser_screen_open(
	void)
{
	browser_screen.active = TRUE;
	browser_screen.selected = 0;
	browser_screen.status[0] = 0;
	browser_screen.button_hovered = NONE;
	browser_screen.connecting = FALSE;
	browser_screen.opened_time = system_milliseconds();
	/* maps may have been added since last time */
	browser_screen.known_map_count = 0;
	p2p_lobby_browse(TRUE);
	/* drop the menu's A that opened us, still queued */
	event_manager_flush();
	/* start listening as System Link does, ending any game left behind */
	if (!ui_online_games_start_network())
		set_status("Could not start the network.");
	fetch_games();
}

/* hosts a new internet game; the map picker opens next */
static void create_game(
	void)
{
	join_first_player();
	pc_menu_host_internet();
	/* (set first: the picker can open as the map select screen loads) */
	browser_screen.creating = TRUE;
	if (ui_widget_online_games_create_game())
	{
		close_screen();
	}
	else
	{
		browser_screen.creating = FALSE;
		set_status("Could not create a game.");
	}
}

/* the map picker, as it opens: whether CREATE GAME opened it (asked once) */
boolean browser_screen_take_create(
	void)
{
	boolean creating = browser_screen.creating;

	browser_screen.creating = FALSE;
	return creating;
}

/* ---------- actions */

static boolean settled(void)
{
	return !browser_screen.connecting && system_milliseconds() - browser_screen.opened_time > OPEN_SETTLE;
}

static void action_join(void)
{
	if (settled())
		join_selected();
}

static void action_create(void)
{
	if (settled())
		create_game();
}

static void action_refresh(void)
{
	p2p_lobby_refresh();
	fetch_games();
	set_status("Refreshed");
}

static void action_sort_next(void)
{
	browser_screen.sort = (short)((browser_screen.sort + 1) % NUMBER_OF_SORTS);
	fetch_games();
}

static void action_profile(void)
{
	browser_open_profile();
	set_status("Opening your profile in the web browser");
}

/* while connecting, cancels the join */
static void action_back(void)
{
	if (browser_screen.connecting)
		browser_screen.connecting = FALSE;
	else
		leave();
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
			case _gamepad_analog_button_a: action_join(); break;
			case _gamepad_analog_button_b: action_back(); break;
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
	/* the menus behind get no input while this is open */
	event_manager_flush();
}

/* ---------- drawing */

/* the theme's, set by layout_update */
static struct overlay_palette const *palette;

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
#define COLOR_RED_TEAM OVERLAY_COLOR_RED_TEAM
#define COLOR_BLUE_TEAM OVERLAY_COLOR_BLUE_TEAM
#define COLOR_NOTICE OVERLAY_COLOR_NOTICE
#define COLOR_GOOD OVERLAY_COLOR_GOOD
#define COLOR_FAIR OVERLAY_COLOR_FAIR
#define COLOR_POOR OVERLAY_COLOR_POOR

/* The list's columns: each spans a share of the list's width, and the
numbers are right-aligned. Clicking a heading sorts by it. */
struct browser_column
{
	char const *heading;
	short sort;
	float left, right;
	boolean numeric;
};
static struct browser_column const columns[] =
{
	{ "NAME", SORT_NAME, 0.035f, 0.41f, FALSE },
	{ "MAP", SORT_MAP, 0.43f, 0.65f, FALSE },
	{ "MODE", SORT_TYPE, 0.67f, 0.81f, FALSE },
	{ "PLAYERS", SORT_PLAYERS, 0.81f, 0.91f, TRUE },
	{ "PING", SORT_PING, 0.92f, 0.985f, TRUE },
};

/* the layout, in the menus' 640x480: a list of games on the left under its
column headings, the selected game's details on the right */
enum
{
	COLUMNS_Y = 74, LIST_Y = 92, ROW_HEIGHT = 22,
	FOOTER_Y = LIST_Y + ROWS_PER_PAGE * ROW_HEIGHT + 6,
	DETAIL_Y = 78, DETAIL_WIDTH = 183, DETAIL_PICTURE_HEIGHT = 112,
	ROSTER_COLUMNS = 2, ROSTER_ROWS = 8,
	/* this client's layout: the gap to the screen's edges and between list
	and details, and the widest the list gets on a very wide screen */
	EDGE_GAP = 24, COLUMN_GAP = 24, MAXIMUM_LIST_WIDTH = 560,
};

/* Where the columns go, in the 640x480 layout's units: Vanilla's are fixed
and centred; this client's list starts at the screen's left edge and the
details end at its right, however wide the screen. */
static struct
{
	float list_x;
	float list_width;
	float detail_x;
	/* the right edge of the details and the header's sort tabs */
	float right;
} layout;

static void layout_update(
	void)
{
	/* the screen's edges are this far outside the 640 units */
	float edge = (float)((halo_screen_width() - 640) / 2);

	palette = overlay_palette_current();
	if (!palette->own_screens)
	{
		layout.list_x = 37.0f;
		layout.list_width = 368.0f;
		layout.detail_x = 420.0f;
		layout.right = 603.0f;
		return;
	}
	layout.list_x = -edge + EDGE_GAP;
	layout.right = 640.0f + edge - EDGE_GAP;
	layout.detail_x = layout.right - DETAIL_WIDTH;
	layout.list_width = MIN(layout.detail_x - COLUMN_GAP - layout.list_x, (float)MAXIMUM_LIST_WIDTH);
}

/* the card row at a point of the 640x480 layout, or NONE */
static short row_at(
	short x,
	short y)
{
	short row = (short)((y - LIST_Y) / ROW_HEIGHT);

	if (x < layout.list_x || x >= layout.list_x + layout.list_width || y < LIST_Y || row >= ROWS_PER_PAGE)
		return NONE;
	return row;
}

/* the button bar, in this order */
enum
{
	BUTTON_JOIN,
	BUTTON_CREATE,
	BUTTON_REFRESH,
	BUTTON_SORT,
	BUTTON_PROFILE,
	BUTTON_BACK,
	NUMBER_OF_BUTTONS
};
static void (*const button_actions[NUMBER_OF_BUTTONS])(void) =
{
	action_join, action_create, action_refresh, action_sort_next, action_profile, action_back,
};

enum
{
	/* the connecting box's CANCEL button */
	CANCEL_Y = 234,
};

static char const *const sort_names[NUMBER_OF_SORTS] = { "PLAYERS", "NAME", "MAP", "MODE", "PING" };
static char const *const cancel_label[] = { "CANCEL" };

/* the bar's labels; SORT shows the current order */
struct button_labels
{
	char const *labels[NUMBER_OF_BUTTONS];
	char sort[32];
};

static void button_labels_get(struct button_labels *buttons)
{
	snprintf(buttons->sort, sizeof(buttons->sort), "SORT: %s", sort_names[browser_screen.sort]);
	buttons->labels[BUTTON_JOIN] = "JOIN";
	buttons->labels[BUTTON_CREATE] = "CREATE GAME";
	buttons->labels[BUTTON_REFRESH] = "REFRESH";
	buttons->labels[BUTTON_SORT] = buttons->sort;
	buttons->labels[BUTTON_PROFILE] = "PROFILE";
	buttons->labels[BUTTON_BACK] = "BACK";
}

/* the bar's left edge: at the list's left in this client's layout, centred in Vanilla's */
static float buttons_left(struct button_labels const *buttons)
{
	return palette->own_screens ? (float)layout.list_x :
		320.0f - overlay_buttons_width(buttons->labels, NUMBER_OF_BUTTONS) / 2;
}

static float cancel_left(void)
{
	return 320.0f - overlay_buttons_width(cancel_label, 1) / 2;
}

/* while connecting, only BACK works */
static unsigned long buttons_disabled(void)
{
	return browser_screen.connecting ? ~FLAG(BUTTON_BACK) : 0;
}

/* a column's edges in the 640x480 layout */
static float column_left(struct browser_column const *column)
{
	return layout.list_x + column->left * layout.list_width;
}

static float column_right(struct browser_column const *column)
{
	return layout.list_x + column->right * layout.list_width;
}

/* the sort of the column heading at a point, or NONE */
static short sort_heading_at(short x, short y)
{
	short index;

	if (y < COLUMNS_Y - 3 || y >= LIST_Y)
		return NONE;
	for (index = 0; index < NUMBEROF(columns); index++)
	{
		if (x >= column_left(&columns[index]) - 4 && x < column_right(&columns[index]) + 4)
			return columns[index].sort;
	}
	return NONE;
}

/* The mouse: hovering a row selects it and clicking joins it; the wheel
moves the selection; the buttons and column headings are clicked. */
void browser_screen_pointer(
	struct halo_ui_pointer const *pointer)
{
	short page_first = (short)(browser_screen.selected - browser_screen.selected % ROWS_PER_PAGE);
	struct button_labels buttons;
	short row;

	layout_update();
	button_labels_get(&buttons);
	if (pointer->moved)
	{
		browser_screen.button_hovered = overlay_button_at(buttons.labels, NUMBER_OF_BUTTONS, buttons_left(&buttons),
			OVERLAY_BUTTON_Y, pointer->x, pointer->y);
		browser_screen.cancel_hovered = overlay_button_at(cancel_label, 1, cancel_left(), CANCEL_Y,
			pointer->x, pointer->y) != NONE;
	}
	if (pointer->left_clicks)
	{
		short button = overlay_button_at(buttons.labels, NUMBER_OF_BUTTONS, buttons_left(&buttons), OVERLAY_BUTTON_Y,
			pointer->click_x, pointer->click_y);
		short sort = sort_heading_at(pointer->click_x, pointer->click_y);

		if (browser_screen.connecting &&
			overlay_button_at(cancel_label, 1, cancel_left(), CANCEL_Y, pointer->click_x, pointer->click_y) != NONE)
		{
			action_back();
			return;
		}
		if (button != NONE && !TEST_FLAG(buttons_disabled(), button))
		{
			button_actions[button]();
			return;
		}
		if (sort != NONE && !browser_screen.connecting)
		{
			browser_screen.sort = sort;
			fetch_games();
			return;
		}
	}
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
}

static char const *type_name(
	struct browser_game const *game,
	char *text,
	long size)
{
	char const *engine = game->engine >= 0 && game->engine < NUMBEROF(engine_names) && engine_names[game->engine][0] ?
		engine_names[game->engine] : "Game";

	/* no game engine on a campaign level means co-op; the host's description
	adds the difficulty ("Co-op Heroic") */
	if (!game->engine && known_map(game->map)->kind == MAP_CAMPAIGN)
	{
		snprintf(text, (size_t)size, "%s", !strncmp(game->gametype, "Co-op", 5) ? game->gametype : "Co-op");
		return text;
	}

	/* CTF is always a team game, so it gets no "Team" prefix */
	snprintf(text, (size_t)size, "%s%s", game->teams && game->engine != 1 ? "Team " : "", engine);
	return text;
}

/* the screen's frame and title, and how many games and players there are */
static void render_header(
	long players)
{
	char text[96];

	overlay_screen_frame("ONLINE GAMES", layout.list_x, 22, 24.0f);
	snprintf(text, sizeof(text), "%d %s  \xC2\xB7  %ld %s", browser_screen.count, browser_screen.count == 1 ? "GAME" : "GAMES",
		players, players == 1 ? "PLAYER" : "PLAYERS");
	overlay_screen_subtitle(text, layout.list_x + 1, 50);
}

/* the column headings, the one sorted by bright and underlined */
static void render_columns(
	void)
{
	short index;

	for (index = 0; index < NUMBEROF(columns); index++)
	{
		struct browser_column const *column = &columns[index];
		boolean sorted = column->sort == browser_screen.sort;
		float width = ui_overlay_text_width(UI_FONT_BOLD, 8.0f, column->heading);
		float x = column->numeric ? column_right(column) - width : column_left(column);

		ui_overlay_text(UI_FONT_BOLD, 8.0f, x, COLUMNS_Y, UI_ALIGN_LEFT, sorted ? COLOR_TITLE : COLOR_DIM, column->heading);
		if (sorted)
			ui_overlay_rect(x, COLUMNS_Y + 11, width, 1.0f, 0, COLOR_TITLE);
	}
	ui_overlay_rect(layout.list_x, LIST_Y - 2, layout.list_width, 0.75f, 0, COLOR_ROW_RULE);
}

static unsigned int ping_color(
	short ping)
{
	return ping < 0 ? COLOR_DIM : ping < 80 ? COLOR_GOOD : ping < 160 ? COLOR_FAIR : COLOR_POOR;
}

/* A game's row: a dot for whether it can be joined, then a column each for
its name, map, mode, players and ping. */
static void render_row(
	struct browser_game const *game,
	short row,
	boolean chosen)
{
	struct known_map const *map = known_map(game->map);
	float y = (float)(LIST_Y + row * ROW_HEIGHT);
	float text_y = y + 5;
	boolean full = game->players >= game->maximum_players && game->maximum_players > 0;
	unsigned int color = game->open ? COLOR_TEXT : COLOR_DIM;
	unsigned int dot = !map->installed ? COLOR_NOTICE : game->open && !full ? COLOR_GOOD : COLOR_DIM;
	char text[96];
	float x;

	overlay_row(layout.list_x, y, layout.list_width, ROW_HEIGHT - 1, chosen, row % 2);
	ui_overlay_rect(layout.list_x + 0.013f * layout.list_width - 2.5f, y + ROW_HEIGHT / 2 - 3, 5, 5, 2.5f, dot);

	overlay_utf8(game->name, NUMBEROF(game->name), text, sizeof(text));
	overlay_text_fitted(UI_FONT_BOLD, 10.0f, column_left(&columns[0]), text_y - 1,
		column_right(&columns[0]) - column_left(&columns[0]), chosen ? COLOR_TITLE : color, text);

	/* a Custom Edition map gets a CE tag after its name; a missing one is
	marked in the notice color */
	x = column_left(&columns[1]);
	if (map->kind == MAP_CUSTOM_EDITION && map->installed)
	{
		float tag = ui_overlay_text_width(UI_FONT_BOLD, 7.0f, "CE") + 6;

		ui_overlay_outline(column_right(&columns[1]) - tag, y + 5, tag, 11, 2.0f, 0.75f, COLOR_DIM);
		ui_overlay_text(UI_FONT_BOLD, 7.0f, column_right(&columns[1]) - tag + 3, y + 6.5f, UI_ALIGN_LEFT, COLOR_DIM, "CE");
		overlay_text_fitted(UI_FONT_REGULAR, 9.5f, x, text_y, column_right(&columns[1]) - x - tag - 4, color, map->name);
	}
	else
	{
		overlay_text_fitted(UI_FONT_REGULAR, 9.5f, x, text_y, column_right(&columns[1]) - x,
			map->installed ? color : COLOR_NOTICE, map->name);
	}

	overlay_text_fitted(UI_FONT_REGULAR, 9.5f, column_left(&columns[2]), text_y,
		column_right(&columns[2]) - column_left(&columns[2]), color, type_name(game, text, sizeof(text)));

	snprintf(text, sizeof(text), "%d/%d", game->players, game->maximum_players);
	ui_overlay_text(UI_FONT_REGULAR, 9.5f, column_right(&columns[3]), text_y, UI_ALIGN_RIGHT,
		full ? COLOR_POOR : color, text);

	if (game->ping >= 0)
		snprintf(text, sizeof(text), "%d", game->ping);
	else
		snprintf(text, sizeof(text), "\xE2\x80\x93");
	ui_overlay_text(UI_FONT_REGULAR, 9.5f, column_right(&columns[4]), text_y, UI_ALIGN_RIGHT,
		ping_color(game->ping), text);
}

/* The selected game's details: map, settings, and the roster if the host
shares one (the last slot says how many more there are). */
static void render_details(
	struct browser_game const *game)
{
	char name[64], text[64];
	float y = DETAIL_Y + DETAIL_PICTURE_HEIGHT + 8;
	long index;

	overlay_panel(layout.detail_x - 6, DETAIL_Y - 4, DETAIL_WIDTH + 12, OVERLAY_FRAME_BOTTOM - DETAIL_Y - 6);
	if (!game)
		return;
	overlay_map_picture(overlay_map_display_index(game->map), layout.detail_x, DETAIL_Y, DETAIL_WIDTH,
		DETAIL_PICTURE_HEIGHT);
	ui_overlay_outline(layout.detail_x, DETAIL_Y, DETAIL_WIDTH, DETAIL_PICTURE_HEIGHT, 0, 0.75f, COLOR_PANEL_EDGE);
	overlay_utf8(game->name, NUMBEROF(game->name), name, sizeof(name));
	ui_overlay_text(UI_FONT_BOLD, 13.0f, layout.detail_x, y, UI_ALIGN_LEFT, COLOR_TITLE, name);
	y += 20;
#define DETAIL_LINE(label, value) \
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.detail_x, y, UI_ALIGN_LEFT, COLOR_DIM, label); \
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.detail_x + DETAIL_WIDTH, y, UI_ALIGN_RIGHT, COLOR_TEXT, value); \
	y += 13;
	DETAIL_LINE("Status", game->open ? "Accepting players" : "In progress");
	if (known_map(game->map)->kind != MAP_XBOX)
	{
		DETAIL_LINE("Map", !known_map(game->map)->installed ? "Not installed" :
			known_map(game->map)->kind == MAP_CUSTOM_EDITION ? "Custom Edition" :
			known_map(game->map)->kind == MAP_CAMPAIGN ? "Campaign" : "Custom");
	}
	DETAIL_LINE("Mode", type_name(game, text, sizeof(text)));
	snprintf(text, sizeof(text), "%d of %d", game->players, game->maximum_players);
	DETAIL_LINE("Players", text);
	if (game->score_limit)
	{
		snprintf(text, sizeof(text), "%d", game->score_limit);
		DETAIL_LINE("Score limit", text);
	}
	if (game->ping >= 0)
	{
		snprintf(text, sizeof(text), "%d ms", game->ping);
		DETAIL_LINE("Ping", text);
	}
#undef DETAIL_LINE

	y += 4;
	ui_overlay_rect(layout.detail_x, y, DETAIL_WIDTH, 0.75f, 0, COLOR_ROW_RULE);
	y += 5;
	if (!game->players && !game->roster_count)
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.detail_x, y, UI_ALIGN_LEFT, COLOR_DIM, "No one yet");
	else if (!game->roster_count)
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.detail_x, y, UI_ALIGN_LEFT, COLOR_DIM, "This host doesn't share names");
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
			ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.detail_x + (index / ROSTER_ROWS) * column_width,
				y + (index % ROSTER_ROWS) * 11, UI_ALIGN_LEFT, color, name);
		}
		if (game->roster_count > shown)
		{
			snprintf(text, sizeof(text), "+%d more", game->roster_count - (int)shown);
			ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.detail_x + (shown / ROSTER_ROWS) * column_width,
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
	struct overlay_button_colors colors;
	struct button_labels buttons;
	struct browser_game const *selected = browser_screen.count ? &browser_screen.games[browser_screen.selected] : NULL;

	if (!ui_overlay_available())
		return;
	for (index = 0; index < browser_screen.count; index++)
		players += browser_screen.games[index].players;

	layout_update();
	render_header(players);

	page_first = (short)(browser_screen.selected - browser_screen.selected % ROWS_PER_PAGE);
	page_count = (short)MAX(1, (browser_screen.count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
	/* (the list on a panel of its own, but on Glassed's band) */
	if (!palette->own_screens || palette->framed)
		overlay_panel(layout.list_x - 4, COLUMNS_Y - 6, layout.list_width + 8, FOOTER_Y - COLUMNS_Y + 4);
	render_columns();
	if (!browser_screen.count)
	{
		ui_overlay_text(UI_FONT_BOLD, 12.0f, layout.list_x + 10, LIST_Y + 20, UI_ALIGN_LEFT, COLOR_TEXT, "No games right now");
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, layout.list_x + 10, LIST_Y + 38, UI_ALIGN_LEFT, COLOR_DIM,
			"Host one with Create Game, and it shows here for everyone.");
	}
	for (row = 0; row < ROWS_PER_PAGE && page_first + row < browser_screen.count; row++)
		render_row(&browser_screen.games[page_first + row], row, page_first + row == browser_screen.selected);
	if (page_count > 1)
	{
		snprintf(text, sizeof(text), "\xE2\x80\xB9  %d / %d  \xE2\x80\xBA", page_first / ROWS_PER_PAGE + 1, page_count);
		ui_overlay_text(UI_FONT_BOLD, 9.0f, layout.list_x + layout.list_width, FOOTER_Y, UI_ALIGN_RIGHT, COLOR_DIM, text);
	}
	render_details(selected);

	colors.fill = COLOR_PANEL;
	colors.fill_lit = COLOR_ROW_SELECTED;
	colors.edge = COLOR_PANEL_EDGE;
	colors.text = COLOR_PROMPT;
	colors.text_lit = COLOR_TITLE;
	colors.text_disabled = COLOR_DIM;
	colors.radius = palette->radius;
	button_labels_get(&buttons);
	overlay_buttons_draw(buttons.labels, NUMBER_OF_BUTTONS, buttons_left(&buttons), OVERLAY_BUTTON_Y,
		browser_screen.button_hovered, buttons_disabled(), &colors);

	if (browser_screen.connecting)
	{
		char line[160];
		long dots = (long)((system_milliseconds() - browser_screen.connecting_time) / 400 % 4);

		snprintf(line, sizeof(line), "Connecting to %s%.*s", browser_screen.connecting_name, (int)dots, "...");
		ui_overlay_rect(170, 200, 300, 64, palette->radius, palette->connecting);
		ui_overlay_outline(170, 200, 300, 64, palette->radius, 0.75f, COLOR_PANEL_EDGE);
		ui_overlay_text(UI_FONT_BOLD, 12.0f, 320, 212, UI_ALIGN_CENTER, 0xFFFFFFFF, line);
		overlay_buttons_draw(cancel_label, 1, cancel_left(), CANCEL_Y, browser_screen.cancel_hovered ? 0 : NONE, 0,
			&colors);
	}
	/* under the list, left of the page number */
	else if (browser_screen.status[0] && system_milliseconds() - browser_screen.status_time < STATUS_DURATION)
	{
		ui_overlay_text(UI_FONT_REGULAR, 10.0f, layout.list_x, FOOTER_Y, UI_ALIGN_LEFT, COLOR_NOTICE, browser_screen.status);
	}
}

#endif
