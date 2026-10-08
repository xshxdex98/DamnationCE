/*
FOLLOWING_CAMERA.C
*/

/* ---------- headers */

#include "following_camera.h"
#include "camera_track_definitions.h"
#include "director.h"
#include "observer.h"

#include "cseries/errors.h" /* port: (error, for a short camera track) */
#include "game/game_globals.h"
#include "game/players.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "tag_files/tag_groups.h"
#include "units/unit_definitions.h"
#include "units/units.h"
#include "units/vehicle_definitions.h"
#include "units/vehicles.h"

/* ---------- prototypes */

static struct unit_camera const *unit_camera_get(
	long unit_index);
static void camera_track_splut(
	struct unit_camera const *camera,
	real pitch,
	real_vector3d *offset);

/* ---------- globals */

real following_camera_zoom_levels[4] =
{
	31.29f,
	12.78f,
	5.13f,
	2.05f
};

/* ---------- public code */

void following_camera_new(
	struct following_camera *camera)
{
	match_assert("c:\\halo\\SOURCE\\camera\\following_camera.c", 19, camera);
	camera->initialized = FALSE;
	camera->confined = FALSE;
	camera->crouched = FALSE;
	camera->zoomed = FALSE;
	camera->zoom_level = 0;
	camera->facing_offset.pitch = 0.f;
	camera->facing_offset.yaw = 0.f;
	camera->unit_index = NONE;
	camera->seat_index = NONE;
	camera->distance_scale = 1.f;
	return;
}

void following_camera_deterministic(
	long unit_index,
	real_point3d *position,
	real_vector3d *forward)
{
	struct unit_datum *unit;
	struct unit_camera const *camera;
	real_vector3d offset;
	real pitch;
	real forward_x;
	real forward_y;
	real horizontal_magnitude;

	unit = unit_get(unit_index);
	camera = unit_camera_get(unit_index);
	unit_get_camera_position(unit_index, position);
	*forward = unit->unit.aiming_vector;

	pitch = arcsine(forward->k);
	camera_track_splut(camera, pitch, &offset);

	forward_x = forward->i;
	forward_y = forward->j;
	horizontal_magnitude = square_root(
		forward_y * forward_y + forward_x * forward_x);
	if (!(0.0001f > fabs(horizontal_magnitude)))
	{
		horizontal_magnitude = 1.f / horizontal_magnitude;
		forward_x *= horizontal_magnitude;
		forward_y *= horizontal_magnitude;
	}

	position->x = offset.j * forward_y + offset.i * forward_x + position->x;
	position->y = offset.i * forward_y - offset.j * forward_x + position->y;
	position->z += offset.k;

	return;
}

void following_camera_update(
	struct following_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	struct player_control_unit_camera_info camera_info;

	match_assert("c:\\halo\\SOURCE\\camera\\following_camera.c", 138, camera);
	match_assert("c:\\halo\\SOURCE\\camera\\following_camera.c", 139, result);

	player_control_get_unit_camera_info(controls->local_player_index, &camera_info);

	result->focus_position = camera_info.position;
	result->timer = 0.f;
	result->flags = 0;
	result->field_of_view = DEGREES_TO_RADIANS(70.f);

	if (camera->initialized &&
		(camera_info.unit_index != camera->unit_index ||
			camera_info.seat_index != camera->seat_index))
	{
		result->timer = 1.f;
	}
	camera->unit_index = camera_info.unit_index;
	camera->seat_index = camera_info.seat_index;

	if (camera_info.camera)
	{
		struct unit_datum *unit;
		boolean crouched;
		real_euler_angles2d facing;
		real_vector3d track_offset;

		unit = unit_get(camera_info.unit_index);
		crouched =
			TEST_FLAG(unit->unit.control_flags, _unit_control_crouch_modifier_bit) ||
			TEST_FLAG(unit->unit.control_flags, _unit_control_jump_bit);
		if (crouched != camera->crouched)
		{
			result->parameter_flags[_observer_command_parameter_focus_offset] = FLAG(_observer_time_valid_bit);
			result->parameter_timers[_observer_command_parameter_focus_offset] = MAX(0.5f, result->parameter_timers[_observer_command_parameter_focus_offset]);
			camera->crouched = crouched;
		}

		if (controls->active)
		{
			camera->facing_offset.yaw += controls->facing_delta.yaw;
			camera->facing_offset.pitch += controls->facing_delta.pitch;
			result->parameter_flags[_observer_command_parameter_orientation] = FLAG(_observer_time_valid_bit);
			result->parameter_timers[_observer_command_parameter_orientation] = MAX(0.4f, result->parameter_timers[_observer_command_parameter_orientation]);
		}
		else if (camera->facing_offset.yaw != 0.f || camera->facing_offset.pitch != 0.f)
		{
			camera->facing_offset.pitch = 0.f;
			camera->facing_offset.yaw = 0.f;
		}

		facing = *player_control_get_facing_angles(controls->local_player_index);
		facing.yaw += camera->facing_offset.yaw;
		facing.pitch = PIN(
			facing.pitch + camera->facing_offset.pitch,
			-_pi / 2.f,
			_pi / 2.f);
		vector3d_from_euler_angles2d(&result->forward, &facing);

		match_vassert(
			"c:\\halo\\SOURCE\\camera\\following_camera.c",
			212,
			magnitude3d(&result->forward) > 0.9999f &&
			magnitude3d(&result->forward) < 1.0001f,
			"magnitude3d(&result->forward) > 0.9999f && magnitude3d(&result->forward) < 1.0001f");

		camera_track_splut(camera_info.camera, facing.pitch, &track_offset);
		result->focus_distance = magnitude3d(&track_offset);
		result->focus_offset.i =
			(result->focus_distance * cosine(facing.pitch) + track_offset.i) * camera->distance_scale;
		result->focus_offset.j = -track_offset.j * camera->distance_scale;
		result->focus_offset.k =
			(result->focus_distance * sine(facing.pitch) + track_offset.k) * camera->distance_scale;
		result->focus_distance = MAX(
			(result->focus_distance - 0.6f) * camera->distance_scale + 0.6f,
			0.6f);

		object_get_velocities(camera_info.unit_index, &result->focus_velocity, NULL);
		SET_FLAG(result->flags, _observer_command_valid_bit, TRUE);
	}

	observer_up_from_forward(&result->forward, &result->up);

	match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\following_camera.c", 238, result);

	camera->initialized = TRUE;
	return;
}

/* ---------- private code */

static struct unit_camera const *unit_camera_get(
	long unit_index)
{
	struct unit_datum *unit;
	struct unit_camera const *camera;

	unit = unit_get(unit_index);
	camera = NULL;
	if (unit->object.parent_object_index != NONE)
	{
		struct unit_datum *vehicle;

		vehicle = vehicle_try_and_get(unit->object.parent_object_index);
		if (vehicle)
		{
			struct unit_definition *definition;
			struct unit_seat *seat;

			definition = vehicle_definition_get(vehicle->definition_index);
			seat = TAG_BLOCK_GET_ELEMENT(
				&definition->unit.seats,
				unit->unit.parent_seat_index,
				struct unit_seat);
			if (seat->flags &
				(FLAG(_unit_seat_invisible_bit) |
					FLAG(_unit_seat_driver_bit) |
					FLAG(_unit_seat_third_person_camera_bit)))
			{
				camera = &seat->camera;
			}
		}
	}

	if (!camera)
		camera = &unit_definition_get(unit->definition_index)->unit.camera;

	return camera;
}

static void camera_track_splut(
	struct unit_camera const *camera,
	real pitch,
	real_vector3d *offset)
{
	long camera_track_index;
	struct camera_track_definition *camera_track;
	long control_point_count;
	long frame_index;
	real h;
	real t;
	short control_point_index;

	camera_track_index = NONE;
	if (camera->unit_camera_tracks.count)
	{
		struct unit_camera_track *track;
		long track_index;

		track_index = MIN(0, camera->unit_camera_tracks.count - 1);
		track = TAG_BLOCK_GET_ELEMENT(
			&camera->unit_camera_tracks,
			track_index,
			struct unit_camera_track);
		if (track)
			camera_track_index = track->track.index;
	}

	if (camera_track_index == NONE)
	{
		struct tag_reference *default_camera_track;

		default_camera_track = TAG_BLOCK_GET_ELEMENT(
			&scenario_get_game_globals()->camera,
			0,
			struct tag_reference);
		camera_track_index = default_camera_track->index;
	}

	camera_track = (struct camera_track_definition *)tag_get(
		CAMERA_TRACK_DEFINITION_TAG,
		camera_track_index);
	t = (pitch + _pi / 2.f) * (1.f / _pi);
	control_point_count = camera_track->control_points.count;
	/* port: the curve below reads four control points, and a custom map's
	track can have fewer: the camera goes between the points it has (none:
	no offset) rather than reading past them (NaN with one point) */
	if (control_point_count < 4)
	{
		static boolean short_track_reported = FALSE;
		struct camera_track_control_point *start_point;
		struct camera_track_control_point *end_point;
		real span;
		real fraction;
		long sample_index;

		if (!short_track_reported)
		{
			error(_error_silent,
				"following camera: a camera track has %ld control points, and the curve wants four, so the camera uses the points it has",
				control_point_count);
			short_track_reported = TRUE;
		}
		if (control_point_count < 1)
		{
			offset->i = 0.f;
			offset->j = 0.f;
			offset->k = 0.f;
			return;
		}

		span = (control_point_count - 1) * PIN(t, 0.f, 1.f);
		sample_index = (long)span;
		fraction = span - sample_index;
		if (sample_index >= control_point_count - 1)
		{
			sample_index = control_point_count - 1;
			fraction = 0.f;
		}
		/* (a pitch not a number) */
		else if (sample_index < 0)
		{
			sample_index = 0;
			fraction = 0.f;
		}
		start_point = TAG_BLOCK_GET_ELEMENT(
			&camera_track->control_points,
			sample_index,
			struct camera_track_control_point);
		end_point = TAG_BLOCK_GET_ELEMENT(
			&camera_track->control_points,
			MIN(sample_index + 1, control_point_count - 1),
			struct camera_track_control_point);
		offset->i = start_point->position.i + (end_point->position.i - start_point->position.i) * fraction;
		offset->j = start_point->position.j + (end_point->position.j - start_point->position.j) * fraction;
		offset->k = start_point->position.k + (end_point->position.k - start_point->position.k) * fraction;
		return;
	}
	frame_index = (long)((control_point_count - 1) * t);
	control_point_index = (short)frame_index;
	h = 1.f / (control_point_count - 1);

	match_assert(
		"c:\\halo\\SOURCE\\camera\\following_camera.c",
		86,
		camera_track->control_points.count >= 4);

	while (control_point_index > 0 &&
		(control_point_index + 4 > camera_track->control_points.count ||
			control_point_index > (short)frame_index - 1))
	{
		control_point_index--;
	}

	uniform_cubic_spline_vector3d(
		offset,
		&TAG_BLOCK_GET_ELEMENT(
			&camera_track->control_points,
			control_point_index,
			struct camera_track_control_point)->position,
		&TAG_BLOCK_GET_ELEMENT(
			&camera_track->control_points,
			control_point_index + 1,
			struct camera_track_control_point)->position,
		&TAG_BLOCK_GET_ELEMENT(
			&camera_track->control_points,
			control_point_index + 2,
			struct camera_track_control_point)->position,
		&TAG_BLOCK_GET_ELEMENT(
			&camera_track->control_points,
			control_point_index + 3,
			struct camera_track_control_point)->position,
		control_point_index * h,
		h,
		t);

	return;
}
