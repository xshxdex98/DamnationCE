/*
INPUT_ABSTRACTION.H
*/

#ifndef __INPUT_ABSTRACTION_H
#define __INPUT_ABSTRACTION_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"

/* ---------- constants */

enum
{
	_game_control_jump,
	_game_control_switch_grenades,
	_game_control_action,
	_game_control_switch_weapons,
	_game_control_melee,
	_game_control_flashlight,
	_game_control_grenade,
	_game_control_primary_trigger,
	_game_control_start,
	_game_control_back,
	_game_control_crouch,
	_game_control_zoom,
	NUMBER_OF_GAME_CONTROLS,
};

/* ---------- structures */

struct game_input_preferences
{
	real yaw_rate;
	real pitch_rate;
	byte game_control_to_xbox_buttons[12];
	short joystick_controls;
	boolean invert_look;
	boolean invert_look_aircraft_control;
};

typedef char verify_game_input_preferences_size[
	sizeof(struct game_input_preferences) == 0x18 ? 1 : -1];

struct game_input_state
{
	byte buttons[NUMBER_OF_GAME_CONTROLS];
	real forward_movement;
	real strafe;
	real yaw;
	real pitch;
};

typedef char game_input_state_size_assert[
	sizeof(struct game_input_state) == 0x1C ? 1 : -1];

/* ---------- prototypes/INPUT_ABSTRACTION.C */

void input_abstraction_initialize(
	void);
void input_abstraction_dispose(
	void);
void input_abstraction_update(
	void);
void input_abstraction_update_device_changes(
	unsigned long device_change_flags);
void input_abstraction_reset_controller_detection_timer(
	void);
void input_abstraction_get_local_player_preferences(
	short local_player_index,
	struct game_input_preferences *preferences);
void input_abstraction_update_local_player_preferences(
	short controller_index,
	struct game_input_preferences const *preferences);
struct game_input_state *input_abstraction_get_input_state(
	short local_player_index);
/* port: the keyboard and mouse's own controls (input_abstraction.c) */
boolean input_abstraction_port_reload(
	short controller_index);
byte input_abstraction_port_accept(
	short controller_index);
boolean input_abstraction_port_action_only(
	short controller_index);
real input_abstraction_port_primary_trigger(
	short controller_index);
boolean input_abstraction_port_crouch(
	short controller_index);
/* port: the game control on each of the controller's buttons, and the
player's look inverted as its stick is (the touch controls) */
void input_abstraction_port_button_controls(
	short controller_index,
	short *controls);
boolean input_abstraction_port_look_inverted(
	short controller_index);

#endif // __INPUT_ABSTRACTION_H
