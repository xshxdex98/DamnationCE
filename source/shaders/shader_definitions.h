/*
SHADER_DEFINITIONS.H
*/

#ifndef __SHADER_DEFINITIONS_H
#define __SHADER_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "real_math.h"
#include "shaders/shader_texture_animation.h"
#include "tag_groups.h"

/* ---------- constants */

/* shader types */
enum
{
	_shader_type_screen,
	_shader_type_effect,
	_shader_type_decal,
	_shader_type_environment,
	_shader_type_model,
	_shader_type_transparent_generic,
	_shader_type_transparent_chicago,
	_shader_type_transparent_water,
	_shader_type_transparent_glass,
	_shader_type_transparent_meter,
	_shader_type_transparent_plasma,
	NUMBER_OF_SHADER_TYPES
};

/* an environment shader's flags */
enum
{
	_shader_environment_alpha_tested_bit = 0,
	_shader_environment_bump_map_is_specular_mask_bit,
	_shader_environment_true_atmospheric_fog_bit,
};

/* its diffuse flags */
enum
{
	_shader_environment_diffuse_rescale_detail_maps_bit = 0,
	_shader_environment_diffuse_rescale_bump_map_bit,
};

/* its self-illumination flags */
enum
{
	_shader_environment_self_illumination_unfiltered_bit = 0,
};

/* its specular flags */
enum
{
	_shader_environment_specular_overbright_bit = 0,
	_shader_environment_specular_extra_shiny_bit,
	_shader_environment_specular_lightmap_bit,
};

/* its reflection flags */
enum
{
	_shader_environment_reflection_dynamic_mirror_bit = 0,
};

/* a model shader's flags */
enum
{
	_shader_model_detail_after_reflection_bit = 0,
	_shader_model_two_sided_bit,
	_shader_model_not_alpha_tested_bit,
	_shader_model_alpha_blended_decal_bit,
	_shader_model_true_atmospheric_fog_bit,
	_shader_model_nocull_two_sided_bit,
	NUMBER_OF_SHADER_MODEL_FLAGS
};

/* its self-illumination flags */
enum
{
	_shader_model_self_illumination_no_random_phase_bit = 0,
};

/* a shader animation's functions and its sources: none, or one of an
object's four outgoing functions (render_animation's values[source - 1]) */
enum
{
	NUMBER_OF_SHADER_ANIMATION_FUNCTIONS = 4,
	NUMBER_OF_SHADER_ANIMATION_SOURCES = 5
};

/* framebuffer blend functions */
enum
{
	_shader_framebuffer_blend_function_alpha_blend = 0,
	_shader_framebuffer_blend_function_multiply,
	_shader_framebuffer_blend_function_double_multiply,
	_shader_framebuffer_blend_function_add,
	_shader_framebuffer_blend_function_subtract,
	_shader_framebuffer_blend_function_component_min,
	_shader_framebuffer_blend_function_component_max,
	_shader_framebuffer_blend_function_alpha_multiply_add,
	NUMBER_OF_SHADER_FRAMEBUFFER_BLEND_FUNCTIONS
};

/* ---------- macros */

#define SHADER_DEFINITION_TAG 'shdr'
#define shader_definition_get(index) \
	((struct shader *)tag_get(SHADER_DEFINITION_TAG, (index)))

/* ---------- structures */

struct shader_radiosity_properties
{
	unsigned short flags;
	short detail_level;
	real power;
	real_rgb_color color_of_emitted_light;
	real_rgb_color tint_color;
};

struct shader_physics_properties
{
	unsigned short flags;
	short material_type;
};

struct shader_base
{
	struct shader_radiosity_properties radiosity;
	struct shader_physics_properties physics;
	short type;
	short pad;
};

struct shader
{
	struct shader_base base;
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
	long unused14C[17];
};

typedef char shader_model_properties_size_assert[
	sizeof(struct shader_model_properties) == 0x190 ? 1 : -1];

struct shader_effect_definition
{
	struct shader shader;
	unsigned short flags;
	short framebuffer_blend_function;
	short framebuffer_fade_mode;
	unsigned short primary_map_flags;
	byte reserved_before_secondary_map[28];
	struct tag_reference secondary_map;
	short secondary_map_anchor;
	unsigned short secondary_map_flags;
	struct shader_texture_animation secondary_map_animation;
	real secondary_map_radius;
	real secondary_map_zsprite_radius_scale;
	byte reserved_after_secondary_map_zsprite_radius_scale[20];
};

typedef char shader_effect_secondary_map_offset_assert[
	offsetof(struct shader_effect_definition, secondary_map) == 0x4C ? 1 : -1];
typedef char shader_effect_secondary_map_anchor_offset_assert[
	offsetof(struct shader_effect_definition, secondary_map_anchor) == 0x5C ? 1 : -1];
typedef char shader_effect_secondary_map_flags_offset_assert[
	offsetof(struct shader_effect_definition, secondary_map_flags) == 0x5E ? 1 : -1];
typedef char shader_effect_secondary_map_animation_offset_assert[
	offsetof(struct shader_effect_definition, secondary_map_animation) == 0x60 ? 1 : -1];
typedef char shader_effect_secondary_map_radius_offset_assert[
	offsetof(struct shader_effect_definition, secondary_map_radius) == 0x98 ? 1 : -1];
typedef char shader_effect_secondary_map_zsprite_radius_scale_offset_assert[
	offsetof(struct shader_effect_definition, secondary_map_zsprite_radius_scale) == 0x9C ? 1 : -1];
typedef char shader_effect_framebuffer_fade_mode_offset_assert[
	offsetof(struct shader_effect_definition, framebuffer_fade_mode) == 0x2C ? 1 : -1];
typedef char shader_effect_definition_size_assert[
	sizeof(struct shader_effect_definition) == 0xB4 ? 1 : -1];

struct water_ripple
{
	byte reserved00[4];
	real contribution_factor;
	byte reserved08[0x20];
	real animation_angle;
	real animation_velocity;
	real_point2d map_offset;
	short map_repeats;
	short map_index;
	byte reserved3C[0x10];
};

typedef char water_ripple_size_assert[
	sizeof(struct water_ripple) == 0x4C ? 1 : -1];

struct shader_transparent_glass_definition
{
	struct shader shader;
	word flags;
	short pad2A;
	byte reserved_before_tint_color[40];
	real_rgb_color tint_color;
	real tint_map_scale;
	struct tag_reference tint_map;
	byte reserved_before_reflection_flags[20];
	word reflection_flags;
	short reflection_type;
	real_argb_color reflection_view_perpendicular_color;
	real_argb_color reflection_view_parallel_color;
	struct tag_reference reflection_map;
	real reflection_bump_map_scale;
	struct tag_reference reflection_bump_map;
	byte reserved_before_diffuse_flags[128];
	word diffuse_flags;
	word pad152;
	real diffuse_map_scale;
	struct tag_reference diffuse_map;
	real diffuse_detail_map_scale;
	struct tag_reference diffuse_detail_map;
	byte reserved_after_diffuse_detail_map[100];
};

typedef char shader_transparent_glass_definition_size_assert[
	sizeof(struct shader_transparent_glass_definition) == 0x1E0 ? 1 : -1];

struct shader_transparent_meter_definition
{
	struct shader shader;
	word flags;
	short pad2A;
	byte reserved_before_map[32];
	struct tag_reference map;
	byte reserved_before_gradient_min_color[32];
	real_rgb_color gradient_min_color;
	real_rgb_color gradient_max_color;
	real_rgb_color background_color;
	real_rgb_color flash_color;
	real_rgb_color tint_color;
	real meter_transparency;
	real background_transparency;
	byte reserved_before_meter_brightness_source[24];
	short meter_brightness_source;
	short flash_brightness_source;
	short value_source;
	short gradient_source;
	short flash_extension_source;
	word padE2;
	byte reserved_after_flash_extension_source[32];
};

typedef char shader_transparent_meter_definition_size_assert[
	sizeof(struct shader_transparent_meter_definition) == 0x104 ? 1 : -1];

struct shader_transparent_generic_map
{
	word flags;
	word pad02;
	real map_u_scale;
	real map_v_scale;
	real map_u_offset;
	real map_v_offset;
	real map_rotation;
	real mipmap_bias;
	struct tag_reference map;
	struct shader_texture_animation map_animation;
};

typedef char shader_transparent_generic_map_size_assert[
	sizeof(struct shader_transparent_generic_map) == 0x64 ? 1 : -1];

struct shader_transparent_chicago_map
{
	word flags;
	byte reserved_before_functions[42];
	short color_function;
	short alpha_function;
	byte reserved_before_map_u_scale[36];
	real map_u_scale;
	real map_v_scale;
	real map_u_offset;
	real map_v_offset;
	real map_rotation;
	real mipmap_bias;
	struct tag_reference map;
	byte reserved_after_map[40];
	struct shader_texture_animation map_animation;
};

typedef char shader_transparent_chicago_map_size_assert[
	sizeof(struct shader_transparent_chicago_map) == 0xDC ? 1 : -1];

struct shader_transparent_generic_stage
{
	word flags;
	short pad02;
	short constant_color0_animation_source;
	short constant_color0_animation_function;
	real constant_color0_animation_period;
	real_argb_color constant_color0_lower_bound;
	real_argb_color constant_color0_upper_bound;
	real_argb_color color1;
	short color_input_A;
	short color_input_A_mapping;
	short color_input_B;
	short color_input_B_mapping;
	short color_input_C;
	short color_input_C_mapping;
	short color_input_D;
	short color_input_D_mapping;
	short color_output_AB;
	short color_output_AB_function;
	short color_output_CD;
	short color_output_CD_function;
	short color_output_AB_CD_mux_sum;
	short color_output_mapping;
	short alpha_input_A;
	short alpha_input_A_mapping;
	short alpha_input_B;
	short alpha_input_B_mapping;
	short alpha_input_C;
	short alpha_input_C_mapping;
	short alpha_input_D;
	short alpha_input_D_mapping;
	short alpha_output_A;
	short alpha_output_B;
	short alpha_output_C;
	short alpha_output_mapping;
};

typedef char shader_transparent_generic_stage_size_assert[
	sizeof(struct shader_transparent_generic_stage) == 0x70 ? 1 : -1];

struct shader_model_definition
{
	struct shader shader;
	struct shader_model_properties model;
};

typedef char shader_model_definition_size_assert[
	sizeof(struct shader_model_definition) == 0x1B8 ? 1 : -1];

struct shader_transparent
{
	byte numeric_counter_limit;
	byte flags;
	short first_map_type;
	short framebuffer_blend_function;
	short framebuffer_fade_mode;
	short framebuffer_fade_source;
	short pad32;
	real lens_flare_spacing;
	struct tag_reference lens_flare;
	struct tag_block extra_layers;
	struct tag_block maps;
};

typedef char shader_transparent_size_assert[
	sizeof(struct shader_transparent) == 0x38 ? 1 : -1];

struct shader_transparent_generic_definition
{
	struct shader shader;
	struct shader_transparent transparent;
	struct tag_block stages;
};

typedef char shader_transparent_generic_definition_size_assert[
	sizeof(struct shader_transparent_generic_definition) == 0x6C ? 1 : -1];

struct shader_transparent_chicago_definition
{
	struct shader shader;
	struct shader_transparent transparent;
	unsigned long extra_flags;
	byte reserved64[8];
};

typedef char shader_transparent_chicago_definition_size_assert[
	sizeof(struct shader_transparent_chicago_definition) == 0x6C ? 1 : -1];

struct shader_environment_self_illumination_properties
{
	word flags;
	short pad02;
	byte reserved04[0x18];
	real_rgb_color primary_on_color;
	real_rgb_color primary_off_color;
	short primary_animation_function;
	short pad36;
	real primary_animation_period;
	real primary_animation_phase;
	byte reserved40[0x18];
	real_rgb_color secondary_on_color;
	real_rgb_color secondary_off_color;
	short secondary_animation_function;
	short pad72;
	real secondary_animation_period;
	real secondary_animation_phase;
	byte reserved7C[0x18];
	real_rgb_color plasma_on_color;
	real_rgb_color plasma_off_color;
	short plasma_animation_function;
	short padAE;
	real plasma_animation_period;
	real plasma_animation_phase;
	byte reservedB8[0x18];
	real map_scale;
	struct tag_reference map;
};

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

struct shader_environment_diffuse_properties
{
	word flags;
	short pad02;
	byte reserved04[0x18];
	struct tag_reference base_map;
	byte reserved2C[0x18];
	short detail_map_function;
	short pad46;
	real primary_detail_map_scale;
	struct tag_reference primary_detail_map;
	real secondary_detail_map_scale;
	struct tag_reference secondary_detail_map;
	byte reserved70[0x18];
	short micro_detail_map_function;
	short pad8A;
	real micro_detail_map_scale;
	struct tag_reference micro_detail_map;
	real_rgb_color material_color;
	byte reservedAC[0xC];
	real bump_map_scale;
	struct tag_reference bump_map;
	real_vector2d runtime_bump_map_scale;
	byte reservedD4[0x10];
	short u_animation_function;
	short pad_u_animation;
	real u_animation_period;
	real u_animation_scale;
	short v_animation_function;
	short pad_v_animation;
	real v_animation_period;
	real v_animation_scale;
	byte reservedFC[0x18];
};

typedef char shader_environment_diffuse_properties_size_assert[
	sizeof(struct shader_environment_diffuse_properties) == 0x114 ? 1 : -1];

struct shader_environment_reflection_properties
{
	word flags;
	short type;
	real lightmap_brightness_scale;
	long unused1[7];
	real view_perpendicular_brightness;
	real view_parallel_brightness;
	long unused2[4];
	real mirror_index_of_refraction;
	real mirror_depth;
	long unused3[4];
	struct tag_reference cube_map;
	long unused4[4];
};

typedef char shader_environment_reflection_properties_size_assert[
	sizeof(struct shader_environment_reflection_properties) == 0x74 ? 1 : -1];

struct shader_environment_properties
{
	word flags;
	short type;
	real lens_flare_spacing;
	struct tag_reference lens_flare;
	long unused[11];
	struct shader_environment_diffuse_properties diffuse;
	struct shader_environment_self_illumination_properties self_illumination;
	byte reserved23C[0x18];
	struct shader_environment_specular_properties specular;
	struct shader_environment_reflection_properties reflection;
};

struct shader_environment_definition
{
	struct shader shader;
	struct shader_environment_properties environment;
};

typedef char shader_environment_definition_size_assert[
	sizeof(struct shader_environment_definition) == 0x344 ? 1 : -1];

struct shader_transparent_water_definition
{
	struct shader shader;
	unsigned short flags;
	short type;
	byte reserved2C[0x20];
	struct tag_reference base_map;
	byte reserved5C[0x10];
	real_argb_color view_perpendicular_tint_color;
	real_argb_color view_parallel_tint_color;
	byte reserved8C[0x10];
	struct tag_reference reflection_map;
	byte reservedAC[0x10];
	real ripple_animation_angle;
	real ripple_animation_velocity;
	real ripple_scale;
	struct tag_reference ripple_maps;
	short ripple_mipmap_levels;
	short pad0DA;
	real ripple_mipmap_fade_factor;
	real ripple_mipmap_lod_bias;
	byte reserved0E4[0x40];
	struct tag_block ripples;
	byte reserved130[0x10];
};

typedef char shader_transparent_water_definition_size_assert[
	sizeof(struct shader_transparent_water_definition) == 0x140 ? 1 : -1];

struct shader_transparent_plasma_definition
{
	struct shader shader;
	byte reserved28[4];
	short intensity_exponent_source;
	short pad2E;
	real intensity_exponent;
	short offset_exponent_source;
	short pad36;
	real offset_amount;
	real offset_exponent;
	byte reserved40[0x20];
	real perpendicular_alpha;
	real_rgb_color perpendicular_color;
	real parallel_alpha;
	real_rgb_color parallel_color;
	short color_source;
	byte reserved82[0x3E];
	real primary_noise_map_animation_period;
	real_vector3d primary_noise_map_animation_direction;
	real primary_noise_map_scale;
	struct tag_reference primary_noise_map;
	byte reservedE4[0x24];
	real secondary_noise_map_animation_period;
	real_vector3d secondary_noise_map_animation_direction;
	real secondary_noise_map_scale;
	struct tag_reference secondary_noise_map;
	byte reserved12C[0x20];
};

typedef char shader_transparent_plasma_definition_size_assert[
	sizeof(struct shader_transparent_plasma_definition) == 0x14C ? 1 : -1];

/* ---------- prototypes/SHADER_DEFINITIONS.C */

struct shader *shader_get_and_verify_type(struct shader *shader, short shader_type);

/* ---------- globals */

extern struct shader_effect_definition global_shader_effect_additive;
extern struct shader_effect_definition global_shader_effect_alpha_blended;

#endif // __SHADER_DEFINITIONS_H
