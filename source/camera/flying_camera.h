/*
FLYING_CAMERA.H
*/

#ifndef __FLYING_CAMERA_H
#define __FLYING_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"

/* ---------- structures */

struct flying_camera
{
	real_point3d position;
	real_euler_angles2d facing;
	real roll;
	real field_of_view;
};

struct camera_control;
struct observer_command;

/* ---------- prototypes/FLYING_CAMERA.C */

void flying_camera_new(
	struct flying_camera *camera);
void flying_camera_new_from_point_and_vector(
	struct flying_camera *camera,
	real_point3d const *position,
	real_vector3d const *forward);
void flying_camera_update(
	struct flying_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result);

#endif // __FLYING_CAMERA_H
