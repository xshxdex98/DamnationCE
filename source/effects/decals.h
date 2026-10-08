/*
DECALS.H
*/

#ifndef __DECALS_H
#define __DECALS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"

/* ---------- constants */

/* decal flags */
enum
{
	_decal_locked_bit,
	_decal_permanent_bit
};

/* decal layers */
enum
{
	_decal_layer_primary,
	_decal_layer_secondary,
	_decal_layer_light,
	_decal_layer_alpha_tested,
	_decal_layer_water,
	NUMBER_OF_DECAL_LAYERS
};

/* ---------- structures */

struct collision_result;
struct decal_editor_geometry;

struct decal_datum
{
	short identifier;
	word flags;
	short cluster_index;
	short layer;
	real_point3d position;
	long creation_time;
	byte sequence_index;
	byte unused_was_frames_remaining;
	byte sprite_index;
	byte bitmap_index;
	real lifetime;
	real decay_time;
	pixel32 color;
	byte intensity;
	byte unused;
	short quad_count;
	long definition_index;
	long previous_decal_index;
	long next_decal_index;
};

typedef char decal_datum_size_assert[
	sizeof(struct decal_datum) == 0x38 ? 1 : -1];

/* ---------- prototypes/DECALS.C */

void decals_initialize(
	void);
void decals_initialize_for_new_map(
	void);
void decals_unlock(
	boolean permanent);
void decal_delete(
	long decal_index);
long decal_get_first_decal_index(
	short cluster_index,
	short layer);
void decal_new_from_media_collision(
	long decal_definition_index,
	struct collision_result const *collision,
	real_vector3d const *velocity,
	real radius_modifier,
	boolean permanent,
	short forced_sequence_index,
	struct decal_editor_geometry *editor_geometry);
void render_debug_decals(
	void);
void decal_new_from_collision(
	long decal_definition_index,
	struct collision_result const *collision,
	real_vector3d const *velocity,
	real radius_modifier,
	boolean permanent,
	short forced_sequence_index,
	struct decal_editor_geometry *editor_geometry);
void decal_new(
	long decal_definition_index,
	real_point3d const *origin,
	real_vector3d const *velocity,
	real radius_modifier,
	boolean permanent,
	short forced_sequence_index,
	struct decal_editor_geometry *editor_geometry);
pixel32 real_a_rgb_color_to_pixel32(
	real alpha,
	real_rgb_color const *color);

/* ---------- globals */

extern boolean decals_enabled;
extern boolean debug_decals;

/* ---------- public code */

void decals_dispose(
	void);
void decals_dispose_from_old_map(
	void);
void decals_update(
	void);
void decals_delete_permanent_from_cluster(
	short cluster_index);
void decals_disconnect_from_structure_bsp(
	void);
void decals_reconnect_to_structure_bsp(
	void);

#endif // __DECALS_H
