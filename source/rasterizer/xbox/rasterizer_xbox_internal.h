/*
RASTERIZER_XBOX_INTERNAL.H

Declarations shared by the Xbox rasterizer's files only.
*/

#ifndef __RASTERIZER_XBOX_INTERNAL_H
#define __RASTERIZER_XBOX_INTERNAL_H
#pragma once

#include "cseries.h"
#include "math/integer_math.h"
#include "shaders/shader_texture_animation.h"
#include "tag_files/tag_groups.h"

struct shader_environment_specular_properties
{
	word flags;
	short type;
	long unused04[4];
	real brightness;
	long unused18[5];
	real_rgb_color view_perpendicular_color;
	real_rgb_color view_parallel_color;
	long unused44[4];
};


struct shader_model_properties
{
	word flags;
	short type;
	byte reserved04[0xC];
	real translucency;
	byte reserved14[0x10];
	short change_color_source;
	byte reserved26[0x1E];
	word self_illumination_flags;
	short pad46;
	short self_illumination_color_source;
	short self_illumination_animation_function;
	real self_illumination_animation_period;
	real_rgb_color self_illumination_animation_color_lower_bound;
	real_rgb_color self_illumination_animation_color_upper_bound;
	byte reserved68[0xC];
	real map_u_scale;
	real map_v_scale;
	struct tag_reference base_map;
	byte reserved8C[8];
	struct tag_reference multipurpose_map;
	byte reservedA4[8];
	short detail_function;
	short detail_mask;
	real detail_map_scale;
	struct tag_reference detail_map;
	real detail_map_v_scale;
	byte reservedC8[0xC];
	struct shader_texture_animation texture_animation;
	byte reserved10C[8];
	real reflection_falloff_distance;
	real reflection_cutoff_distance;
	real perpendicular_brightness;
	real_rgb_color perpendicular_tint_color;
	real parallel_brightness;
	real_rgb_color parallel_tint_color;
	struct tag_reference reflection_cube_map;
};


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


struct bitmap_data;
struct rasterizer_model_begin_parameters;
struct shader;
struct triangle_buffer;
struct vertex_buffer;
struct render_distant_light;
struct render_fog;
struct render_lighting;
struct rasterizer_frame_begin_parameters;
struct rasterizer_model_lighting_constants;
struct rasterizer_window_begin_parameters;

boolean _rasterizer_initialize(
	void);
void _rasterizer_reset_state(
	void);
void _rasterizer_frame_begin(
	struct rasterizer_frame_begin_parameters const *parameters);
void _rasterizer_frame_end(
	void);
void _rasterizer_present(
	struct bitmap_data *screenshot_bitmap,
	point2d const *screenshot_index);
void _rasterizer_windows_begin(
	void);
void _rasterizer_window_begin(
	struct rasterizer_window_begin_parameters const *parameters);
void _rasterizer_window_get_fog(
	struct render_fog *fog);
void _rasterizer_window_set_fog(
	struct render_fog const *fog);
void _rasterizer_window_end(
	void);
void _rasterizer_windows_end(
	void);
void _rasterizer_dispose(
	void);
void _rasterizer_set_vblank_callback(
	void (*callback)(unsigned long));

short rasterizer_get_stencil_mode(
	void);
void rasterizer_set_model_lighting(
	struct render_lighting const *lighting);
void rasterizer_set_model_lighting_point_light(
	long light_index,
	short constant_index,
	struct rasterizer_model_lighting_constants *lighting_constants);
void rasterizer_set_model_lighting_distant_light(
	struct render_distant_light const *light,
	short light_index,
	struct rasterizer_model_lighting_constants *lighting_constants);
void rasterizer_set_frustum_z(
	real z_near,
	real z_far);
void rasterizer_set_vertex_shader(
	short vertex_shader_index);
void *rasterizer_get_bitmap_default_hardware_format(
	struct bitmap_data const *bitmap);
void rasterizer_filthy_bitmap_default_initialize(
	void);
void rasterizer_secondary_render_target_debug(
	rectangle2d *bounds);
void rasterizer_water_set_visibility_for_frame(
	boolean visibility);
void rasterizer_decal_vertices_begin_update(
	void);
void rasterizer_decal_vertices_end_update(
	void);
boolean rasterizer_detail_objects_initialize(
	void);
void rasterizer_detail_objects_dispose(
	void);
boolean rasterizer_environment_fog_screen_initialize(
	void);
void rasterizer_environment_fog_screen_window_begin(
	void);
void rasterizer_environment_fog_screen_window_end(
	void);
void _rasterizer_environment_fog_screen_draw(
	struct shader const *shader,
	short bitmap_index,
	long dynamic_triangle_buffer_index,
	long first_triangle_index,
	long triangle_count,
	struct vertex_buffer const *vertex_buffer);
void _rasterizer_environment_fog_screen_end(
	void);
boolean rasterizer_environment_fog_screen_model_begin(
	struct rasterizer_model_begin_parameters const *parameters);
void rasterizer_environment_fog_screen_model_end(
	void);
void rasterizer_environment_fog_screen_model_submit(
	struct shader *shader,
	short shader_permutation_index,
	struct triangle_buffer const *triangle_buffer,
	long dynamic_triangle_buffer_index,
	long triangle_count,
	struct vertex_buffer const *vertex_buffer,
	long dynamic_vertex_buffer_index);
void rasterizer_environment_fog_screen_dispose(
	void);

void _rasterizer_environment_reflection_mirrors_begin(
	void);
void _rasterizer_environment_reflection_mirror_draw(
	struct shader const *shader,
	short bitmap_index,
	long dynamic_triangle_buffer_index,
	long first_triangle_index,
	long triangle_count,
	struct vertex_buffer const *vertex_buffer);
void _rasterizer_environment_reflection_mirrors_end(
	void);
void _rasterizer_environment_reflections_begin(
	void);
void _rasterizer_environment_reflection_draw(
	struct shader const *shader,
	short bitmap_index,
	long dynamic_triangle_buffer_index,
	long first_triangle_index,
	long triangle_count,
	struct vertex_buffer const *vertex_buffer);
void _rasterizer_environment_reflections_end(
	void);

#endif /* __RASTERIZER_XBOX_INTERNAL_H */
