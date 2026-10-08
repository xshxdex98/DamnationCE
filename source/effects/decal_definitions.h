/*
DECAL_DEFINITIONS.H
*/

#ifndef __DECAL_DEFINITIONS_H
#define __DECAL_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "shaders/shader_definitions.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	DECAL_GROUP_TAG = 'deca',
};

enum decal_definition_flag
{
	_decal_definition_geometry_inherited_by_next_decal_in_chain_bit,
	_decal_definition_color_interpolate_in_hsv_bit,
	_decal_definition_color_interpolate_along_farthest_hue_path_bit,
	_decal_definition_no_random_rotation_bit,
	_decal_definition_water_effect_bit,
	_decal_definition_SAPIEN_ONLY_snap_to_axis_bit,
	_decal_definition_SAPIEN_ONLY_incremental_counter_bit,
	_decal_definition_animation_loop_bit,
	_decal_definition_preserve_aspect_bit,
	NUMBER_OF_DECAL_DEFINITION_FLAGS
};

enum decal_type
{
	_decal_type_scratch,
	_decal_type_splatter,
	_decal_type_burn,
	_decal_type_painted_sign,
	NUMBER_OF_DECAL_TYPES
};

/* ---------- macros */

#define decal_definition_get(index) ((struct decal_definition *)tag_get(DECAL_GROUP_TAG, (index)))

/* ---------- structures */

struct decal_shader_definition
{
	struct shader shader;
	word flags;
	short type;
	short framebuffer_blend_function;
	word pad02E;
	long unused030[5];
	struct tag_reference map;
	long unused054[5];
};

struct decal_definition
{
	word flags;                                 /* 0x000 */
	short type;                                 /* 0x002 */
	short layer;                                /* 0x004 */
	word pad006;
	struct tag_reference next_decal_in_chain;   /* 0x008 */
	real radius_lower_bound;                    /* 0x018 */
	real radius_upper_bound;                    /* 0x01C */
	long unused020[3];
	real intensity_lower_bound;                 /* 0x02C */
	real intensity_upper_bound;                 /* 0x030 */
	real_rgb_color color_lower_bound;           /* 0x034 */
	real_rgb_color color_upper_bound;           /* 0x040 */
	long unused04C[3];
	short animation_loop_frame_index;
	short animation_speed;
	long unused05C[7];
	real lifetime_lower_bound;                  /* 0x078 */
	real lifetime_upper_bound;                  /* 0x07C */
	real decay_time_lower_bound;                /* 0x080 */
	real decay_time_upper_bound;                /* 0x084 */
	long unused088[3];
	struct decal_shader_definition shader;      /* 0x094 */
	real runtime_maximum_sprite_extent;         /* 0x0FC */
	word runtime_incremental_counter;
	word pad102;
	long unused104[2];
};

typedef char decal_definition_size_assert[
	sizeof(struct decal_definition) == 0x10C ? 1 : -1];
typedef char decal_definition_blend_function_offset_assert[
	offsetof(struct decal_definition, shader.framebuffer_blend_function) == 0xC0 ? 1 : -1];

#endif // __DECAL_DEFINITIONS_H
