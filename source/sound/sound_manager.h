/*
SOUND_MANAGER.H
*/

#ifndef __SOUND_MANAGER_H
#define __SOUND_MANAGER_H
#pragma once

/* ---------- headers */

#include "sound/game_sound.h"

/* ---------- constants */

/* sound channel states */
enum
{
	NUMBER_OF_SOUND_CHANNEL_STATES = 3
};

/* sound channel flags */
enum
{
	_sound_channel_3d_bit,
	_sound_channel_stereo_bit,
	_sound_channel_44k_bit,
	_sound_channel_compressed_bit
};

/* sound spatialization modes */
enum
{
	_sound_spatialization_mode_none,
	_sound_spatialization_mode_absolute,
	_sound_spatialization_mode_relative,
	NUMBER_OF_SOUND_SPATIALIZATION_MODES
};

enum looping_sound_refresh_state
{
	_looping_sound_refresh_start,
	_looping_sound_refresh_loop,
	_looping_sound_refresh_stop,
	NUMBER_OF_LOOPING_SOUND_REFRESH_STATES,
};

/* ---------- structures */

struct platform_sound_channel_properties
{
	real minimum_distance;
	real maximum_distance;
	real pitch;
	real gain;
	real cone_inside_angle;
	real cone_outside_angle;
	real cone_outside_gain;
	real reverb_attenuation;
};

struct platform_sound_listener_properties
{
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
	real_vector3d velocity;
	struct sound_environment_definition const *environment;
};

struct sound_source
{
	short spatialization_mode;
	short pad_2;
	real scale;
	real gain;
	struct sound_location location;
	real obstruction;
	real occlusion;
};

struct sound_source;
struct sound_environment_definition;

/* ---------- prototypes/SOUND_MANAGER.C */

boolean sound_valid_for_channel(
	short compression,
	short encoding,
	short sample_rate,
	short spatialization_mode,
	short channel_type_flags);

void sound_dispose(
	void);
void sound_enable(
	boolean enable);

void sound_initialize(
	void);
void sound_initialize_for_new_map(
	void);
void sound_dispose_from_old_map(
	void);
void sound_reconnect_to_structure_bsp(
	void);

long sound_render_time(
	void);
void sound_render(
	void);

void sound_stop_all(
	void);
void sound_stop_impulse(
	long sound_index);
void sound_stop_impulse_by_source_and_definition(
	long source_identifier,
	long definition_index);
long sound_new_impulse(
	long definition_index,
	struct sound_source *source,
	long source_identifier,
	boolean (*track_proc)(
		long source_identifier,
		void const *track_data,
		struct sound_source *source),
	void const *track_data,
	short track_data_size);
void sound_pause(
	boolean paused);
void sound_idle(
	void);
void sound_manager_set_sound_environment(
	struct sound_environment_definition const *environment);
boolean sound_refresh_looping(
	long definition_index,
	long looping_sound_index,
	struct sound_source *source,
	short refresh_state,
	boolean alternate,
	real fade_time);

/* ---------- globals */

extern boolean debug_sound;

#endif // __SOUND_MANAGER_H
