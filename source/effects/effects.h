/*
EFFECTS.H
*/

#ifndef __EFFECTS_H
#define __EFFECTS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

/* effect vectors */
enum
{
	_effect_vector_normal,
	_effect_vector_incident,
	_effect_vector_negative_incident,
	_effect_vector_reflected,
	_effect_vector_gravity,
	NUMBER_OF_EFFECT_MARKERS
};

/* ---------- macros */

/* ---------- structures */

struct effect_location_definition
{
	char marker_name[32];
};

struct effect_particles_definition
{
	short environment;
	short disposition;
	short camera_mode;
	short unused006;
	short location_index;
	short unused00a;
	real_euler_angles2d direction;
	real_vector3d offset;
	real_vector3d runtime_direction;
	long unused02c[10];
	struct tag_reference particle;
	unsigned long flags;
	short distribution_function;
	short unused06a;
	short count_lower_bound;
	short count_upper_bound;
	real distribution_radius_lower_bound;
	real distribution_radius_upper_bound;
	long unused078[3];
	real velocity_lower_bound;
	real velocity_upper_bound;
	real velocity_cone_angle;
	real angular_velocity_lower_bound;
	real angular_velocity_upper_bound;
	long unused098[2];
	real radius_lower_bound;
	real radius_upper_bound;
	long unused0a8[2];
	real_argb_color tint_lower_bound;
	real_argb_color tint_upper_bound;
	long unused0d0[4];
	unsigned long scale_a_flags;
	unsigned long scale_b_flags;
};

struct effect_vector_field;
struct effects_information
{
	short effect_count;
	short location_count;
	short active_effect_count;
};

/* ---------- prototypes/EFFECTS.C */

void effects_initialize(
	void);
void effects_initialize_for_new_map(
	void);
void effects_dispose_from_old_map(
	void);
void effects_dispose(
	void);
void effect_delete(
	long effect_index);
void effects_stop_on_first_person_weapon(
	short local_player_index);
void effects_information_get(
	struct effects_information *information);
void effects_disconnect_from_structure_bsp(
	void);
void effects_reconnect_to_structure_bsp(
	void);
void effect_stop(
	long effect_index,
	boolean and_delete);
boolean dangerous_effects_near_player(
	void);
long effect_new_looping(
	long definition_index,
	long object_index,
	short scale_a_function_index,
	short scale_b_function_index,
	short change_color_index);
long effect_new_from_object(
	long definition_index,
	long owner_object_index,
	long object_index,
	short force_local_player_index,
	real scale_a,
	real scale_b,
	real_rgb_color const *color,
	struct effect_vector_field const *impulse_field);
long effect_new_attached_from_markers(
	long definition_index,
	long owner_object_index,
	long object_index,
	short node_index,
	short marker_count,
	char const **marker_names,
	real_point3d const *marker_points,
	real_vector3d const *marker_forwards,
	real scale_a,
	real scale_b,
	real_rgb_color const *color,
	struct effect_vector_field const *impulse_field);
long effect_new_unattached_from_markers(
	long definition_index,
	long owner_object_index,
	real_vector3d const *translational_velocity,
	short marker_count,
	char const **marker_names,
	real_point3d const *marker_points,
	real_vector3d const *marker_forwards,
	real scale_a,
	real scale_b,
	real_rgb_color const *color,
	struct effect_vector_field const *impulse_field,
	boolean can_be_deterministic);
void effects_update(
	real dt);
void effects_start_on_first_person_weapon(
	short local_player_index,
	long object_index);

/* ---------- globals */

extern boolean effects_corpse_nonviolent;
extern boolean debug_effects_nonviolent;

/* ---------- public code */

#endif // __EFFECTS_H
