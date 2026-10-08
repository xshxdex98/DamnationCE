/*
HUD_MESSAGING.H

header included in hcex build.
*/

#ifndef __HUD_MESSAGING_H
#define __HUD_MESSAGING_H
#pragma once

/* ---------- constants */

/* hud icon types */
enum
{
	_icon_a_button,
	_icon_b_button,
	_icon_x_button,
	_icon_y_button,
	_icon_black_button,
	_icon_white_button,
	_icon_left_trigger,
	_icon_right_trigger,
	_icon_dpad_up,
	_icon_dpad_down,
	_icon_dpad_left,
	_icon_dpad_right,
	_icon_start_button,
	_icon_back_button,
	_icon_left_thumb,
	_icon_right_thumb,
	_icon_left_stick,
	_icon_right_stick,
	_icon_action,
	_icon_throw_grenade,
	_icon_primary_trigger,
	_icon_integrated_light,
	_icon_jump,
	_icon_use_equipment,
	_icon_rotate_weapons,
	_icon_rotate_grenades,
	_icon_crouch,
	_icon_zoom,
	_icon_accept,
	_icon_back,
	_icon_move,
	_icon_look,
	_icon_custom_1,
	_icon_custom_2,
	_icon_custom_3,
	_icon_custom_4,
	_icon_custom_5,
	_icon_custom_6,
	_icon_custom_7,
	_icon_custom_8,
	NUMBER_OF_ICON_TYPES
};

/* hud number display flags */
enum
{
	_hud_number_show_all_leading_zeros_bit,
	_hud_number_show_only_when_zoomed_bit,
	_hud_number_show_trailing_m_bit,
	NUMBER_OF_HUD_NUMBER_SHOW_FLAGS
};

/* hud icon flags */
enum
{
	_hud_icon_use_text_bit,
	_hud_icon_use_color_bit,
	_hud_icon_absolute_width_bit,
	NUMBER_OF_HUD_ICON_FLAGS
};

/* ---------- macros */

/* ---------- structures */

struct icon_hud_element_definition;
union real_argb_color;

/* ---------- prototypes/HUD_MESSAGING.C */

void hud_messaging_initialize(
	void);
void hud_messaging_initialize_for_new_map(
	void);
void hud_messaging_dispose_from_old_map(
	void);
void hud_messaging_dispose(
	void);
/* port: network co-op (port/linux/game/network_coop.c) */
struct hud_timer_state
{
	long reference_time;
	short ticks;
	short flash_cutoff;
	short x, y;
	short corner;
	boolean paused;
	boolean enabled;
};
void hud_messaging_port_timer_get(
	struct hud_timer_state *state);
void hud_messaging_port_timer_set(
	struct hud_timer_state const *state);
short hud_messaging_port_message_count(
	void);

void scripted_hud_set_state_message(
	short message_index);
void hud_messaging_globals_update(
	void);
void scripted_hud_set_flashing_state(
	boolean flash);
void scripted_hud_restart_flashing(
	void);
void scripted_hud_set_objective(
	short message_index);
void scripted_hud_set_timer_position(
	short x,
	short y,
	short corner);
void scripted_hud_show_timer(
	boolean show);
void scripted_hud_pause_timer(
	boolean pause);
void scripted_hud_set_timer_time(
	short minutes,
	word seconds);
void scripted_hud_set_timer_warning_cutoff(
	short minutes,
	word seconds);
short scripted_hud_get_timer_ticks(
	void);
void scripted_hud_time_code_show(
	boolean show);
void scripted_hud_time_code_start(
	boolean start);
void scripted_hud_time_code_reset(
	void);
void scripted_hud_messages_clear(
	void);
void hud_render_timer(
	void);
void hud_print_message(
	short local_player_index,
	wchar_t const *message);
void hud_add_item_message(
	short local_player_index,
	long item_definition_index,
	short quantity,
	char message_offset);
void hud_broadcast_team_message(
	long victim_player_index,
	wchar_t const *message);
void hud_messaging_update(
	short local_player_index);
void hud_set_state_message(
	short local_player_index,
	short message_index);
void hud_set_state_message_text(
	short local_player_index,
	short custom_icon_index,
	short icon_string_index,
	boolean uses_scenario_names);
void hud_set_state_message_icon(
	short local_player_index,
	short custom_icon_index,
	struct icon_hud_element_definition const *icon);
void hud_enable_custom_state_message(
	short local_player_index,
	boolean enabled);
void hud_set_state_text(
	short local_player_index,
	wchar_t const *message);
wchar_t *hud_messaging_get_objective(
	void);
long hud_get_font_index(
	void);
union real_argb_color *hud_get_text_color(
	union real_argb_color *result);

/* ---------- globals */

/* ---------- public code */

#endif // __HUD_MESSAGING_H
