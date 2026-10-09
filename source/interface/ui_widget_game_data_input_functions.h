/*
UI_WIDGET_GAME_DATA_INPUT_FUNCTIONS.H
*/

#ifndef __UI_WIDGET_GAME_DATA_INPUT_FUNCTIONS_H
#define __UI_WIDGET_GAME_DATA_INPUT_FUNCTIONS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"

/* ---------- structures */

struct widget_instance;

typedef void (*ui_widget_game_data_function)(
	struct widget_instance *widget);

struct single_player_level_entry
{
	char const *map_name;
	boolean available;
	boolean completion_marker;
	boolean difficulty_marker;
	boolean cooperative_marker;
};

/* ---------- prototypes/UI_WIDGET_GAME_DATA_INPUT_FUNCTIONS.C */

void ui_widget_game_data_function_invoke(
	struct widget_instance *widget,
	word function);

#endif // __UI_WIDGET_GAME_DATA_INPUT_FUNCTIONS_H
