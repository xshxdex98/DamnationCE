/*
COOP_SPECTATE.C

A dead player of a co-op game over the network (a campaign map, which no
game engine runs) watches the living until it comes back (players.c: beside
a teammate once it is safe). The campaign's own dead camera watches the
body, then whoever it picks in turn; here it follows a living teammate
instead, from behind as it turns, and A (the keyboard's jump) moves it to the
next. The HUD says whom it is watching (hud.c).
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

#include <math.h>

/* ---------- constants */

/* the camera: how far behind the watched unit, how far above it looks
down (radians), and how quickly it turns after it (of the way a frame) */
#define SPECTATE_DISTANCE 3.5f
#define SPECTATE_PITCH -0.3f
#define SPECTATE_TURN 0.15f

/* ---------- globals */

/* the player each local player watches, or NONE */
static long coop_spectate_watched[MAXIMUM_LOCAL_PLAYERS] = { NONE, NONE, NONE, NONE };

/* (input_abstraction.c: for how many ticks the keyboard's jump key is held) */
byte input_abstraction_port_accept(short controller_index);

/* ---------- private code */

/* the next player after `after` (NONE: from the first) who has a unit, other
than `self`, or NONE */
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

/* whether A, or the keyboard's jump, went down on this frame */
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
	return game_connection() != _game_connection_local && game_connection() != _game_connection_film_playback &&
		!game_engine_running();
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
	/* (alive again: the next death starts from whoever it watched) */
	if (player->unit_index != NONE)
		return NONE;
	if (*watched == NONE || !player_try_and_get(*watched) || player_get(*watched)->unit_index == NONE ||
		next_pressed(player->network_player_data.controller_index))
	{
		*watched = next_living_player(self, *watched);
	}

	return *watched != NONE ? player_get(*watched)->unit_index : NONE;
}

void coop_spectate_camera(
	struct dead_camera *camera)
{
	struct object_datum *unit = camera->unit_index != NONE ? object_try_and_get(camera->unit_index) : NULL;
	real yaw, turn;

	if (!unit)
		return;
	/* (behind it: the camera's facing is the unit's, turned to a little at a time) */
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

void coop_spectate_draw(
	short local_player_index)
{
	long watched = coop_spectate_watched[local_player_index];
	long font_index = hud_get_font_index();
	struct font_header *font;
	real_argb_color color = *global_real_argb_white;
	rectangle2d bounds;
	wchar_t text[96];
	short height;

	if (watched == NONE || !player_try_and_get(watched) || font_index == NONE)
		return;
	font = font_definition_get(font_index);
	height = (short)(font->ascending_height + font->descending_height);
	usnprintf(text, NUMBEROF(text) - 1, L"SPECTATING %.12s   (A: NEXT)\r\nYou come back beside them once it is safe",
		player_get(watched)->name);
	text[NUMBEROF(text) - 1] = 0;
	bounds = render.camera.window_bounds;
	bounds.y0 = (short)(bounds.y1 - 3 * height - 24);
	bounds.y1 = (short)(bounds.y1 - 24);
	/* (centred: 2) */
	draw_string_set_draw_mode(font_index, NONE, 2, 0, &color);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
}
