/*
UNIT_CONTROL_DATA.H
*/

#ifndef __UNIT_CONTROL_DATA_H
#define __UNIT_CONTROL_DATA_H
#pragma once

/* ---------- headers */

#include "cseries.h"
#include "math/real_math.h"

/* ---------- structures */

struct unit_control_data
{
	char animation_state;
	char aiming_speed;
	word control_flags;
	short weapon_index;
	short grenade_index;
	short zoom_level;
	word pad;
	real_vector3d throttle;
	real primary_trigger;
	real_vector3d facing_vector;
	real_vector3d aiming_vector;
	real_vector3d looking_vector;
};

typedef char unit_control_data_size_assert[
	sizeof(struct unit_control_data) == 0x40 ? 1 : -1];

#endif // __UNIT_CONTROL_DATA_H
