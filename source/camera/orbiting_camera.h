/*
ORBITING_CAMERA.H
*/

#ifndef __ORBITING_CAMERA_H
#define __ORBITING_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"

/* ---------- structures */

struct orbiting_camera
{
	real_euler_angles2d facing;
	real distance;
};

struct camera_control;
struct observer_command;

/* ---------- prototypes/ORBITING_CAMERA.C */

void orbiting_camera_new(
	struct orbiting_camera *camera,
	real distance,
	real_vector3d const *forward);
void orbiting_camera_update(
	struct orbiting_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result);

#endif // __ORBITING_CAMERA_H
