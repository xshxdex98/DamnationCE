/*
CAMERA_TRACK_DEFINITIONS.H
*/

#ifndef __CAMERA_TRACK_DEFINITIONS_H
#define __CAMERA_TRACK_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	CAMERA_TRACK_DEFINITION_TAG = 'trak'
};

/* ---------- structures */

struct camera_track_definition
{
	unsigned long flags;
	struct tag_block control_points;
	long unused[8];
};

struct camera_track_control_point
{
	real_vector3d position;
	real_quaternion orientation;
	long unused[8];
};

typedef char camera_track_definition_size_assert[
	sizeof(struct camera_track_definition) == 0x30 ? 1 : -1];
typedef char camera_track_control_point_size_assert[
	sizeof(struct camera_track_control_point) == 0x3C ? 1 : -1];

#endif // __CAMERA_TRACK_DEFINITIONS_H
