/*
RASTERIZER_TRANSPARENT_GEOMETRY.H

Narrow cross-translation-unit interface owned by RASTERIZER_TRANSPARENT_GEOMETRY.C.
*/

#ifndef __RASTERIZER_TRANSPARENT_GEOMETRY_H
#define __RASTERIZER_TRANSPARENT_GEOMETRY_H
#pragma once

#include "cseries.h"
#include "rasterizer/rasterizer_model_types.h"

struct triangle_buffer;
struct vertex_buffer;
struct bitmap_data;
struct render_lighting;
struct render_animation;

struct transparent_geometry_group
{
	unsigned int geometry_flags;
	int object_index;
	int source_object_index;
	struct shader *shader;
	short shader_permutation_index;
	word pad12;
	struct render_model_effect effect;
	real_vector2d model_base_map_scale;
	int dynamic_triangle_buffer_index;
	/* a widget group (one with no shader) holds its render proc here and the
	proc's two arguments in the next two fields */
	union
	{
		struct triangle_buffer const *triangle_buffer;
		void (*render_proc)(int object_index, int widget_index);
	};
	int first_triangle_index;
	int triangle_count;
	int dynamic_vertex_buffer_index;
	struct vertex_buffer const *vertex_buffer;
	struct bitmap_data const *lightmap;
	real_matrix4x3 const *node_matrices;
	short node_matrix_count;
	word pad66;
	struct render_lighting const *lighting;
	struct render_animation const *animation;
	real z_sort;
	real_point3d centroid;
	real_plane3d plane;
	int sorted_index;
	short previous_group_presorted_index;
	short next_group_presorted_index;
	int active_camouflage_transparent_source_object_index;
	boolean sort_last;
	boolean cortana_hack;
	byte pad9E[2];
};

/* port: static enclosure recognition, used when model tags load. */
struct shader;
struct vertex_buffer;
struct triangle_buffer;
boolean rasterizer_transparent_geometry_is_enclosure(
	struct shader const *glass, struct vertex_buffer const *outer,
	struct triangle_buffer const *triangles,
	struct shader const *energy, struct vertex_buffer const *inner);

void rasterizer_transparent_geometry_groups_begin(
	void);
void rasterizer_transparent_geometry_groups_end(
	void);
void rasterizer_transparent_geometry_group_draw(
	struct transparent_geometry_group *group,
	boolean dirty);
void rasterizer_transparent_geometry_group_draw__internal(
	struct transparent_geometry_group const *group,
	boolean has_lightmap);

/* port: refine centroid sorting using planar BSP glass and model bounds. */
void rasterizer_transparent_geometry_order_models(short *order, long count);

#endif /* __RASTERIZER_TRANSPARENT_GEOMETRY_H */
