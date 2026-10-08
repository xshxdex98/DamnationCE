/*
ACTION_SLEEP.C
*/

/* ---------- headers */

#include "cseries/cseries.h"
#include "ai/actors.h"

/* ---------- constants */

enum
{
	_actor_persistent_control_ticks_offset = 0x3FC,
};

/* ---------- public code */

void action_sleep_control(
	long actor_index)
{
	*(short *)((byte *)actor_get(actor_index) + _actor_persistent_control_ticks_offset) = 0;
}

