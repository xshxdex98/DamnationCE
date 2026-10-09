/*
VEHICLE_DEFINITIONS.H
*/

#ifndef __VEHICLE_DEFINITIONS_H
#define __VEHICLE_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "unit_definitions.h"
#include "physics/physics_variables.h"

/* ---------- constants */

/* a vehicle definition's AI flags */
enum
{
	_vehicle_ai_weapon_cannot_rotate_bit = 8,
	_vehicle_ai_driver_enable_bit = 11,
	_vehicle_ai_driver_flying_bit,
	_vehicle_ai_driver_nondirectional_bit,
	_vehicle_ai_driver_hovering_bit
};

enum
{
	VEHICLE_DEFINITION_TAG = 'vehi'
};

enum
{
	_vehicle_causes_collision_damage_bit = 7,
};

/* ---------- macros */

#define vehicle_definition_get(index) ((struct unit_definition *)tag_get(VEHICLE_DEFINITION_TAG, index))
#define vehicle_specific_definition_get(index) ((struct vehicle_definition *)tag_get(VEHICLE_DEFINITION_TAG, index))

/* ---------- structures */

struct vehicle_definition
{
	struct unit_definition unit;
	unsigned long flags;
	short vehicle_type;
	short pad2f6;
	struct physics_variable_speed_parameters speed;
	real maximum_left_turn;
	real maximum_right_turn;
	real wheel_circumference;
	real turn_rate;
	real unknown318;
	short function_modes[4];
	byte unknown324[0xc];
	real maximum_left_slide;
	real maximum_right_slide;
	byte unused338[8];
	real unknown340;
	real unknown344;
	byte unused348[0x1c];
	real fixed_gun_pitch;
	byte unused368[0x18];
	real ai_sideslip_distance;
	real ai_destination_radius;
	real ai_avoidance_distance;
	real ai_pathfinding_radius;
	real ai_charge_repeat_time;
	real ai_strafing_stop_range;
	real ai_oversteer_angle_lower_bound;
	real ai_oversteer_angle_upper_bound;
	real ai_steering_max_angle;
	real ai_steering_max_throttle;
	byte unused3A8[8];
	struct tag_reference suspension_sound;
	struct tag_reference crash_sound;
	struct tag_reference material_effects;
	struct tag_reference effect;
};

typedef char vehicle_definition_size_assert[
	sizeof(struct vehicle_definition) == 0x3F0 ? 1 : -1];

#endif // __VEHICLE_DEFINITIONS_H
