/*
COOP_SPECTATE.C

Spectating in network co-op. A dead player watches a living teammate
until they respawn (players.c respawns them beside a teammate once it is
safe). The campaign's dead camera would watch the body; instead this
follows a teammate from behind, and A (jump on the keyboard) switches to
the next one. director.c and hud.c call in here.

It also draws the line telling players about the vote to skip a cutscene
(network_coop.c counts the votes).
*/

#include "cseries.h"
#include "camera/dead_camera.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "input/input.h"
#include "interface/hud_messaging.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

#include "coop_spectate.h"
#include "network_coop.h"

#include <math.h>

/* ---------- constants */

/* camera distance behind the unit, pitch (radians), and how much of the
turn toward the unit's facing it makes each frame */
#define SPECTATE_DISTANCE 3.5f
#define SPECTATE_PITCH -0.3f
#define SPECTATE_TURN 0.15f

/* ---------- globals */

/* the player each local player is watching, or NONE */
static long coop_spectate_watched[MAXIMUM_LOCAL_PLAYERS] = { NONE, NONE, NONE, NONE };

/* input_abstraction.c: how many ticks the keyboard's jump key has been held */
byte input_abstraction_port_accept(short controller_index);

/* ---------- private code */

/* The next living player after `after` (or the first, if `after` is NONE),
skipping `self` and wrapping around. NONE if nobody else is alive. */
static long next_living_player(long self, long after)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long first = NONE;
	boolean passed = after == NONE;

	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		if (iterator.datum_index == after)
		{
			passed = TRUE;
			continue;
		}
		if (iterator.datum_index == self || player->unit_index == NONE)
			continue;
		if (passed)
			return iterator.datum_index;
		if (first == NONE)
			first = iterator.datum_index;
	}

	return first;
}

/* whether A or the keyboard's jump was pressed this frame */
static boolean next_pressed(short controller_index)
{
	struct gamepad_state const *gamepad;

	if (controller_index < 0 || controller_index >= MAXIMUM_GAMEPADS)
		return FALSE;
	gamepad = input_get_gamepad_state(controller_index);
	return (gamepad && gamepad->buttons[FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_a] == 1) ||
		input_abstraction_port_accept(controller_index) == 1;
}

/* ---------- public code */

boolean coop_spectating(
	void)
{
	return network_coop_active();
}

long coop_spectate_unit(
	short local_player_index)
{
	long self = local_player_get_player_index(local_player_index);
	long *watched = &coop_spectate_watched[local_player_index];
	struct player_datum *player;

	if (self == NONE)
		return NONE;
	player = player_get(self);
	/* alive: keep `watched`, so the next death starts on the same teammate */
	if (player->unit_index != NONE)
		return NONE;
	if (*watched == NONE || !player_try_and_get(*watched) || player_get(*watched)->unit_index == NONE ||
		next_pressed(player->network_player_data.controller_index))
	{
		*watched = next_living_player(self, *watched);
	}

	return *watched != NONE ? player_get(*watched)->unit_index : NONE;
}

boolean coop_spectate_nothing_to_watch(
	short local_player_index)
{
	long self = local_player_get_player_index(local_player_index);

	return self != NONE && player_get(self)->unit_index == NONE && next_living_player(self, NONE) == NONE;
}

void coop_spectate_camera(
	struct dead_camera *camera)
{
	struct object_datum *unit = camera->unit_index != NONE ? object_try_and_get(camera->unit_index) : NULL;
	real yaw, turn;

	if (!unit)
		return;
	/* ease the camera round behind the unit */
	yaw = (real)atan2(unit->object.forward.j, unit->object.forward.i);
	turn = yaw - camera->facing.yaw;
	while (turn > _pi)
		turn -= 2.0f * _pi;
	while (turn < -_pi)
		turn += 2.0f * _pi;
	camera->facing.yaw += turn * SPECTATE_TURN;
	camera->facing.pitch = SPECTATE_PITCH;
	camera->distance = SPECTATE_DISTANCE;
}

/* centred text near the bottom of the screen, in the HUD's font */
static void draw_bottom_text(
	wchar_t const *text)
{
	long font_index = hud_get_font_index();
	struct font_header *font;
	real_argb_color color = *global_real_argb_white;
	rectangle2d bounds;
	short height;

	if (font_index == NONE)
		return;
	font = font_definition_get(font_index);
	height = (short)(font->ascending_height + font->descending_height);
	bounds = render.camera.window_bounds;
	bounds.y0 = (short)(bounds.y1 - 3 * height - 24);
	bounds.y1 = (short)(bounds.y1 - 24);
	/* 2: centred */
	draw_string_set_draw_mode(font_index, NONE, 2, 0, &color);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
}

void coop_spectate_draw(
	short local_player_index)
{
	long watched = coop_spectate_watched[local_player_index];
	wchar_t text[96];
	wchar_t const *hint;

	if (watched == NONE || !player_try_and_get(watched))
		return;
	if (!players_coop_waiting_to_start(local_player_get_player_index(local_player_index)))
		hint = L"You come back beside them once it is safe";
	else if (players_coop_waits_for_checkpoint())
		hint = L"You join them at the level's first checkpoint";
	else
		hint = L"You join them once there is room";
	usnprintf(text, NUMBEROF(text) - 1, L"SPECTATING %.12s   (A: NEXT)\r\n%s", player_get(watched)->name, hint);
	text[NUMBEROF(text) - 1] = 0;
	draw_bottom_text(text);
}

void coop_skip_vote_draw(
	short local_player_index)
{
	short votes, voters;
	boolean voted;
	wchar_t text[96];

	/* once, for the first local player */
	if (local_player_index != local_player_get_next(NONE) || !network_coop_skip_vote_status(&votes, &voters, &voted))
		return;
	if (voted)
		usnprintf(text, NUMBEROF(text) - 1, L"VOTED TO SKIP   %d OF %d", votes, voters);
	else
		usnprintf(text, NUMBEROF(text) - 1, L"PRESS SPACE OR A TO VOTE TO SKIP   %d OF %d", votes, voters);
	text[NUMBEROF(text) - 1] = 0;
	draw_bottom_text(text);
}
