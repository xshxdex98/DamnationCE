/*
CAMERA_SCRIPTING.C
*/

/* ---------- headers */

#include "camera_scripting.h"
#include "dead_camera.h"
#include "director.h"
#include "first_person_camera.h"
#include "observer.h"

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "math/real_math.h"
#include "models/model_animation_definitions.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	_camera_script_mode_point = 0,
	_camera_script_mode_animation,
	_camera_script_mode_first_person,
	_camera_script_mode_dead,
	NUMBER_OF_CAMERA_SCRIPT_MODES,
};

/* ---------- structures */

struct scripted_camera_globals
{
	boolean enabled;
	boolean first_update;
	short mode;
	short camera_point_index;
	byte pad06[2];
	real timer;
	real_point3d point;
	real_vector3d forward;
	real_vector3d up;
	real field_of_view;
	long relative_object_index;
	long animation_graph_index;
	short animation_index;
};

typedef char scripted_camera_globals_size_assert[
	sizeof(struct scripted_camera_globals) == 0x40 ? 1 : -1];

/* ---------- prototypes */

void scripted_camera_set(
	short camera_point_index,
	word transition_time,
	long relative_object_index);

/* ---------- globals */

static struct scripted_camera_globals camera_script_globals =
{
	FALSE,
	FALSE,
	NONE,
	NONE,
	{ 0, 0 },
	0.f,
	{ { 0.f, 0.f, 0.f } },
	{ { 0.f, 0.f, 1.f } },
	{ { 0.f, 1.f, 0.f } },
	1.22173047f,
	NONE,
	NONE,
	0
};

/* ---------- public code */

void scripted_camera_enable(
	boolean enabled)
{
	camera_script_globals.enabled = enabled;
	camera_script_globals.first_update = TRUE;
	return;
}

void scripted_camera_set_animation(
	long animation_graph_index,
	char const *animation_name)
{
	struct animation_graph *animation_graph;
	struct animation *animation;
	short animation_index;

	if (animation_graph_index != NONE)
	{
		animation_graph = animation_graph_definition_get(animation_graph_index);
		if (animation_graph->nodes.count == 1)
		{
			animation_index = 0;
			while (animation_index < animation_graph->animations.count)
			{
				animation = TAG_BLOCK_GET_ELEMENT(
					&animation_graph->animations,
					animation_index,
					struct animation);
				if (!_stricmp(animation_name, animation->name))
				{
					camera_script_globals.camera_point_index = NONE;
					camera_script_globals.relative_object_index = NONE;
					camera_script_globals.mode = _camera_script_mode_animation;
					camera_script_globals.first_update = TRUE;
					camera_script_globals.animation_index = animation_index;
					camera_script_globals.animation_graph_index = animation_graph_index;
					camera_script_globals.field_of_view = 1.22173047f;
					camera_script_globals.timer =
						(real)(animation->frame_count / TICKS_PER_SECOND);
					break;
				}

				animation_index++;
			}
		}
	}

	return;
}

void scripted_camera_set_first_person(
	long unit_index)
{
	if (unit_index != NONE)
	{
		camera_script_globals.mode = _camera_script_mode_first_person;
		camera_script_globals.first_update = TRUE;
		camera_script_globals.relative_object_index = unit_index;
	}
	else
	{
		error(
			_error_silent,
			"cannot set first person camera on a unit that doesn't exist.");
	}

	return;
}

void scripted_camera_set_dead(
	long unit_index)
{
	if (unit_index != NONE)
	{
		camera_script_globals.mode = _camera_script_mode_dead;
		camera_script_globals.first_update = TRUE;
		camera_script_globals.relative_object_index = unit_index;
	}
	else
	{
		error(
			_error_silent,
			"cannot set first person camera on a unit that doesn't exist.");
	}

	return;
}

boolean scripted_camera_object_is_first_person_camera(
	long object_index)
{
	return camera_script_globals.enabled &&
		camera_script_globals.mode == _camera_script_mode_first_person &&
		camera_script_globals.relative_object_index == object_index;
}

void scripted_camera_set(
	short camera_point_index,
	word transition_time,
	long relative_object_index)
{
	struct scenario_cutscene_camera_point *camera_point;
	long camera_time;

	camera_point = TAG_BLOCK_GET_ELEMENT(
		&global_scenario_get()->cutscene_camera_points,
		camera_point_index,
		struct scenario_cutscene_camera_point);
	camera_time = (short)transition_time / TICKS_PER_SECOND;
	camera_script_globals.mode = _camera_script_mode_point;
	camera_script_globals.first_update = TRUE;
	camera_script_globals.camera_point_index = camera_point_index;
	camera_script_globals.point = camera_point->position;
	vectors3d_from_euler_angles3d(
		&camera_script_globals.forward,
		&camera_script_globals.up,
		&camera_point->orientation);
	if (camera_point->field_of_view != 0.f)
		camera_script_globals.field_of_view = camera_point->field_of_view;
	else
		camera_script_globals.field_of_view = 1.22173047f;
	camera_script_globals.relative_object_index = relative_object_index;
	camera_script_globals.timer = (real)camera_time;

	director_update(0.f);
	observer_update(0.0001f);
	return;
}

void scripted_camera_set_absolute(
	short camera_point_index,
	word transition_time)
{
	scripted_camera_set(camera_point_index, transition_time, NONE);
	return;
}

void scripted_camera_set_camera_point_relative(
	real_point3d const *position,
	real_vector3d const *forward,
	real_vector3d const *up,
	real field_of_view,
	word transition_time,
	long relative_object_index)
{
	camera_script_globals.mode = _camera_script_mode_point;
	camera_script_globals.camera_point_index = NONE;
	camera_script_globals.point = *position;
	camera_script_globals.forward = *forward;
	camera_script_globals.up = *up;
	if (field_of_view != 0.f)
		camera_script_globals.field_of_view = field_of_view;
	else
		camera_script_globals.field_of_view = 1.22173047f;
	camera_script_globals.timer =
		(real)((short)transition_time / TICKS_PER_SECOND);
	camera_script_globals.relative_object_index = relative_object_index;

	director_update(0.f);
	observer_update(0.0001f);
	return;
}

void scripted_camera_set_camera_point_absolute(
	real_point3d const *position,
	real_vector3d const *forward,
	real_vector3d const *up,
	real field_of_view,
	word transition_time)
{
	scripted_camera_set_camera_point_relative(
		position,
		forward,
		up,
		field_of_view,
		transition_time,
		NONE);
	return;
}

short scripted_camera_next_camera_point(
	void)
{
	return camera_script_globals.camera_point_index;
}

long scripted_camera_object_relative_to(
	void)
{
	return camera_script_globals.relative_object_index;
}

short scripted_camera_time(
	void)
{
	return (short)(camera_script_globals.timer * 30.f);
}

void scripted_camera_update(
	struct dead_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result)
{
	real_point3d focus_position;
	real speed;

	focus_position = *global_origin3d;
	speed = game_time_get_speed();
	result->flags = FLAG(_observer_command_force_time_bit);
	if (game_time_get_paused())
	{
		result->flags |= FLAG(_observer_command_freeze_camera_bit);
	}

	switch (camera_script_globals.mode)
	{
	case _camera_script_mode_point:
		if (camera_script_globals.relative_object_index != NONE)
		{
			struct object_datum *object;

			object = object_try_and_get_and_verify_type(
				camera_script_globals.relative_object_index,
				_object_mask_all);
			if (!object)
			{
				break;
			}
			focus_position = object->object.bounding_sphere_center;
		}

		result->timer = speed != 0.f
			? camera_script_globals.timer / speed
			: 0.f;
		result->field_of_view = camera_script_globals.field_of_view;
		result->forward = camera_script_globals.forward;
		result->up = camera_script_globals.up;
		if (camera_script_globals.relative_object_index != NONE)
		{
			real angle;
			real cosine_value;
			real dot;
			real rotated_offset_i;
			real sine_value;
			real_vector3d offset;

			angle = arctangent(result->forward.j, result->forward.i);
			dot = dot_product3d(
				(real_vector3d const *)&camera_script_globals.point,
				&result->forward);
			if (dot > 0.f)
			{
				dot = 0.f;
			}

			result->focus_distance = -dot;
			result->focus_position = focus_position;
			offset.i = camera_script_globals.point.x - dot * result->forward.i;
			offset.j = camera_script_globals.point.y - dot * result->forward.j;
			offset.k = camera_script_globals.point.z - dot * result->forward.k;
			result->parameter_timers[_observer_command_parameter_focus_position] = 0.f;
			result->parameter_flags[_observer_command_parameter_focus_position] = FLAG(_observer_time_valid_bit);
			sine_value = sine(angle);
			cosine_value = cosine(angle);
			rotated_offset_i = offset.i * cosine_value;
			rotated_offset_i += sine_value * offset.j;
			result->focus_offset.i = rotated_offset_i;
			result->focus_offset.j = sine_value * offset.i - cosine_value * offset.j;
			result->focus_offset.k = offset.k;
			result->flags |= FLAG(_observer_command_valid_bit);
		}
		else
		{
			result->focus_position = camera_script_globals.point;
			result->flags |= FLAG(_observer_command_valid_bit);
		}
		break;

	case _camera_script_mode_animation:
		{
			struct animation_graph *animation_graph;
			struct animation *animation;
			short frame;
			long frame_count;
			real_matrix4x3 root_matrix;

			animation_graph = animation_graph_definition_get(
				camera_script_globals.animation_graph_index);
			animation = TAG_BLOCK_GET_ELEMENT(
				&animation_graph->animations,
				camera_script_globals.animation_index,
				struct animation);
			frame_count = animation->frame_count;
			frame = (short)(frame_count - camera_script_globals.timer * 30.f);
			animation_get_root_matrix(
				NULL,
				animation,
				frame < 0 ? 0 : MIN(frame, frame_count - 1),
				&root_matrix);
			result->forward = root_matrix.forward;
			result->up = root_matrix.up;
			result->field_of_view = DEGREES_TO_RADIANS(70.f);
			result->focus_position = root_matrix.position;
			result->focus_distance = 0.f;
			result->timer = 0.f;
			result->flags |= FLAG(_observer_command_valid_bit);
		}
		break;

	case _camera_script_mode_first_person:
		if (object_try_and_get_and_verify_type(
			camera_script_globals.relative_object_index,
			_object_mask_unit))
		{
			first_person_camera_fake(
				camera_script_globals.relative_object_index,
				result);
		}
		break;

	case _camera_script_mode_dead:
		if (object_try_and_get_and_verify_type(
			camera_script_globals.relative_object_index,
			_object_mask_unit))
		{
			if (camera_script_globals.first_update)
			{
				dead_camera_new(
					camera,
					controls->local_player_index,
					camera_script_globals.relative_object_index);
			}
			dead_camera_update(camera, controls, result);
		}
		break;
	}

	camera_script_globals.timer = MAX(
		0.f,
		camera_script_globals.timer - speed * controls->seconds_elapsed);
	camera_script_globals.first_update = FALSE;

	match_assert_valid_observer_command("c:\\halo\\SOURCE\\camera\\camera_scripting.c", 0x172, result);

	return;
}

