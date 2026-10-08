/*
STATIC_CAMERA.H
*/

#ifndef __STATIC_CAMERA_H
#define __STATIC_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"

/* ---------- structures */

struct static_camera
{
	real_point3d position;
	long location;
	real_vector3d forward;
	real_vector3d up;
	real field_of_view;
	long timer;
	long flags;
	boolean initialized;
};

struct camera_control;
struct observer_command;

/* ---------- prototypes/STATIC_CAMERA.C */

void static_camera_new(
	struct static_camera *camera,
	real_point3d const *position,
	long location,
	real_vector3d const *forward,
	real_vector3d const *up,
	real field_of_view,
	long timer,
	long flags);
void static_camera_update(
	struct static_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result);

#endif // __STATIC_CAMERA_H
