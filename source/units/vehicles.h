/*
VEHICLES.H
*/

#ifndef __VEHICLES_H
#define __VEHICLES_H
#pragma once

/* ---------- headers */

#include "units.h"
#include "models/model_animation_definitions.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

/* vehicle types */
enum
{
	_vehicle_type_human_tank,
	_vehicle_type_human_jeep,
	_vehicle_type_human_boat,
	_vehicle_type_human_plane,
	_vehicle_type_alien_scout,
	_vehicle_type_alien_fighter,
	_vehicle_type_turret,
	NUMBER_OF_VEHICLE_TYPES
};

enum vehicle_flags
{
	_vehicle_blurred_bit = 0,
	_vehicle_hovering_bit,
	_vehicle_control_crouch_bit,
	_vehicle_control_jump_bit,
	_vehicle_upending_bit,
	NUMBER_OF_VEHICLE_FLAGS,
};

/* ---------- macros */

#define vehicle_get(index) ((struct unit_datum *)object_get_and_verify_type((index), _object_mask_vehicle))
#define vehicle_try_and_get(index) ((struct unit_datum *)object_try_and_get_and_verify_type((index), _object_mask_vehicle))
#define vehicle_runtime_get(index) ((struct vehicle_runtime_datum *)object_get_and_verify_type((index), _object_mask_vehicle))

/* ---------- structures */

struct vehicle_suspension
{
	short mass_point_index;
	short animation_index;
	real unknown4;
	real unknown8;
	byte unknownc[8];
};

struct vehicle_animation
{
	struct animation_aiming_screen_bounds steering_screen_bounds;
	long unused[0x11];
	struct tag_block animations;
	struct tag_block suspensions;
};

struct vehicle_runtime_datum;

/* ---------- prototypes/VEHICLES.C */

void vehicle_hover(
	long vehicle_index,
	boolean hover);
void vehicle_reset(
	long object_index);
boolean vehicle_new(
	long object_index);

void vehicles_initialize(
	void);
void vehicles_initialize_for_new_map(
	void);
void vehicles_dispose_from_old_map(
	void);
void vehicles_dispose(
	void);
boolean vehicle_moving_near_any_player(
	void);
boolean vehicle_stuck(
	long vehicle_index,
	real_vector3d *direction);
void vehicle_export_function_values(
	long object_index);
void vehicle_delete(
	long vehicle_index);
boolean vehicle_causes_collision_damage(
	long vehicle_index);
long vehicle_find_pathfinding_surface_index(
	long vehicle_index,
	real_point3d *position);
void vehicle_preprocess_node_orientations(
	long object_index,
	struct real_orientation *node_orientations);
boolean vehicle_update(
	long object_index);
void vehicle_accelerate(
	long vehicle_index,
	real_vector3d const *acceleration);
void vehicle_render_debug(
	long object_index);

/* ---------- globals */

/* ---------- public code */

#endif // __VEHICLES_H
