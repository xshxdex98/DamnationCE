/*
UI_WIDGET_INSTANCE.H

The widget instance, shared by every file that touches one.
*/

#ifndef __UI_WIDGET_INSTANCE_H
#define __UI_WIDGET_INSTANCE_H
#pragma once

#include "cseries/cseries.h"

struct widget_animation_data
{
	short current_frame_index;
	short first_frame_index;
	short last_frame_index;
	short number_of_sprite_frames;
};

typedef char widget_animation_data_size_assert[
	sizeof(struct widget_animation_data) == 0x8 ? 1 : -1];

struct widget_instance
{
	int definition_tag_index;
	char const *name;
	short local_player_index;
	short horizontal_offset;
	short vertical_offset;
	short type;
	boolean visible;
	boolean render_regardless_of_controller_index;
	boolean disabled;
	boolean pause_game_time;
	boolean delete_recursion_lock;
	boolean widget_is_error_dialog;
	boolean close_if_local_player_controller_present;
	byte pad17;
	int creation_time;
	unsigned int milliseconds_to_auto_close;
	unsigned int auto_close_fade_time;
	real alpha_modifier;
	struct widget_instance *previous;
	struct widget_instance *next;
	struct widget_instance *parent;
	struct widget_instance *child;
	struct widget_instance *focused_child;
	union
	{
		struct
		{
			wchar_t *text;
			short string_list_index;
		} text_box;
		struct
		{
			short selected_index;
			/* counted back toward zero one step per rendered frame; the two
			tab functions start it at +15 and -15 and the column list renderer
			clears it */
			short last_list_tab_direction;
			void *list_items;
			word number_of_items;
			struct widget_instance *extended_description;
			wchar_t *item_text;
		} list;
		int value;
	} parameters;
	struct widget_animation_data animation;
};

#endif
