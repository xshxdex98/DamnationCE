/*
RASTERIZER_LIGHTS.H
*/

#ifndef __RASTERIZER_LIGHTS_H
#define __RASTERIZER_LIGHTS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "rasterizer/rasterizer.h"
#include "tag_files/tag_groups.h"

/* ---------- structures */

struct rasterizer_light_submit_parameters
{
	struct point_light_definition *definition;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
	real_rgb_color color;
	real radius;
};

struct rasterizer_lights
{
	long light_count;
	struct rasterizer_light_submit_parameters lights[MAXIMUM_LIGHTS_PER_WINDOW];
};

struct lens_flare_reflection
{
	word flags;
	short type;
	short bitmap_index;
	word pad06;
	byte reserved08[0x14];
	real offset;
	real rotation_offset;
	byte reserved24[0x4];
	real radius_lower_bound;
	real radius_upper_bound;
	short radius_scale_function;
	word pad32;
	real brightness_lower_bound;
	real brightness_upper_bound;
	short brightness_scale_function;
	word pad3E;
	real_argb_color tint_color;
	real_argb_color animation_color_lower_bound;
	real_argb_color animation_color_upper_bound;
	word animation_flags;
	short animation_function;
	real animation_period;
	real animation_phase;
	byte reserved7C[0x4];
};

typedef char lens_flare_reflection_size_assert[
	sizeof(struct lens_flare_reflection) == 0x80 ? 1 : -1];

struct lens_flare_definition
{
	real falloff_angle;
	real cutoff_angle;
	real runtime_cosine_falloff_angle;
	real runtime_cosine_cutoff_angle;
	real occlusion_radius;
	short occlusion_offset_direction;
	word pad16;
	real near_fade_distance;
	real far_fade_distance;
	struct tag_reference primary_map;
	word flags;
	word pad32;
	byte reserved34[0x4C];
	short corona_rotation_function;
	word pad82;
	real corona_rotation_function_scale;
	byte reserved88[0x18];
	real_vector2d corona_radius_scale;
	byte reservedA8[0x1C];
	struct tag_block reflections;
	byte reservedD0[0x20];
};

typedef char lens_flare_definition_size_assert[
	sizeof(struct lens_flare_definition) == 0xF0 ? 1 : -1];

struct rasterizer_lens_flare_submit_parameters
{
	struct lens_flare_definition *definition;
	real_point3d position;
	unsigned long compressed_direction;
	unsigned long compressed_up;
	unsigned long compressed_light_color;
	short light_identifier;
	short light_index;
	short lens_flare_index;
	byte compressed_window_index;
	byte compressed_light_scale;
	long internal_occlusion_pixels;
};

/* ---------- prototypes/RASTERIZER_LIGHTS.C */

void rasterizer_lights_begin_for_new_frame(
	void);
long rasterizer_light_submit(
	struct rasterizer_light_submit_parameters const *parameters);
void rasterizer_lens_flare_submit(
	struct rasterizer_lens_flare_submit_parameters const *parameters);
void rasterizer_lens_flare_submit_for_cluster(
	short cluster_index);
void rasterizer_lens_flares_submit_occlusion_tests(
	void);
void rasterizer_lens_flares_draw(
	void);
void rasterizer_sun_glow_draw(
	struct rasterizer_lens_flare_submit_parameters const *flare);

/* ---------- globals */

extern struct rasterizer_lights rasterizer_lights;

#endif // __RASTERIZER_LIGHTS_H
