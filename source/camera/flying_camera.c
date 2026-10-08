/*
FLYING_CAMERA.C
*/

/* ---------- headers */

#include "flying_camera.h"
#include "director.h"
#include "observer.h"

#include "rasterizer/rasterizer_console_vars.h"

/* ---------- public code */

void flying_camera_new(
	struct flying_camera *camera)
{
	camera->position.y = 0.f;
	camera->position.x = 0.f;
	camera->facing.yaw = 0.f;
	camera->facing.pitch = 0.f;
	camera->roll = 0.f;
	camera->field_of_view = DEGREES_TO_RADIANS(70);
	return;
}

void flying_camera_new_from_point_and_vector(
	struct flying_camera *camera,
	real_point3d const *position,
	real_vector3d const *forward)
{
	flying_camera_new(camera);
	camera->position = *position;
	euler_angles2d_from_vector3d(&camera->facing, forward);
	return;
}

void flying_camera_update(
	struct flying_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	match_assert("c:\\halo\\SOURCE\\camera\\flying_camera.c", 41, camera);
	match_assert("c:\\halo\\SOURCE\\camera\\flying_camera.c", 42, controls);
	match_assert("c:\\halo\\SOURCE\\camera\\flying_camera.c", 43, result);

	if (controls->active)
	{
		camera->facing.yaw += controls->facing_delta.yaw;
		camera->facing.pitch = PIN(
			camera->facing.pitch + controls->facing_delta.pitch,
			-1.56765485f,
			1.56765485f);
		camera->roll += controls->facing_delta.roll;
	}

	if (rasterizer_debug_options.freeze_flying_camera > 0)
	{
		camera->facing.yaw = 0.f;
		camera->facing.pitch = 0.f;
		camera->roll = 0.f;
		rasterizer_debug_options.freeze_flying_camera--;
	}

	result->timer = 0.3f;
	vector3d_from_euler_angles2d(&result->forward, &camera->facing);
	observer_up_from_forward(&result->forward, &result->up);
	rotate_vector_about_axis(
		&result->up,
		&result->forward,
		sine(camera->roll),
		cosine(camera->roll));

	if (controls->active)
	{
		real cosine_yaw = cosine(camera->facing.yaw);
		real sine_yaw = sine(camera->facing.yaw);
		real_vector3d translation;
		real_point3d position;

		set_real_vector3d(
			&translation,
			cosine_yaw*controls->position_delta.i - sine_yaw*controls->position_delta.j,
			cosine_yaw*controls->position_delta.j + sine_yaw*controls->position_delta.i,
			controls->position_delta.k);
		position.x = camera->position.x + translation.i;
		position.y = camera->position.y + translation.j;
		position.z = camera->position.z + translation.k;
		camera->position = position;
	}

	result->focus_position = camera->position;
	result->focus_offset = *global_zero_vector3d;
	result->focus_distance = 0.f;
	result->field_of_view = camera->field_of_view;
	result->flags = FLAG(_observer_command_valid_bit);

	match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\flying_camera.c", 149, result);

	return;
}

