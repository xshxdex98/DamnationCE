/*
WEATHER_PARTICLE_DEFINITIONS.H
*/

#ifndef __WEATHER_PARTICLE_DEFINITIONS_H
#define __WEATHER_PARTICLE_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "shaders/shader_definitions.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	WEATHER_PARTICLE_SYSTEM_DEFINITION_TAG = 'rain',
};

/* ---------- macros */

#define weather_particle_system_definition_get(index) ((struct weather_particle_system_definition *)tag_get(WEATHER_PARTICLE_SYSTEM_DEFINITION_TAG, (index)))

/* ---------- structures */

struct weather_particle_system_definition
{
	unsigned long flags;
	long unused[8];
	struct tag_block particle_types;
};

typedef char weather_particle_system_definition_size_assert[
	sizeof(struct weather_particle_system_definition) == 0x30 ? 1 : -1];

struct weather_particle_type_definition
{
	char name[32];
	unsigned long flags;
	real distance_fadein_start;
	real distance_fadein_end;
	real distance_fadeout_start;
	real distance_fadeout_end;
	real height_fadein_start;
	real height_fadein_end;
	real height_fadeout_start;
	real height_fadeout_end;
	long unused[24];
	real particle_count_lower_bound;
	real particle_count_upper_bound;
	struct tag_reference physics;
	long unused2[4];
	real acceleration_lower_bound;
	real acceleration_upper_bound;
	real acceleration_turning_rate;
	real acceleration_change_rate;
	long unused3[8];
	real radius_lower_bound;
	real radius_upper_bound;
	real animation_rate_lower_bound;
	real animation_rate_upper_bound;
	real rotation_rate_lower_bound;
	real rotation_rate_upper_bound;
	long unused4[8];
	real_argb_color color_lower_bound;
	real_argb_color color_upper_bound;
	real runtime_one_over_sprite_width;
	long unused5[15];
	struct tag_reference bitmap;
	short render_mode;
	short render_direction_source;
	struct shader_effect_definition shader;
};

#endif // __WEATHER_PARTICLE_DEFINITIONS_H
