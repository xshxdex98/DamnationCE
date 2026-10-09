/*
UI_WIDGET_DEFINITIONS.H

The start of the UI widget tag definition.
*/

#ifndef __UI_WIDGET_DEFINITIONS_H
#define __UI_WIDGET_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "tag_files/tag_groups.h"

/* ---------- constants */

/* widget types */
enum
{
	_ui_widget_type_container = 0,
	_ui_widget_type_text_box,
	_ui_widget_type_spinner_list,
	_ui_widget_type_column_list,
	_ui_widget_type_game_model,
	_ui_widget_type_movie,
	_ui_widget_type_custom,
	NUMBER_OF_UI_WIDGET_TYPES
};

/* list flags */
enum
{
	_list_items_generated_in_code,
	_list_items_generated_from_string_list_tag,
	_list_items_only_one_tooltip_entry,
	_list_single_preview_box_no_scroll,
	NUMBER_OF_UI_WIDGET_LIST_FLAGS
};

enum
{
	UI_WIDGET_DEFINITION_TAG = 'DeLa'
};

/* ---------- macros */

#define ui_widget_definition_get(index) \
	((struct ui_widget_definition *)tag_get(UI_WIDGET_DEFINITION_TAG, (index)))

/* ---------- structures */

struct ui_widget_definition
{
	short type;
	short controller_index;
	char name[32];
	rectangle2d bounds;
	long flags;
	long milliseconds_to_auto_close;
	long auto_close_fade_time;
	struct tag_reference background_bitmap;
	struct tag_block game_data_inputs;
	struct tag_block event_handlers;
	struct tag_block search_and_replace_functions;
	byte unknown06C[0xEC - 0x6C];
	struct tag_reference text_label_string_list;
	struct tag_reference text_font;
	real_argb_color text_color;
	short justification;
	word text_box_flags;
	byte unknown120[0x12E - 0x120];
	short string_list_index;
	short horizontal_offset;
	short vertical_offset;
	byte unknown134[0x150 - 0x134];
	long list_flags;
	struct tag_reference list_header_bitmap;
	struct tag_reference list_footer_bitmap;
	rectangle2d list_header_bounds;
	rectangle2d list_footer_bounds;
	byte unknown184[0x1A4 - 0x184];
	struct tag_reference extended_description_widget;
	byte unknown1B4[0x2D4 - 0x1B4];
	struct tag_block conditional_widgets;
	byte unknown2E0[0x3E0 - 0x2E0];
	struct tag_block child_widgets;
};

#endif // __UI_WIDGET_DEFINITIONS_H
