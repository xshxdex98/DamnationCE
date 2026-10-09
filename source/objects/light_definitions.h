/*
LIGHT_DEFINITIONS.H
*/

#ifndef __LIGHT_DEFINITIONS_H
#define __LIGHT_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	LIGHT_DEFINITION_TAG = 'ligh',
	LIGHT_DEFINITION_VERSION = 3,
};

/* ---------- structures */

struct point_light_geometry_parameters
{
	real radius;
	real radius_modifier_lower_bound;
	real radius_modifier_upper_bound;
	real falloff_angle;
	real cutoff_angle;
	real lens_flare_radius;
	real runtime_cosine_falloff_angle;
	real runtime_cosine_cutoff_angle;
	real specular_radius_multiplier;
	real runtime_sine_cutoff_angle;
	long unused[2];
};

struct point_light_gel_parameters
{
	struct tag_reference map;
	word pad0;
	short texture_animation_function;
	real texture_animation_rate;
	struct tag_reference secondary_map;
	word pad1;
	short yaw_function;
	real yaw_period;
	word pad2;
	short roll_function;
	real roll_period;
	word pad3;
	short pitch_function;
	real pitch_period;
	long unused[2];
};

struct point_light_definition
{
	unsigned long flags;
	struct point_light_geometry_parameters geometry;
	unsigned long color_interpolation_flags;
	real_argb_color color_lower_bound;
	real_argb_color color_upper_bound;
	long unused58[3];
	struct point_light_gel_parameters gel;
	struct tag_reference lens_flare;
	long unusedBC[14];
	real transition_duration;
	word padF8;
	short falloff_function;
	long unusedFC[25];
};

typedef char point_light_geometry_parameters_size_assert[
	sizeof(struct point_light_geometry_parameters) == 0x30 ? 1 : -1];
typedef char point_light_gel_parameters_size_assert[
	sizeof(struct point_light_gel_parameters) == 0x48 ? 1 : -1];
typedef char point_light_definition_lens_flare_offset_assert[
	offsetof(struct point_light_definition, lens_flare) == 0xAC ? 1 : -1];
typedef char point_light_definition_size_assert[
	sizeof(struct point_light_definition) == 0x160 ? 1 : -1];

#endif // __LIGHT_DEFINITIONS_H
