/*
LOBBY_SCREEN.C

The pregame lobby, drawn in the server browser's style (browser_screen.c):
the players down the left (a column per team in a team game); the map,
the game's details and the countdown on the right; the buttons along the
foot.

The lobby's widgets (tools/port_settings.py, _lobby) still run it. They
are invisible, with no pictures and clear text, but they update the lobby
("port lobby update" in menu_functions.c), take the keyboard's focus and
catch the mouse. This draws over them, in the same places, and lights the
row or button that has the focus.
*/

#ifdef HALO_GAME_BROWSER

#include "cseries.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "networking/network_game_manager.h"

#include "custom_edition_maps.h"
#include "overlay_screens.h"
#include "../src/p2p.h"
#include "../src/ui_overlay.h"

#include <string.h>

/* ---------- constants */

/* the widgets' places (tools/port_settings.py: LOBBY_ROW_*, LOBBY_BUTTON*) */
enum
{
	ROW_LEFT = 24, ROW_TOP = 92, ROW_HEIGHT = 24, ROW_WIDTH = 380, ROWS = 13,
	BUTTONS_TOP = 448, BUTTON_WIDTH = 104, BUTTON_HEIGHT = 22,
	NUMBER_OF_BUTTONS = 4,
};
static short const button_lefts[NUMBER_OF_BUTTONS] = { 176, 288, 400, 512 };
static char const *const button_names[NUMBER_OF_BUTTONS] =
	{ "lobby_button_team", "lobby_button_start", "lobby_button_add", "lobby_button_leave" };
static char const *const button_labels[NUMBER_OF_BUTTONS] = { "SWITCH TEAM", "START NOW", "ADD PLAYER", "LEAVE" };

/* the rest of the layout, in the menus' 640x480 */
enum
{
	HEADING_Y = 74,
	PANEL_X = 420, PANEL_WIDTH = 196, PANEL_TOP = 78, PICTURE_HEIGHT = 118,
	RIGHT = PANEL_X + PANEL_WIDTH,
};

static char const *const engine_names[] = { "Game", "CTF", "Slayer", "Oddball", "King of the Hill", "Race" };
static char const *const difficulty_names[] = { "Easy", "Normal", "Heroic", "Legendary" };

/* ---------- prototypes */

void *global_network_game_client_get(void);
void *global_network_game_server_get(void);
struct network_game *network_game_client_get_game(void *client);
short network_game_client_get_local_machine_index(void);
short network_game_client_get_seconds_to_game_start(void *client);
struct widget_instance *ui_widget_port_top(void);
/* menu_functions.c */
short pc_menu_lobby_players(struct network_player *const **players, short *first);
wchar_t const *pc_menu_lobby_join_help(void);

/* ---------- private code */

/* the child of a widget with this name, or NULL */
static struct widget_instance *child_named(
	struct widget_instance *widget,
	char const *name)
{
	struct widget_instance *child;

	for (child = widget ? widget->child : NULL; child; child = child->next)
	{
		if (child->name && !strcmp(child->name, name))
			return child;
	}
	return NULL;
}

/* the lobby's list (its rows and button bar), when the lobby is the screen
up */
static struct widget_instance *lobby_list(
	void)
{
	struct widget_instance *top = ui_widget_port_top();

	if (!top || !top->name || strcmp(top->name, "lobby_screen"))
		return NULL;
	return child_named(top, "lobby_list");
}

static boolean game_cooperative(
	struct network_game const *game)
{
	return !game->variant.game_engine_index && custom_edition_maps_level_campaign(game->map.name);
}

/* one of the panel's lines: a label, and its value right-aligned */
static float detail_line(
	struct overlay_palette const *palette,
	float y,
	char const *label,
	char const *value)
{
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, PANEL_X, y, UI_ALIGN_LEFT, palette->dim, label);
	ui_overlay_text(UI_FONT_REGULAR, 9.0f, RIGHT, y, UI_ALIGN_RIGHT, palette->text, value);
	return y + 14;
}

/* what the game is, and the countdown (or what it waits for) at the right */
static void render_header(
	struct overlay_palette const *palette,
	struct network_game const *game,
	void *client,
	short player_count)
{
	short seconds = network_game_client_get_seconds_to_game_start(client);
	char description[64], text[96];

	overlay_utf8((unsigned short const *)game->variant.human_readable_game_description,
		NUMBEROF(game->variant.human_readable_game_description), description, sizeof(description));
	snprintf(text, sizeof(text), "%s  \xC2\xB7  %d / %d PLAYERS", description, player_count, game->maximum_players);
	overlay_screen_subtitle(text, ROW_LEFT + 1, 50);
	if (seconds > 0)
	{
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, RIGHT, 22, UI_ALIGN_RIGHT, palette->dim, "STARTING IN");
		snprintf(text, sizeof(text), "%d", seconds);
		ui_overlay_text(UI_FONT_BOLD, 26.0f, RIGHT, 32, UI_ALIGN_RIGHT, OVERLAY_COLOR_NOTICE, text);
	}
	else
	{
		ui_overlay_text(UI_FONT_BOLD, 10.0f, RIGHT, 40, UI_ALIGN_RIGHT, palette->dim,
			game->machine_count < 2 ? "WAITING FOR PLAYERS" : "READY");
	}
}

/* A player's row: the name, a bar of its team's color in a team game, and
YOU on this machine's players; lit, or striped (every other row). */
static void render_player_row(
	struct overlay_palette const *palette,
	struct network_player const *player,
	float x,
	float y,
	float width,
	boolean teams,
	boolean lit,
	boolean striped)
{
	char name[64];

	overlay_row(x, y, width, ROW_HEIGHT - 1, lit, striped);
	if (teams)
	{
		ui_overlay_rect(x, y, 3.0f, ROW_HEIGHT - 1, 0,
			player->team_index ? OVERLAY_COLOR_BLUE_TEAM : OVERLAY_COLOR_RED_TEAM);
	}
	overlay_utf8((unsigned short const *)player->name, NUMBEROF(player->name), name, sizeof(name));
	overlay_text_fitted(UI_FONT_BOLD, 11.0f, x + 10, y + 5, width - 50, lit ? palette->title : palette->text, name);
	if (player->machine_index == network_game_client_get_local_machine_index())
		ui_overlay_text(UI_FONT_BOLD, 8.0f, x + width - 8, y + 7, UI_ALIGN_RIGHT, OVERLAY_COLOR_NOTICE, "YOU");
}

/* A team game's players: a column per team under its name and count, as
many as fit (the rest counted at the foot). */
static void render_teams(
	struct overlay_palette const *palette,
	struct network_player *const *players,
	short count)
{
	static char const *const team_names[] = { "RED TEAM", "BLUE TEAM" };
	static unsigned int const team_colors[] = { OVERLAY_COLOR_RED_TEAM, OVERLAY_COLOR_BLUE_TEAM };
	float width = (ROW_WIDTH - 12) / 2.0f;
	short team;

	for (team = 0; team < NUMBEROF(team_names); team++)
	{
		float x = ROW_LEFT + team * (width + 12);
		short members = 0, shown = 0, index;
		char text[32];

		for (index = 0; index < count; index++)
			members += (players[index]->team_index ? 1 : 0) == team;
		ui_overlay_text(UI_FONT_BOLD, 8.0f, x + 10, HEADING_Y, UI_ALIGN_LEFT, team_colors[team], team_names[team]);
		snprintf(text, sizeof(text), "%d", members);
		ui_overlay_text(UI_FONT_BOLD, 8.0f, x + width - 8, HEADING_Y, UI_ALIGN_RIGHT, palette->dim, text);
		ui_overlay_rect(x, ROW_TOP - 2, width, 0.75f, 0, team_colors[team]);
		for (index = 0; index < count; index++)
		{
			float y = (float)(ROW_TOP + shown * ROW_HEIGHT);

			if ((players[index]->team_index ? 1 : 0) != team)
				continue;
			if (shown == ROWS - 1 && members > ROWS)
			{
				snprintf(text, sizeof(text), "+%d MORE", members - shown);
				ui_overlay_text(UI_FONT_BOLD, 9.0f, x + 10, y + 6, UI_ALIGN_LEFT, palette->dim, text);
				break;
			}
			render_player_row(palette, players[index], x, y, width, TRUE, FALSE, shown % 2);
			shown++;
		}
	}
}

/* The players: in a team game a column per team, else a list (scrolled by
the rows' focus, the row with the focus lit). */
static void render_players(
	struct overlay_palette const *palette,
	struct widget_instance *list,
	struct network_game const *game)
{
	struct network_player *const *players;
	short first, count = pc_menu_lobby_players(&players, &first);
	struct widget_instance *row = list->child;
	char text[64];
	short index;

	if (game->variant.universal_variant.teams)
	{
		render_teams(palette, players, count);
		return;
	}
	ui_overlay_text(UI_FONT_BOLD, 8.0f, ROW_LEFT + 10, HEADING_Y, UI_ALIGN_LEFT, palette->dim, "PLAYERS");
	if (count > ROWS)
		snprintf(text, sizeof(text), "%d\xE2\x80\x93%d OF %d", first + 1, MIN(first + ROWS, count), count);
	else
		snprintf(text, sizeof(text), "%d", count);
	ui_overlay_text(UI_FONT_BOLD, 8.0f, ROW_LEFT + ROW_WIDTH - 8, HEADING_Y, UI_ALIGN_RIGHT, palette->dim, text);
	ui_overlay_rect(ROW_LEFT, ROW_TOP - 2, ROW_WIDTH, 0.75f, 0, palette->row_rule);
	for (index = 0; index < ROWS && first + index < count; index++, row = row ? row->next : NULL)
	{
		float y = (float)(ROW_TOP + index * ROW_HEIGHT);
		boolean lit = row && list->focused_child == row;

		render_player_row(palette, players[first + index], ROW_LEFT, y, ROW_WIDTH, FALSE, lit, index % 2);
	}
}

/* the map's picture and name, the game's details, and the invite note */
static void render_panel(
	struct overlay_palette const *palette,
	struct network_game const *game,
	short player_count)
{
	boolean cooperative = game_cooperative(game);
	char text[96];
	char link[256];
	float y = PANEL_TOP + PICTURE_HEIGHT + 8;

	overlay_panel(PANEL_X - 6, PANEL_TOP - 4, PANEL_WIDTH + 12, OVERLAY_FRAME_BOTTOM - PANEL_TOP - 6);
	overlay_map_picture(overlay_map_display_index(game->map.name), PANEL_X, PANEL_TOP, PANEL_WIDTH, PICTURE_HEIGHT);
	ui_overlay_outline(PANEL_X, PANEL_TOP, PANEL_WIDTH, PICTURE_HEIGHT, 0, 0.75f, palette->panel_edge);
	overlay_map_name(game->map.name, text, sizeof(text));
	overlay_text_fitted(UI_FONT_BOLD, 13.0f, PANEL_X, y, PANEL_WIDTH, palette->title, text);
	y += 22;

	overlay_utf8((unsigned short const *)game->variant.human_readable_game_description,
		NUMBEROF(game->variant.human_readable_game_description), text, sizeof(text));
	y = detail_line(palette, y, "Game", text);
	if (cooperative)
	{
		y = detail_line(palette, y, "Difficulty", difficulty_names[PIN(game->difficulty, 0, NUMBEROF(difficulty_names) - 1)]);
	}
	else
	{
		y = detail_line(palette, y, "Mode", engine_names[PIN(game->variant.game_engine_index, 0, NUMBEROF(engine_names) - 1)]);
		y = detail_line(palette, y, "Teams", game->variant.universal_variant.teams ? "Yes" : "No");
	}
	snprintf(text, sizeof(text), "%d of %d", player_count, game->maximum_players);
	detail_line(palette, y, "Players", text);

	if (global_network_game_server_get() && p2p_invite_link(link, sizeof(link)))
	{
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, PANEL_X, OVERLAY_FRAME_BOTTOM - 40, UI_ALIGN_LEFT, OVERLAY_COLOR_NOTICE,
			"Invite link copied");
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, PANEL_X, OVERLAY_FRAME_BOTTOM - 27, UI_ALIGN_LEFT, palette->dim,
			"Paste it to friends to bring them in.");
	}
}

/* the buttons, where the widgets are; the one with the focus lit */
static void render_buttons(
	struct overlay_palette const *palette,
	struct widget_instance *list)
{
	struct widget_instance *bar = child_named(list, "lobby_button_bar");
	struct overlay_button_colors colors;
	short index;

	overlay_button_colors_get(&colors);
	colors.radius = palette->radius / 2;
	for (index = 0; bar && index < NUMBER_OF_BUTTONS; index++)
	{
		struct widget_instance *button = child_named(bar, button_names[index]);
		boolean lit = button && list->focused_child == bar && bar->focused_child == button;

		if (button && button->visible)
			overlay_button_draw(button_labels[index], (float)button_lefts[index], BUTTONS_TOP, BUTTON_WIDTH, BUTTON_HEIGHT,
				lit, TRUE, &colors);
	}
}

/* how another player joins (split screen), left of the buttons */
static void render_join_help(
	struct overlay_palette const *palette)
{
	wchar_t const *help = pc_menu_lobby_join_help();
	char text[64];
	size_t length;

	/* the label is plain ASCII */
	for (length = 0; help[length] && length < sizeof(text) - 1; length++)
		text[length] = (char)help[length];
	text[length] = 0;
	if (length)
		ui_overlay_text(UI_FONT_REGULAR, 9.0f, ROW_LEFT, BUTTONS_TOP + 6, UI_ALIGN_LEFT, palette->dim, text);
}

/* ---------- public code */

/* whether the lobby is the screen up, which this draws over (this client's
screens: Vanilla keeps the stock lobby) */
boolean lobby_screen_active(
	void)
{
	return ui_overlay_available() && overlay_palette_current()->own_screens && lobby_list() != NULL;
}

/* ui_widget.c, after the menus are drawn */
void lobby_screen_render(
	void)
{
	struct overlay_palette const *palette = overlay_palette_current();
	struct widget_instance *list = lobby_list();
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	struct network_player *const *players;
	short first, player_count = pc_menu_lobby_players(&players, &first);

	if (!list)
		return;
	overlay_screen_frame("LOBBY", ROW_LEFT, 22, 24.0f);
	render_buttons(palette, list);
	render_join_help(palette);
	if (!game)
	{
		ui_overlay_text(UI_FONT_REGULAR, 10.0f, ROW_LEFT + 10, ROW_TOP + 6, UI_ALIGN_LEFT, palette->dim,
			"Joining the game\xE2\x80\xA6");
		return;
	}
	render_header(palette, game, client, player_count);
	render_players(palette, list, game);
	render_panel(palette, game, player_count);
}

#endif
