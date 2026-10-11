/*
RASTERIZER_XBOX_PLASMA_ENERGY.C
*/

/* ---------- headers */

#include "cseries.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_geometry.h"
#include "real_math.h"
#include "rasterizer/rasterizer_transparent_geometry.h"
#include "rasterizer/xbox/rasterizer_xbox_plasma_energy.h"
#include "shaders/shader_definitions.h"
#include <stddef.h>

#include <xtl.h>
#include "rasterizer/xbox/rasterizer_xbox_pixel_shader.h"
#include "rasterizer/rasterizer_console_vars.h"
#include "view_fov.h" /* port: port/linux/game/view_fov.c */

/* port: the farthest the shield's flare is pushed out from the first-person
arms (display.viewmodel_shield), in world units: a body's shield stands off
much farther than an arm's radius, which so close to the camera fills the
view */
#define PLASMA_FIRST_PERSON_MAXIMUM_OFFSET 0.0075f

/* ---------- structures */

struct plasma_runtime_parameters
{
	real_rgb_color const *colors;
	real const *exponents;
};

#ifdef HALO_64BIT
/* the shared group (rasterizer_transparent_geometry.h) under the plasma
shader's names: its permutation index is the bitmap sequence index, and its
animation field carries the runtime parameters */
struct rasterizer_transparent_geometry_group_plasma
{
	unsigned int geometry_flags; /* port: (the first-person bit) */
	byte reserved04[offsetof(struct transparent_geometry_group, shader) - sizeof(unsigned int)];
	struct shader *shader;
	short bitmap_sequence_index;
	byte reserved12[offsetof(struct transparent_geometry_group, animation) -
		offsetof(struct transparent_geometry_group, shader_permutation_index) - sizeof(short)];
	struct plasma_runtime_parameters const *runtime_parameters;
};

typedef char plasma_group_flags_offset_assert[
	offsetof(struct rasterizer_transparent_geometry_group_plasma, geometry_flags) ==
		offsetof(struct transparent_geometry_group, geometry_flags) ? 1 : -1];
typedef char plasma_group_shader_offset_assert[
	offsetof(struct rasterizer_transparent_geometry_group_plasma, shader) ==
		offsetof(struct transparent_geometry_group, shader) ? 1 : -1];
typedef char plasma_group_sequence_offset_assert[
	offsetof(struct rasterizer_transparent_geometry_group_plasma, bitmap_sequence_index) ==
		offsetof(struct transparent_geometry_group, shader_permutation_index) ? 1 : -1];
typedef char plasma_group_runtime_offset_assert[
	offsetof(struct rasterizer_transparent_geometry_group_plasma, runtime_parameters) ==
		offsetof(struct transparent_geometry_group, animation) ? 1 : -1];
#else
struct rasterizer_transparent_geometry_group_plasma
{
	unsigned long geometry_flags; /* port: (the first-person bit) */
	byte reserved04[8];
	struct shader *shader;
	short bitmap_sequence_index;
	byte reserved12[0x5A];
	struct plasma_runtime_parameters const *runtime_parameters;
};
#endif

#ifndef HALO_64BIT
typedef char plasma_group_size_assert[
	sizeof(struct rasterizer_transparent_geometry_group_plasma) == 0x70 ? 1 : -1];
typedef char plasma_group_shader_offset_assert[
	offsetof(struct rasterizer_transparent_geometry_group_plasma, shader) == 0xC ? 1 : -1];
typedef char plasma_group_runtime_offset_assert[
	offsetof(struct rasterizer_transparent_geometry_group_plasma, runtime_parameters) == 0x6C ? 1 : -1];
#endif
typedef char pixel_shader_definition_size_assert[
	sizeof(struct pixel_shader_definition) == 0xF0 ? 1 : -1];

/* ---------- prototypes */

/* port: the definition's type (rasterizer_xbox.h), which WebAssembly calls
it by */
union point2d *rasterizer_set_texture(
	short stage,
	short bitmap_type,
	short bitmap_index,
	long bitmap_definition_index,
	short bitmap_sequence_index);

#ifdef HALO_64BIT
/* (as defined: an x64 Windows caller leaves the upper bits of an argument
narrower than the definition's parameter as they are) */
void rasterizer_set_vertex_shader_permutation(
	short vertex_shader_index,
	short vertex_type,
	short permutation_index);
#else
void rasterizer_set_vertex_shader_permutation(
	short vertex_type,
	short permutation,
	boolean one_node);
#endif

void rasterizer_set_pixel_shader(
	struct pixel_shader_definition const *definition);

/* port: port/linux/game/shield_color.c */
boolean shield_color_override(real_rgb_color const *colors, real_rgb_color *chosen);
void shield_color_apply(real_rgb_color const *chosen, real_rgb_color const *original, real_rgb_color *result);

/* ---------- globals */

extern void *global_d3d_device;
extern struct rasterizer_frame_begin_parameters global_frame_parameters;
extern struct pixel_shader_definition pixel_shader;

/* ---------- public code */

/* three-axis texture wrapping and additive blending */
void rasterizer_plasma_energy_draw(
	struct rasterizer_transparent_geometry_group_plasma const *group)
{
	struct shader_transparent_plasma_definition const *plasma;
	struct plasma_runtime_parameters const *runtime;
	real_rgb_color const *tint;
	real intensity;
	real offset;
	real offset_amount; /* port: PLASMA_FIRST_PERSON_MAXIMUM_OFFSET */
	real primary_time;
	real secondary_time;
	real primary_scale;
	real secondary_scale;
	real vertex_constants[6][4];
	real color_constants[3][4];
	short source;
	real_rgb_color perpendicular_color; /* port: (shield_color.c) */
	real_rgb_color parallel_color;
	real_rgb_color chosen;

	match_assert(
		"c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_plasma_energy.c",
		21,
		global_d3d_device);
	if (rasterizer_debug_options.plasma_energy_enabled)
	{
		plasma = (struct shader_transparent_plasma_definition const *)
			shader_get_and_verify_type(group->shader, _shader_type_transparent_plasma);
		tint = global_real_rgb_white;
		intensity = 1.0f;
		offset = 0.0f;
		/* port: the shield on the first-person arms kept close to them */
		offset_amount = plasma->offset_amount;
		if (viewmodel_shield_is_visible() &&
			TEST_FLAG(group->geometry_flags, _rasterizer_geometry_first_person_bit) &&
			offset_amount > PLASMA_FIRST_PERSON_MAXIMUM_OFFSET)
		{
			offset_amount = PLASMA_FIRST_PERSON_MAXIMUM_OFFSET;
		}
		runtime = group->runtime_parameters;
		if (runtime)
		{
			if (runtime->colors)
			{
				source = plasma->color_source;
				if (source >= 1 && source <= 4)
					tint = runtime->colors + source - 1;
			}
			if (runtime->exponents)
			{
				source = plasma->intensity_exponent_source;
				if (source >= 1 && source <= 4)
					intensity = (real)pow(
						(double)runtime->exponents[source - 1],
						(double)plasma->intensity_exponent);
				source = plasma->offset_exponent_source;
				if (source >= 1 && source <= 4)
					offset = (real)pow(
						(double)runtime->exponents[source - 1],
						(double)plasma->offset_exponent) * offset_amount;
			}
		}

		/* port: a local player's shield in the color chosen for it
		(port/linux/game/shield_color.c) */
		perpendicular_color = plasma->perpendicular_color;
		parallel_color = plasma->parallel_color;
		if (shield_color_override(runtime ? runtime->colors : NULL, &chosen))
		{
			shield_color_apply(&chosen, &plasma->perpendicular_color, &perpendicular_color);
			shield_color_apply(&chosen, &plasma->parallel_color, &parallel_color);
			tint = global_real_rgb_white;
		}

		rasterizer_set_texture(0, 1, 0, plasma->primary_noise_map.index, group->bitmap_sequence_index);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 0, D3DTSS_ADDRESSW, D3DTADDRESS_WRAP);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		rasterizer_set_texture(1, 1, 0, plasma->secondary_noise_map.index, group->bitmap_sequence_index);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 1, D3DTSS_ADDRESSW, D3DTADDRESS_WRAP);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice8_SetTextureStageState(global_d3d_device, 1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);

		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_CULLMODE, D3DCULL_NONE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_COLORWRITEENABLE,
			D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_DESTBLEND, D3DBLEND_ONE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_BLENDOP, D3DBLENDOP_ADD);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHATESTENABLE, FALSE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZENABLE, TRUE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZWRITEENABLE, FALSE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZBIAS, 0);

		rasterizer_set_vertex_shader_permutation(
			15,
			rasterizer_transparent_geometry_get_primary_vertex_type(
				(struct transparent_geometry_group const *)group),
			FALSE);

		match_assert(
			"c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_plasma_energy.c",
			105,
			plasma->primary_noise_map_animation_period!=0.0f);
		match_assert(
			"c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_plasma_energy.c",
			106,
			plasma->secondary_noise_map_animation_period!=0.0f);

		if (offset < 0.0005f)
			*(long *)&offset = 0;
		primary_time = global_frame_parameters.game_time_sec / plasma->primary_noise_map_animation_period;
		vertex_constants[0][1] = 0.0f;
		vertex_constants[0][2] = offset;
		vertex_constants[1][0] = 0.0f;
		vertex_constants[1][2] = 0.0f;
		vertex_constants[2][0] = 0.0f;
		vertex_constants[2][1] = 0.0f;
		vertex_constants[3][1] = 0.0f;
		vertex_constants[3][2] = 0.0f;
		vertex_constants[4][0] = 0.0f;
		vertex_constants[4][2] = 0.0f;
		vertex_constants[5][0] = 0.0f;
		vertex_constants[5][1] = 0.0f;
		color_constants[0][0] = 1.0f;
		color_constants[0][1] = 1.0f;
		color_constants[0][2] = 1.0f;
		color_constants[0][3] = 1.0f;

		secondary_time = global_frame_parameters.game_time_sec / plasma->secondary_noise_map_animation_period;
		primary_scale = plasma->primary_noise_map_scale;
		secondary_scale = plasma->secondary_noise_map_scale;
		vertex_constants[0][0] = primary_scale;
		vertex_constants[0][3] = primary_time * plasma->primary_noise_map_animation_direction.i;
		vertex_constants[1][1] = primary_scale;
		vertex_constants[1][3] = primary_time * plasma->primary_noise_map_animation_direction.j;
		vertex_constants[2][2] = primary_scale;
		vertex_constants[2][3] = primary_time * plasma->primary_noise_map_animation_direction.k;
		vertex_constants[3][0] = secondary_scale;
		vertex_constants[3][3] = secondary_time * plasma->secondary_noise_map_animation_direction.i;
		vertex_constants[4][1] = secondary_scale;
		vertex_constants[4][3] = secondary_time * plasma->secondary_noise_map_animation_direction.j;
		vertex_constants[5][2] = secondary_scale;
		vertex_constants[5][3] = secondary_time * plasma->secondary_noise_map_animation_direction.k;
		color_constants[1][0] = (perpendicular_color.red - parallel_color.red) * tint->red;
		color_constants[1][1] = (perpendicular_color.green - parallel_color.green) * tint->green;
		color_constants[1][2] = (perpendicular_color.blue - parallel_color.blue) * tint->blue;
		color_constants[1][3] = (plasma->perpendicular_alpha - plasma->parallel_alpha) * intensity;
		color_constants[2][0] = parallel_color.red * tint->red;
		color_constants[2][1] = parallel_color.green * tint->green;
		color_constants[2][2] = parallel_color.blue * tint->blue;
		color_constants[2][3] = intensity * plasma->parallel_alpha;

		IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, -81, vertex_constants, 6);
		IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, -84, color_constants, 3);

		csmemset(&pixel_shader, 0, sizeof(pixel_shader));
		pixel_shader.texture_modes = 0x42;
		pixel_shader.combiner_count = 0x104;
		pixel_shader.alpha_inputs[0] = 0x0820A920;
		pixel_shader.alpha_outputs[0] = 0xC00;
		pixel_shader.rgb_inputs[0] = 0x1920B820;
		pixel_shader.rgb_outputs[0] = 0xC00;
		pixel_shader.alpha_inputs[1] = 0x1C1C0C0C;
		pixel_shader.alpha_outputs[1] = 0x24C00;
		pixel_shader.rgb_inputs[1] = 0;
		pixel_shader.rgb_outputs[1] = 0;
		pixel_shader.alpha_inputs[2] = 0x5C5C;
		pixel_shader.alpha_outputs[2] = 0x4D00;
		pixel_shader.rgb_inputs[2] = 0;
		pixel_shader.rgb_outputs[2] = 0;
		pixel_shader.alpha_inputs[3] = 0x14150000;
		pixel_shader.alpha_outputs[3] = 0x40;
		pixel_shader.rgb_inputs[3] = 0x1C051DA0;
		pixel_shader.rgb_outputs[3] = 0xC00;
		pixel_shader.final_combiner_inputs_abcd = 0x0C0F0000;
		pixel_shader.final_combiner_inputs_efg = 0x1C1C1400;
		rasterizer_set_pixel_shader(&pixel_shader);
		rasterizer_transparent_geometry_group_draw__internal(
			(struct transparent_geometry_group const *)group,
			FALSE);
	}
	return;
}

