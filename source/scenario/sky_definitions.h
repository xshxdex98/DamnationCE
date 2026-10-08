/*
SKY_DEFINITIONS.H
*/

#ifndef __SKY_DEFINITIONS_H
#define __SKY_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "tag_files/tag_files.h"

/* ---------- constants */

#define SKY_DEFINITION_TAG 'sky '

/* ---------- macros */

#define sky_definition_get(index) ((struct sky *)tag_get(SKY_DEFINITION_TAG, (index)))

/* ---------- structures */

struct sky_atmospheric_fog
{
	real_rgb_color color;
	byte unused0C[8];
	real maximum_density;
	real start_distance;
	real opaque_distance;
};

struct sky_shader_function
{
	long unused;
	char global_function_name[TAG_STRING_LENGTH+1];
};

struct sky_animation
{
	short animation_index;
	word pad2;
	real period;
	byte unused8[0x1C];
};

struct sky_light
{
	struct tag_reference lens_flare;
	char marker_name[TAG_STRING_LENGTH+1];
	byte unused30[0x38];
	real_euler_angles2d direction;
	long unused70;
};

struct sky
{
	struct tag_reference model;
	struct tag_reference animation_graph;
	long unused20[6];
	real_rgb_color indoor_ambient_radiosity_color;
	real indoor_ambient_radiosity_power;
	real_rgb_color outdoor_ambient_radiosity_color;
	real outdoor_ambient_radiosity_power;
	struct sky_atmospheric_fog outdoor_fog;
	struct sky_atmospheric_fog indoor_fog;
	struct tag_reference indoor_fog_screen;
	long unusedA8;
	struct tag_block shader_functions;
	struct tag_block animations;
	struct tag_block lights;
};

typedef char sky_atmospheric_fog_size_assert[
	sizeof(struct sky_atmospheric_fog) == 0x20 ? 1 : -1];
typedef char sky_shader_function_size_assert[
	sizeof(struct sky_shader_function) == 0x24 ? 1 : -1];
typedef char sky_animation_size_assert[
	sizeof(struct sky_animation) == 0x24 ? 1 : -1];
typedef char sky_light_size_assert[
	sizeof(struct sky_light) == 0x74 ? 1 : -1];
typedef char sky_indoor_fog_offset_assert[
	offsetof(struct sky, indoor_fog) == 0x78 ? 1 : -1];
typedef char sky_size_assert[
	sizeof(struct sky) == 0xD0 ? 1 : -1];

#endif // __SKY_DEFINITIONS_H
