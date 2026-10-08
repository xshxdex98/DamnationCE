/*
STATIC_CAMERA.C
*/

/* ---------- headers */

#include "static_camera.h"
#include "director.h"
#include "observer.h"

/* ---------- public code */

void static_camera_new(
	struct static_camera *camera,
	real_point3d const *position,
	long location,
	real_vector3d const *forward,
	real_vector3d const *up,
	real field_of_view,
	long timer,
	long flags)
{
	camera->position = *position;
	camera->location = location;
	camera->forward = *forward;
	camera->up = *up;
	camera->field_of_view = field_of_view;
	camera->timer = timer;
	camera->initialized = FALSE;
	camera->flags = flags;
	return;
}

void static_camera_update(
	struct static_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	match_assert("c:\\halo\\SOURCE\\camera\\static_camera.c", 36, camera);
	match_assert("c:\\halo\\SOURCE\\camera\\static_camera.c", 37, controls);
	match_assert("c:\\halo\\SOURCE\\camera\\static_camera.c", 38, result);

	if (!camera->initialized)
	{
		result->focus_position = camera->position;
		result->forward = camera->forward;
		result->up = camera->up;
		result->field_of_view = camera->field_of_view;
		result->timer = (real)camera->timer;
		result->focus_velocity.i = result->focus_velocity.j = result->focus_velocity.k = 0.f;
		result->flags = camera->flags | FLAG(_observer_command_valid_bit);
		result->focus_offset = *global_zero_vector3d;
		camera->initialized = TRUE;

		match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\static_camera.c", 53, result);
	}

	return;
}

