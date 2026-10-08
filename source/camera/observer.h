/*
OBSERVER.H
*/

#ifndef __OBSERVER_H
#define __OBSERVER_H
#pragma once

/* ---------- headers */

#include "objects/objects.h"

/* ---------- constants */

enum observer_command_flags
{
	_observer_command_valid_bit,
	_observer_command_force_under_media_bit,
	_observer_command_force_above_media_bit,
	_observer_command_force_time_bit,
	_observer_command_ignore_obstructions_bit,
	_observer_command_freeze_camera_bit,
	NUMBER_OF_OBSERVER_COMMAND_FLAGS
};

enum observer_command_parameter
{
	_observer_command_parameter_focus_position,
	_observer_command_parameter_focus_offset,
	_observer_command_parameter_focus_distance,
	_observer_command_parameter_field_of_view,
	_observer_command_parameter_orientation,
	NUMBER_OF_OBSERVER_COMMAND_PARAMETERS
};

enum observer_time_flags
{
	_observer_time_valid_bit = 0,
	_observer_time_force_bit
};

/* ---------- structures */

struct observer_result
{
	real_point3d position;
	struct location location;
	real_vector3d velocity;
	real_vector3d forward;
	real_vector3d up;
	real field_of_view;
};

struct observer_command
{
	long flags;
	union
	{
		struct
		{
			real_point3d focus_position;
			real_vector3d focus_offset;
			real focus_distance;
			real field_of_view;
			real_vector3d forward;
			real_vector3d up;
		};
		real parameters[14];
	};
	real_vector3d focus_velocity;
	real timer;
	byte parameter_flags[NUMBER_OF_OBSERVER_COMMAND_PARAMETERS];
	byte pad51[3];
	real parameter_timers[NUMBER_OF_OBSERVER_COMMAND_PARAMETERS];
};

typedef char observer_command_size_assert[
	sizeof(struct observer_command) == 0x68 ? 1 : -1];
typedef char observer_command_parameters_offset_assert[
	offsetof(struct observer_command, parameters) == 0x04 ? 1 : -1];
typedef char observer_command_parameter_flags_offset_assert[
	offsetof(struct observer_command, parameter_flags) == 0x4C ? 1 : -1];
typedef char observer_command_parameter_timers_offset_assert[
	offsetof(struct observer_command, parameter_timers) == 0x54 ? 1 : -1];

/* ---------- macros */

#define match_assert_valid_observer_command(file, line, command) \
	match_vassert( \
		file, \
		line, \
		(command) && \
		(!TEST_FLAG((command)->flags, _observer_command_valid_bit) || \
		(valid_real_vector3d_axes2(&(command)->forward, &(command)->up) && \
			valid_real((command)->focus_position.x) && (command)->focus_position.x>=-5000.f && (command)->focus_position.x<=5000.f && \
			valid_real((command)->focus_position.y) && (command)->focus_position.y>=-5000.f && (command)->focus_position.y<=5000.f && \
			valid_real((command)->focus_position.z) && (command)->focus_position.z>=-5000.f && (command)->focus_position.z<=5000.f && \
			valid_real((command)->focus_offset.i) && (command)->focus_offset.i>=-5000.f && (command)->focus_offset.i<=5000.f && \
			valid_real((command)->focus_offset.j) && (command)->focus_offset.j>=-5000.f && (command)->focus_offset.j<=5000.f && \
			valid_real((command)->focus_offset.k) && (command)->focus_offset.k>=-5000.f && (command)->focus_offset.k<=5000.f && \
			valid_real_vector3d(&(command)->focus_velocity) && \
			valid_real((command)->focus_distance) && (command)->focus_distance>=0.f && (command)->focus_distance<=5000.f && \
			valid_real((command)->field_of_view) && (command)->field_of_view>=0.001f && (command)->field_of_view<=_pi / 2.f && \
			valid_real((command)->timer) && (command)->timer>=0.f && (command)->timer<=3600.f)), \
		csprintf( \
			temporary, \
			"Invalid camera command.\nF: (%f, %f, %f) U: (%f, %f, %f)\nP: (%f, %f, %f) O: (%f, %f, %f)\nD: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld", \
			(command)->forward.i, \
			(command)->forward.j, \
			(command)->forward.k, \
			(command)->up.i, \
			(command)->up.j, \
			(command)->up.k, \
			(command)->focus_position.x, \
			(command)->focus_position.y, \
			(command)->focus_position.z, \
			(command)->focus_offset.i, \
			(command)->focus_offset.j, \
			(command)->focus_offset.k, \
			(command)->focus_distance, \
			(command)->focus_velocity.i, \
			(command)->focus_velocity.j, \
			(command)->focus_velocity.k, \
			(command)->field_of_view, \
			(command)->timer, \
			(command)->flags))

/* ---------- prototypes/OBSERVER.C */

void observer_obsolete_position(
	short local_player_index);

void observer_initialize(
	void);
void observer_dispose_from_old_map(
	void);
void observer_initialize_for_new_map(
	void);
void observer_reconnect_to_structure_bsp(
	void);
void observer_update(
	real time_delta_sec);

struct observer_result const *observer_get_camera(
	short local_player_index);
boolean observer_command_has_finished(
	short local_player_index);

void observer_up_from_forward(
	real_vector3d const *forward,
	real_vector3d *up);
void observer_set_camera(
	short local_player_index,
	struct observer_command *command);

#endif // __OBSERVER_H
