/*
DEAD_CAMERA.C
*/

/* ---------- headers */

#include "dead_camera.h"
#include "director.h"
#include "observer.h"

#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "objects/objects.h"

/* ---------- structures */

struct dead_camera_constants
{
	real dead_timer;
	real multiplayer_switch_timer;
	real singleplayer_switch_timer;
};

/* ---------- prototypes */

static boolean player_has_allies(
	long player_index);
static long player_get_next_player_with_a_unit(
	long player_index,
	long old_player_index,
	boolean match_team);

/* ---------- globals */

static struct dead_camera_constants const dead_camera_constants =
{
	3.f,
	15.f,
	3.f
};

/* ---------- public code */

void dead_camera_new(
	struct dead_camera *camera,
	short local_player_index,
	long unit_index)
{
	struct observer_result const *observer = observer_get_camera(local_player_index);
	real pitch;
	real switch_timer;

	match_assert("c:\\halo\\SOURCE\\camera\\dead_camera.c", 23, camera);
	camera->position = observer->position;
	camera->field_of_view = DEGREES_TO_RADIANS(70.f);
	camera->distance = real_local_random_range(2.f, 6.f);
	camera->facing.yaw = real_local_random_range(0.f, 2.f * _pi);
	pitch = real_local_random_range(0.47123894f, 1.0995574f);
	camera->facing.pitch = -pitch;
	camera->timer = dead_camera_constants.dead_timer;
	if (unit_index != NONE)
	{
		switch_timer = FLT_MAX;
	}
	else
	{
		switch_timer = game_engine_running()
			? dead_camera_constants.multiplayer_switch_timer
			: dead_camera_constants.singleplayer_switch_timer;
	}
	camera->switch_timer = switch_timer;
	camera->player_index = local_player_get_player_index(local_player_index);
	camera->unit_index = unit_index == NONE
		? player_get(camera->player_index)->dead_unit_index
		: unit_index;
	camera->current_player_index = camera->player_index;

	return;
}

void dead_camera_update(
	struct dead_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	struct object_datum *unit;

	unit = camera->unit_index == NONE
		? NULL
		: object_try_and_get(camera->unit_index);
	if (unit)
	{
		result->focus_position = unit->object.bounding_sphere_center;
	}
	else
	{
		result->focus_position = camera->position;
	}

	result->focus_distance = camera->distance;
	vector3d_from_euler_angles2d(&result->forward, &camera->facing);
	observer_up_from_forward(&result->forward, &result->up);
	result->field_of_view = camera->field_of_view;
	result->focus_offset = *global_zero_vector3d;
	result->focus_velocity = *global_zero_vector3d;
	result->flags = FLAG(_observer_command_valid_bit);
	result->timer = MAX(0.f, camera->timer);
	result->parameter_timers[_observer_command_parameter_focus_position] = 0.f;
	result->parameter_flags[_observer_command_parameter_focus_position] = FLAG(_observer_time_valid_bit) | FLAG(_observer_time_force_bit);

	if (camera->timer == dead_camera_constants.dead_timer)
	{
		result->focus_distance = 0.5f;
		result->parameter_timers[_observer_command_parameter_focus_distance] = 0.f;
		result->parameter_flags[_observer_command_parameter_focus_distance] = FLAG(_observer_time_valid_bit) | FLAG(_observer_time_force_bit);
	}

	camera->timer -= controls->seconds_elapsed;
	camera->switch_timer = MAX(
		0.f,
		camera->switch_timer - controls->seconds_elapsed);

	if (0.f == camera->switch_timer && !game_time_get_paused())
	{
		long next_player_index;
		long next_unit_index;
		boolean match_team;

		match_team = player_has_allies(camera->player_index);
		next_player_index = player_get_next_player_with_a_unit(
			camera->player_index,
			camera->current_player_index,
			match_team);
		camera->current_player_index = next_player_index;

		if (next_player_index != NONE)
		{
			next_unit_index = player_get(next_player_index)->unit_index;
		}
		else
		{
			next_unit_index = NONE;
		}
		/* With no living player, keep observing the dead unit/current position. */

		if (next_unit_index != camera->unit_index && next_unit_index != NONE)
		{
			camera->timer = dead_camera_constants.dead_timer;
			camera->unit_index = next_unit_index;
		}

		camera->switch_timer = game_engine_running()
			? dead_camera_constants.multiplayer_switch_timer
			: dead_camera_constants.singleplayer_switch_timer;
	}

	match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\dead_camera.c", 158, result);

	return;
}

/* ---------- private code */

static boolean player_has_allies(
	long player_index)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long team_index;
	boolean found;

	team_index = player_get(player_index)->team_index;
	found = FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (iterator.datum_index != player_index && player->team_index == team_index)
		{
			found = TRUE;
			break;
		}
	}

	return found;
}

static long player_get_next_player_with_a_unit(
	long player_index,
	long old_player_index,
	boolean match_team)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long first_player_index;
	long next_player_index;
	long team_index;

	team_index = match_team ? player_get(player_index)->team_index : NONE;
	first_player_index = NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (iterator.datum_index != player_index &&
			player->unit_index != NONE &&
			(!match_team || player->team_index == team_index))
		{
			if (first_player_index == NONE)
			{
				first_player_index = iterator.datum_index;
			}
			else if ((iterator.datum_index & 0xFFFF) >
				(old_player_index & 0xFFFF))
			{
				first_player_index = iterator.datum_index;
				break;
			}
		}
	}

	next_player_index = first_player_index == NONE
		? old_player_index
		: first_player_index;
	return next_player_index;
}
