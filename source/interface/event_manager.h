/*
EVENT_MANAGER.H
*/

#ifndef __EVENT_MANAGER_H
#define __EVENT_MANAGER_H
#pragma once

#include "math/integer_math.h"

/* ---------- structures */

struct event_record
{
	short type;
	short controller_index;
	union event_record_data
	{
		point2d stick;
		struct event_record_button
		{
			byte index;
			byte value;
		} button;
		long value;
	} data;
};

typedef char event_record_size_assert[
	sizeof(struct event_record) == 0x8 ? 1 : -1];

/* ---------- prototypes/EVENT_MANAGER.C */

void event_manager_initialize(
	void);

void event_manager_dispose(
	void);

void event_manager_suppress(
	boolean suppress);

boolean get_next_event(
	struct event_record *event,
	short local_player_index);

unsigned long event_manager_time_of_last_event(
	void);

void event_manager_flush(
	void);

void event_manager_update(
	void);

/* a press of a button, as if the controller had just pressed it (the menus'
mouse pointer, ui_widget.c) */
void event_manager_post_button(
	short controller_index,
	short button_index);

#endif // __EVENT_MANAGER_H
