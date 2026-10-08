/*
DETAIL_OBJECT_DEFINITIONS.H
*/

#ifndef __DETAIL_OBJECT_DEFINITIONS_H
#define __DETAIL_OBJECT_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- structures */

struct detail_object
{
	byte position[3];
	byte data;
	word color;
};

typedef char detail_object_size_assert[
	sizeof(struct detail_object) == 0x6 ? 1 : -1];

struct detail_object_cell_definition
{
	short cell_x;
	short cell_y;
	short cell_z;
	short offset_z;
	unsigned long valid_layers;
	long start_index;
	long count_index;
	long unused14[3];
};

typedef char detail_object_cell_definition_size_assert[
	sizeof(struct detail_object_cell_definition) == 0x20 ? 1 : -1];

struct structure_detail_object_data
{
	struct tag_block cells;
	struct tag_block detail_objects;
	struct tag_block counts;
	struct tag_block z_reference_vectors;
	byte valid;
	byte pad31[3];
	long unused34[3];
};

typedef char structure_detail_object_data_size_assert[
	sizeof(struct structure_detail_object_data) == 0x40 ? 1 : -1];

struct detail_object_type_definition
{
	char name[32];
	byte sequence_index;
	byte flags;
	byte first_sprite_index;
	byte sprite_count;
	real color_override_factor;
	long unused28[2];
	real near_fade_distance;
	real far_fade_distance;
	real size_min;
	real size_max;
	byte reserved40[0x20];
};

typedef char detail_object_type_definition_size_assert[
	sizeof(struct detail_object_type_definition) == 0x60 ? 1 : -1];

struct detail_object_collection_definition
{
	short collection_type;
	word pad02;
	real global_z_offset;
	long unused08[11];
	struct tag_reference map;
	struct tag_block type_definitions;
	long unused50[12];
};

typedef char detail_object_collection_definition_size_assert[
	sizeof(struct detail_object_collection_definition) == 0x80 ? 1 : -1];

#endif // __DETAIL_OBJECT_DEFINITIONS_H
