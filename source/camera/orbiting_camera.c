/*
ORBITING_CAMERA.C
*/

/* ---------- headers */

#include "orbiting_camera.h"
#include "director.h"
#include "observer.h"

#include "game/players.h"
#include "objects/objects.h"

/* ---------- structures */

struct orbiting_camera_constants
{
	real field_of_view;
	real timer;
	real vertical_offset;
};

/* ---------- globals */

static struct orbiting_camera_constants const orbiting_camera_constants =
{
	DEGREES_TO_RADIANS(50.f),
	0.5f,
	0.52f
};

/* ---------- public code */

void orbiting_camera_new(
	struct orbiting_camera *camera,
	real distance,
	real_vector3d const *forward)
{
	camera->distance = distance;
	euler_angles2d_from_vector3d(&camera->facing, forward);
	return;
}

void orbiting_camera_update(
	struct orbiting_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	struct player_control_unit_camera_info camera_info;

	player_control_get_unit_camera_info(controls->local_player_index, &camera_info);
	result->focus_position = camera_info.position;

	if (controls->active)
	{
		camera->facing.yaw -= controls->facing_delta.yaw;
		camera->facing.pitch = PIN(
			camera->facing.pitch - controls->facing_delta.pitch,
			-DEGREES_TO_RADIANS(72.f),
			DEGREES_TO_RADIANS(72.f));
		director_inhibit_input(controls->local_player_index);
	}

	camera->distance = MAX(camera->distance - controls->wheel_delta / 3.f, 0.6f);

	if (camera_info.unit_index != NONE)
	{
		vector3d_from_euler_angles2d(&result->forward, &camera->facing);
		observer_up_from_forward(&result->forward, &result->up);
		object_get_velocities(camera_info.unit_index, &result->focus_velocity, NULL);
		result->focus_position.z += orbiting_camera_constants.vertical_offset;
		result->flags = FLAG(_observer_command_valid_bit);
	}

	result->focus_offset = *global_zero_vector3d;
	result->focus_distance = camera->distance;
	result->field_of_view = orbiting_camera_constants.field_of_view;
	result->timer = orbiting_camera_constants.timer;

	match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\orbiting_camera.c", 71, result);

	return;
}

