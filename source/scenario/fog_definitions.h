/*
FOG_DEFINITIONS.H
*/

#ifndef __FOG_DEFINITIONS_H
#define __FOG_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	FOG_TAG = 'fog ',
};

/* ---------- macros */

#define fog_definition_get(index) ((struct fog_definition *)tag_get(FOG_TAG, (index)))

/* ---------- structures */

struct fog_definition
{
	byte flags;
	byte unused1[0x73];
	real plane_distance;
	byte unused78[0x88];
	long background_sound_index;
	struct tag_reference sound_environment;
};

typedef char fog_definition_size_assert[
	sizeof(struct fog_definition) == 0x114 ? 1 : -1];
typedef char fog_definition_plane_distance_offset_assert[
	offsetof(struct fog_definition, plane_distance) == 0x74 ? 1 : -1];
typedef char fog_definition_background_sound_offset_assert[
	offsetof(struct fog_definition, background_sound_index) == 0x100 ? 1 : -1];
typedef char fog_definition_sound_environment_offset_assert[
	offsetof(struct fog_definition, sound_environment) == 0x104 ? 1 : -1];

struct fog_screen
{
	word flags;
	short layer_count;
	real near_distance;
	real far_distance;
	real near_density;
	real far_density;
	real start_distance_from_fog_plane;
	byte reserved18[4];
	pixel32 color;
	real rotation_multiplier;
	real strafing_multiplier;
	real zoom_multiplier;
	byte reserved2C[8];
	real map_scale;
	struct tag_reference map;
	real animation_period;
	real animation_unused;
	struct real_bounds wind_velocity;
	struct real_bounds wind_period;
	real wind_acceleration_weight;
	real wind_perpendicular_weight;
};

typedef char fog_screen_size_assert[
	sizeof(struct fog_screen) == 0x68 ? 1 : -1];

#endif // __FOG_DEFINITIONS_H
