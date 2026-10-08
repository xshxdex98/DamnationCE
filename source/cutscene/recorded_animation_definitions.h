/*
RECORDED_ANIMATION_DEFINITIONS.H
*/

#ifndef __RECORDED_ANIMATION_DEFINITIONS_H
#define __RECORDED_ANIMATION_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "math/real_math.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct recorded_unit_control
{
	byte byte_field0;
	byte byte_field1;
	short word_field2;
	short word_field4;
	short version2_field;
	short version3_field;
	short unused_field10;
	real_vector2d vector2d_field12;
	long long_field20;
	long version1_field;
	real_vector3d vector3d_field28;
	real_vector3d vector3d_field40;
	real_vector3d vector3d_field52;
};

struct scenario;

struct recorded_animation_definition
{
	char name[TAG_STRING_LENGTH+1];
	byte version;
	char raw_animation_data;
	byte unit_control_data_version;
	byte pad;
	short length_in_ticks;
	word pad2;
	unsigned long pad3;
	struct tag_data event_stream;
};

/* ---------- prototypes/RECORDED_ANIMATION_DEFINITIONS.C */

short scenario_get_animation_by_name(struct scenario const *scenario, char const *name);

/* ---------- globals */

/* ---------- public code */

#endif // __RECORDED_ANIMATION_DEFINITIONS_H
