/*
BORED_CAMERA.C
*/

/* ---------- headers */

#include "bored_camera.h"
#include "director.h"
#include "observer.h"

#include "cseries/cseries_windows.h"
#include "game/players.h"
#include "math/real_math.h"
#include "units/unit_definitions.h"
#include "units/units.h"

/* ---------- prototypes */

static long bored_camera_shot_threshold_milliseconds(
	long boredom_count);
static long bored_camera_shot_duration_milliseconds(
	long boredom_count);

/* ---------- public code */

void bored_camera_new(
	struct bored_camera *camera)
{
	camera->boredom_count = 0;
	camera->last_update_milliseconds = system_milliseconds();
	camera->timer_milliseconds = 0;
	return;
}

void bored_camera_update(
	struct bored_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	unsigned long now;

	now = system_milliseconds();
	match_assert("c:\\halo\\SOURCE\\camera\\bored_camera.c", 51, camera);
	match_assert("c:\\halo\\SOURCE\\camera\\bored_camera.c", 52, result);
	camera->timer_milliseconds += camera->last_update_milliseconds - now;
	camera->last_update_milliseconds = now;

	if (camera->timer_milliseconds < bored_camera_shot_threshold_milliseconds(camera->boredom_count))
	{
		struct player_control_unit_camera_info camera_info;
		long aiming_unit_index;

		aiming_unit_index = player_control_get_aiming_unit_index(
			controls->local_player_index);
		player_control_get_unit_camera_info(
			controls->local_player_index,
			&camera_info);
		result->focus_position = camera_info.position;

		if (aiming_unit_index != NONE)
		{
			long timer_milliseconds;
			real_point3d camera_position;
			real_euler_angles2d angles;
			real field_of_view;

			if (camera_info.camera->unit_camera_tracks.count)
			{
				TAG_BLOCK_GET_ELEMENT(
					&camera_info.camera->unit_camera_tracks,
					0,
					struct unit_camera_track);
			}

			angles = *player_control_get_facing_angles(
				controls->local_player_index);
			unit_get_camera_position(aiming_unit_index, &camera_position);
			angles.pitch = real_local_random_range(
				-DEGREES_TO_RADIANS(63.f),
				DEGREES_TO_RADIANS(22.5f));
			angles.yaw += real_local_random_range(
				-DEGREES_TO_RADIANS(45.f),
				DEGREES_TO_RADIANS(45.f)) + _pi;
			vector3d_from_euler_angles2d(&result->forward, &angles);
			observer_up_from_forward(&result->forward, &result->up);
			field_of_view = real_local_random_range(
				DEGREES_TO_RADIANS(30.f),
				DEGREES_TO_RADIANS(80.f));
			result->field_of_view = field_of_view;
			result->focus_distance = real_local_random_range(1.f, 6.f);
			result->focus_velocity = *global_zero_vector3d;

			timer_milliseconds = bored_camera_shot_duration_milliseconds(camera->boredom_count);
			camera->timer_milliseconds = timer_milliseconds;
			result->flags = FLAG(_observer_command_valid_bit);
			result->timer = (real)timer_milliseconds;
			camera->boredom_count++;

			/* (as the original: the timer can be set to 10000..30000 here, where the check below
			 * allows 3600 at most) */
			match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\bored_camera.c", 95, result);
		}
	}
	return;
}

/* ---------- private code */

static long bored_camera_shot_threshold_milliseconds(
	long boredom_count)
{
	return MIN(boredom_count, 3) * 1000;
}

static long bored_camera_shot_duration_milliseconds(
	long boredom_count)
{
	return MIN(boredom_count + 1, 3) * 10000;
}

boolean is_bored(
	void)
{
	return FALSE;
}

boolean is_still_bored(
	void)
{
	return FALSE;
}
