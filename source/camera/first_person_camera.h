/*
FIRST_PERSON_CAMERA.H
*/

#ifndef __FIRST_PERSON_CAMERA_H
#define __FIRST_PERSON_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"

/* ---------- structures */

struct first_person_camera
{
	real field_of_view;
};

struct camera_control;
struct observer_command;

/* ---------- prototypes/FIRST_PERSON_CAMERA.C */

void first_person_camera_new(
	struct first_person_camera *camera);
void first_person_camera_deterministic(
	long unit_index,
	real_point3d *position,
	real_vector3d *forward);
void first_person_camera_fake(
	long unit_index,
	struct observer_command *result);
void first_person_camera_update(
	struct first_person_camera *camera,
	struct camera_control const *controls,
	struct observer_command *result);

#endif // __FIRST_PERSON_CAMERA_H
