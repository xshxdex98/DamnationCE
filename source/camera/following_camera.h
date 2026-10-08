/*
FOLLOWING_CAMERA.H
*/

#ifndef __FOLLOWING_CAMERA_H
#define __FOLLOWING_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

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

struct following_camera
{
	boolean initialized;
	boolean confined;
	boolean crouched;
	boolean zoomed;
	short zoom_level;
	short pad6;
	long unit_index;
	short seat_index;
	short padE;
	real_euler_angles2d facing_offset;
	real distance_scale;
};

struct following_camera_control;
struct following_camera_result;

/* ---------- prototypes/FOLLOWING_CAMERA.C */

void following_camera_new(
	struct following_camera *camera);
void following_camera_update(
	struct following_camera *camera,
	struct following_camera_control const *controls,
	struct following_camera_result *result);
void following_camera_deterministic(
	long unit_index,
	real_point3d *position,
	real_vector3d *forward);

#endif // __FOLLOWING_CAMERA_H
