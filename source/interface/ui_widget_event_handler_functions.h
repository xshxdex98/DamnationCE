/*
UI_WIDGET_EVENT_HANDLER_FUNCTIONS.H
*/

#ifndef __UI_WIDGET_EVENT_HANDLER_FUNCTIONS_H
#define __UI_WIDGET_EVENT_HANDLER_FUNCTIONS_H
#pragma once

/* ---------- structures */

struct event_record;
struct widget_instance;

/* ---------- prototypes/UI_WIDGET_EVENT_HANDLER_FUNCTIONS.C */

boolean ui_widget_event_handler_function_invoke(
	struct widget_instance *widget,
	struct event_record *event,
	word function_index,
	boolean *widget_deleted);

void reset_last_player1_profile_index(
	void);

/* port: a function's name by its index, as the tags name it (NULL past
the last) */
char const *ui_widget_event_handler_function_name(
	long function_index);

#endif // __UI_WIDGET_EVENT_HANDLER_FUNCTIONS_H
