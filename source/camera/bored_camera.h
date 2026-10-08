/*
BORED_CAMERA.H
*/

#ifndef __BORED_CAMERA_H
#define __BORED_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct unit_camera_track
{
	struct tag_reference track;
	long unused[3];
};

struct bored_camera
{
	unsigned long last_update_milliseconds;
	long timer_milliseconds;
	long boredom_count;
};

struct camera_action;
struct camera_command;

/* ---------- prototypes/BORED_CAMERA.C */

void bored_camera_new(
	struct bored_camera *camera);
void bored_camera_update(
	struct bored_camera *camera,
	struct camera_action const *action,
	struct camera_command *result);

boolean is_bored(
	void);

boolean is_still_bored(
	void);

/* ---------- globals */

/* ---------- public code */

#endif // __BORED_CAMERA_H
