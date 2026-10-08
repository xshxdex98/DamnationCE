/*
FLAGS.H
*/

#ifndef __FLAGS_H
#define __FLAGS_H
#pragma once

/* ---------- headers */

#include "objects/widgets/widget_types.h"
#include "tag_files/tag_groups.h"
#include "math/real_math.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct flag_definition
{
	unsigned long flags;
	short trailing_edge_shape;
	short trailing_edge_offset;
	short attached_edge_shape;
	short padA;
	short width;
	short height;
	real cell_width_scale;
	real cell_height_scale;
	struct tag_reference shader_red;
	struct tag_reference physics;
	real wind_noise;
	long unused3C[2];
	struct tag_reference shader_blue;
	struct tag_block attachment_points;
};

struct flag_attachment_point
{
	short height_to_next_attachment;
	short pad2;
	long unused[4];
	char marker_name[32];
};

/* ---------- prototypes/EXAMPLE.C */

void flags_initialize(
	void);
void flags_initialize_for_new_map(
	void);
void flags_dispose_from_old_map(
	void);
void flags_dispose(
	void);
long flag_new(
	long definition_index);
void flag_delete(
	long flag_index);
void flags_update(
	real delta);
void flag_render(
	long object_index,
	long flag_index,
	struct render_lighting const *lighting,
	struct render_animation const *animation);

/* ---------- globals */

extern struct data_array *flag_data;

/* ---------- public code */

#endif // __FLAGS_H
