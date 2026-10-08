/*
BITMAP_GROUP.H
*/

#ifndef __BITMAP_GROUP_H
#define __BITMAP_GROUP_H
#pragma once

/* ---------- headers */

#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

/* bitmap group types */
enum
{
	_bitmap_group_type_2d_textures,
	_bitmap_group_type_3d_textures,
	_bitmap_group_type_cube_maps,
	_bitmap_group_type_sprites,
	_bitmap_group_type_interface_bitmaps,
	NUMBER_OF_BITMAP_GROUP_TYPES
};

/* bitmap flags (bitmap_data.flags) */
enum
{
	_bitmap_has_power_of_two_dimensions_bit,
	_bitmap_compressed_bit,
	_bitmap_palettized_bit,
	_bitmap_swizzled_bit,
	_bitmap_linear_bit,
	_bitmap_v16u16_bit,
	_bitmap_allocated_bit,
	_bitmap_cached_bit,
	NUMBER_OF_BITMAP_FLAGS
};

/* bitmap types (bitmap_data.type) */
enum
{
	_bitmap_type_2d,
	_bitmap_type_3d,
	_bitmap_type_cube_map,
	NUMBER_OF_BITMAP_TYPES
};

/* bitmap formats (bitmap_data.format) */
enum
{
	_bitmap_format_a8,
	_bitmap_format_y8,
	_bitmap_format_ay8,
	_bitmap_format_a8y8,
	_bitmap_format_unused1,
	_bitmap_format_unused2,
	_bitmap_format_r5g6b5,
	_bitmap_format_unused3,
	_bitmap_format_a1r5g5b5,
	_bitmap_format_a4r4g4b4,
	_bitmap_format_x8r8g8b8,
	_bitmap_format_a8r8g8b8,
	_bitmap_format_unused4,
	_bitmap_format_unused5,
	_bitmap_format_dxt1,
	_bitmap_format_dxt3,
	_bitmap_format_dxt5,
	_bitmap_format_p8_bump,
	NUMBER_OF_BITMAP_FORMATS
};

enum
{
	BITMAP_GROUP_TAG = 'bitm',
};

/* ---------- macros */

#define bitmap_group_get(index) ((struct bitmap_group *)tag_get(BITMAP_GROUP_TAG, (index)))

/* ---------- structures */

struct bitmap_data
{
	unsigned long signature;
	short width;
	short height;
	short depth;
	short type;
	short format;
	unsigned short flags;
	union point2d registration_point;
	short mipmap_count;
	short mipmap_pad;
	long pixels_offset;
	long pixels_size;
	long tag_index;
	long cache_block_index;
#ifdef HALO_64BIT
	/* tag data: Xbox addresses (a D3D texture header, the pixels) */
	XPTR(IDirect3DBaseTexture8) hardware_format;
	XPTR(void) base_address;
#else
	void *hardware_format;
	void *base_address;
#endif
};

struct bitmap_group_sprite
{
	short bitmap_index;
	short bitmap_pad;
	long unused;
	real_rectangle2d bounds;
	real_point2d registration_point;
};

struct bitmap_group_sequence
{
	char name[32];
	short first_bitmap_index;
	short bitmap_count;
	long unused[4];
	struct tag_block sprites;
};

typedef char bitmap_group_sprite_size_assert[
	sizeof(struct bitmap_group_sprite) == 0x20 ? 1 : -1];
typedef char bitmap_group_sequence_size_assert[
	sizeof(struct bitmap_group_sequence) == 0x40 ? 1 : -1];

struct bitmap_group
{
	short type;
	short format;
	short usage;
	unsigned short flags;
	real detail_fade;
	real sharpen_amount;
	real bump_height;
	short sprite_budget_size;
	unsigned short sprite_budget_count;
	short import_width;
	short import_height;
	struct tag_data import_bitmap;
	struct tag_data pixel_data;
	real smoothing_filter_size;
	real alpha_bias;
	short mipmap_count;
	short sprite_usage;
	short sprite_spacing;
	unsigned short unused;
	struct tag_block sequences;
	struct tag_block bitmaps;
};

/* ---------- prototypes/BITMAP_GROUP.C */

struct bitmap_data *bitmap_group_get_bitmap_from_sequence(
	long bitmap_group_index,
	short sequence_index,
	short frame_index);

/* ---------- globals */

/* ---------- public code */

#endif // __BITMAP_GROUP_H
