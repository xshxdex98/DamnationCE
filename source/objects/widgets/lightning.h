/*
LIGHTNING.H
*/

#ifndef __LIGHTNING_H
#define __LIGHTNING_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "memory/data.h"
#include "objects/widgets/widget_types.h"
#include "tag_files/tag_groups.h"
#include "math/real_math.h"

/* ---------- macros */

#define lightning_get(lightning_index) \
	((struct lightning_datum *)datum_get(lightning_globals.lightning_data, (lightning_index)))

/* ---------- structures */

struct lightning_definition
{
	word flags;
	short count;
	byte reserved04[0x10];
	real near_fade_distance;
	real far_fade_distance;
	byte reserved1C[0x10];
	short jitter_scale_source;
	short thickness_scale_source;
	short tint_modulation_source;
	short brightness_scale_source;
	struct tag_reference map;
	byte reserved44[0x54];
	struct tag_block markers;
	struct tag_block shaders;
	byte reservedB0[0x58];
};

struct lightning_marker_definition
{
	char attachment_marker[32];
	word flags;
	short type;
	short octaves_to_next_marker;
	word pad26;
	byte reserved28[0x4C];
	real_vector3d random_position_bounds;
	real random_jitter_offset;
	real thickness;
	real_argb_color tint;
	byte reserved98[0x4C];
};

struct lightning_globals
{
	struct data_array *lightning_data;
};

struct lightning_datum
{
	struct datum_header header;
	short __unknown2;
	long definition_index;
};

#ifndef HALO_64BIT
typedef char lightning_globals_size_assert[
	sizeof(struct lightning_globals) == 0x4 ? 1 : -1];
#endif
typedef char lightning_datum_size_assert[
	sizeof(struct lightning_datum) == 0x8 ? 1 : -1];

/* ---------- prototypes/LIGHTNING.C */

void lightnings_initialize(
	void);

void lightnings_dispose(
	void);
void lightnings_initialize_for_new_map(
	void);
void lightnings_dispose_from_old_map(
	void);

long lightning_new(
	long definition_index);

void lightning_delete(
	long lightning_index);

void lightning_submit(
	long object_index,
	long widget_index,
	struct render_lighting const *lighting,
	struct render_animation const *animation);

void lightning_render(
	void);

#endif // __LIGHTNING_H
