/*
GLOW.H
*/

#ifndef __GLOW_H
#define __GLOW_H
#pragma once

/* ---------- headers */

#include "objects/widgets/widget_types.h"
#include "tag_files/tag_groups.h"
#include "math/real_math.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct glow_definition
{
	char attachment_marker[32];
	short number_of_particles;
	short boundary_effect;
	short particle_distribution;
	short trailing_particle_distribution;
	unsigned long flags;
	long unused02C[7];
	short render_mode;
	short render_orientation;
	long render_flags;
	short particle_rotational_velocity_attachment_index;
	short pad052;
	real particle_rotational_velocity;
	real particle_rotational_velocity_scale_lower_bound;
	real particle_rotational_velocity_scale_upper_bound;
	short effect_rotational_velocity_attachment_index;
	short pad062;
	real effect_rotational_velocity;
	real effect_rotational_velocity_scale_lower_bound;
	real effect_rotational_velocity_scale_upper_bound;
	short effect_translational_velocity_attachment_index;
	short pad072;
	real effect_translational_velocity;
	real effect_translational_velocity_scale_lower_bound;
	real effect_translational_velocity_scale_upper_bound;
	short distance_to_object_attachment_index;
	short pad082;
	real minimum_distance_to_object;
	real maximum_distance_to_object;
	real distance_to_object_scale_lower_bound;
	real distance_to_object_scale_upper_bound;
	long unused094[2];
	short particle_size_attachment_index;
	short pad09E;
	real particle_size_lower_bound;
	real particle_size_upper_bound;
	real particle_size_scale_lower_bound;
	real particle_size_scale_upper_bound;
	short color_attachment_index;
	short pad0B2;
	real_argb_color color_lower_bound;
	real_argb_color color_upper_bound;
	real_argb_color scale_color_lower_bound;
	real_argb_color scale_color_upper_bound;
	real color_rate_of_change;
	real percentage_edge_fade;
	real trailing_particle_generation_frequency;
	real trailing_particle_lifetime;
	real trailing_particle_velocity;
	real trailing_particle_minimum_t;
	real trailing_particle_maximum_t;
	long unused110[13];
	struct tag_reference texture;
};

/* ---------- prototypes/EXAMPLE.C */

void glow_initialize(
	void);
void glow_initialize_for_new_map(
	void);
void glow_dispose_from_old_map(
	void);
void glow_dispose(
	void);
long glow_new(
	long definition_index);
void glow_delete(
	long glow_index);
void glow_submit(
	long object_index,
	long glow_index,
	struct render_lighting const *lighting,
	struct render_animation const *animation);

/* ---------- globals */

/* ---------- public code */

#endif // __GLOW_H
