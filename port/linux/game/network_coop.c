/*
NETWORK_COOP.C

What a co-op game's host shows and plays its players, on every machine
(port/linux/NETCODE.md). A network game on a campaign map, which no game
engine runs, is co-op, and only its host runs the map's scripts (game.c):
the cinematics they start, the camera they move and the screen fades they
make are the host's alone. Each tick the host sends its clients those: whether
a cinematic is in progress and its letterbox shown, where its camera is and
looks (whatever moves it: a camera point, an animation), and its screen
fade. A client starts the cinematic as the host did (its players' input off,
the letterbox), sees through the host's camera until it ends, and fades as
the host faded. A client that hears nothing for a while ends the cinematic
it started, rather than keep its players still for good.

The sounds the scripts play (game_sound.c: dialogue, music, ambience) go to
the clients too, each in a few ticks' messages in case one is lost, with a
number a client plays each once by.
*/

/* ---------- headers */

#include "cseries.h"
#include "cache/cache_files.h"
#include "camera/camera_scripting.h"
#include "camera/observer.h"
#include "cutscene/cinematics.h"
#include "effects/player_effects.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "sound/game_sound.h"
#include "sound/sound_definitions.h"
#include "network_distributed.h"

/* ---------- constants */

enum
{
	/* a client's cinematic ended after this long without word of it */
	PRESENTATION_SILENCE_TICKS = 2 * TICKS_PER_SECOND,
	/* the field of view, in radians, as a word */
	FIELD_OF_VIEW_SCALE = 10000,
	/* the scripts' sounds kept to send, and in how many ticks' messages each goes */
	MAXIMUM_QUEUED_SOUNDS = 32,
	SOUND_SENDS = 3,
};

/* struct distributed_coop_presentation flags */
enum
{
	_presentation_cinematic_bit = 0,
	_presentation_letterbox_bit,
	_presentation_fading_out_bit,
};

/* ---------- structures */

struct distributed_coop_presentation
{
	byte flags;
	byte fade_color[3];
	short fade_ticks;
	/* ticks since the fade began (as far as a short counts) */
	short fade_elapsed;
	/* the game time the host's fade began: a new value is a new fade */
	long fade_start_time;
	real_point3d camera_position;
	struct distributed_vector camera_forward;
	struct distributed_vector camera_up;
	word camera_field_of_view;
	word pad;
};

struct distributed_coop_presentation_message
{
	struct distributed_message_header header;
	struct distributed_coop_presentation presentation;
};

/* a sound the host's scripts played (network_coop_note_sound's kinds) */
struct distributed_coop_sound
{
	byte kind;
	byte pad;
	word number;
	long definition_index;
	long object_index;
	real scale;
};

struct distributed_coop_sounds_message
{
	struct distributed_message_header header;
	struct distributed_coop_sound sounds[MAXIMUM_QUEUED_SOUNDS];
};

/* ---------- globals */

/* (a client) the cinematic it started for the host's, when it last heard,
and the host's fade it last made */
static struct
{
	boolean cinematic_started;
	long heard_time;
	long fade_start_time;
} coop_presentation;

/* (the host) the scripts' sounds not yet sent SOUND_SENDS times, and the
next one's number; (a client) the number of the last played */
static struct
{
	struct distributed_coop_sound sounds[MAXIMUM_QUEUED_SOUNDS];
	short sends[MAXIMUM_QUEUED_SOUNDS];
	short count;
	word next_number;
	word played_number;
	boolean played_any;
} coop_sounds;

/* ---------- private code */

static boolean coop_game(
	void)
{
	return !game_engine_running();
}

/* whether a tag index the host sent is a tag of the group */
static boolean tag_of_group(
	long tag_index,
	unsigned long group_tag)
{
	struct tag_iterator iterator;
	long index;

	tag_iterator_new(&iterator, group_tag);
	while ((index = tag_iterator_next(&iterator)) != NONE)
	{
		if (index == tag_index)
			return TRUE;
	}

	return FALSE;
}

static void client_cinematic_end(
	void)
{
	if (!coop_presentation.cinematic_started)
		return;
	coop_presentation.cinematic_started = FALSE;
	scripted_camera_enable(FALSE);
	cinematic_stop();
}

/* ---------- public code */

void network_coop_new_game(
	void)
{
	csmemset(&coop_presentation, 0, sizeof(coop_presentation));
	csmemset(&coop_sounds, 0, sizeof(coop_sounds));
}

/* (the host) a sound its scripts played (game_sound.c), for its clients */
void network_coop_note_sound(
	short kind,
	long definition_index,
	long object_index,
	real scale)
{
	struct distributed_coop_sound *sound;

	if (game_connection() != _game_connection_network_server || !coop_game() ||
		coop_sounds.count == MAXIMUM_QUEUED_SOUNDS)
	{
		return;
	}
	sound = &coop_sounds.sounds[coop_sounds.count];
	sound->kind = (byte)kind;
	sound->pad = 0;
	sound->number = ++coop_sounds.next_number;
	sound->definition_index = definition_index;
	sound->object_index = object_index;
	sound->scale = scale;
	coop_sounds.sends[coop_sounds.count] = 0;
	coop_sounds.count++;
}

/* (the host, after each tick) its presentation, to every client */
void network_coop_host_tick(
	void)
{
	struct distributed_coop_presentation_message message;
	struct distributed_coop_presentation *presentation = &message.presentation;
	struct observer_result const *camera = observer_get_camera(0);
	real_rgb_color fade_color;
	boolean fading_out;
	long elapsed;

	if (!coop_game())
		return;
	csmemset(presentation, 0, sizeof(*presentation));
	SET_FLAG(presentation->flags, _presentation_cinematic_bit, cinematic_in_progress());
	SET_FLAG(presentation->flags, _presentation_letterbox_bit, cinematic_globals->show_letterbox);
	player_effect_port_screen_fade_get(&fade_color, &presentation->fade_ticks, &fading_out,
		&presentation->fade_start_time);
	SET_FLAG(presentation->flags, _presentation_fading_out_bit, fading_out);
	presentation->fade_color[0] = (byte)(PIN(fade_color.red, 0.0f, 1.0f) * 255.0f + 0.5f);
	presentation->fade_color[1] = (byte)(PIN(fade_color.green, 0.0f, 1.0f) * 255.0f + 0.5f);
	presentation->fade_color[2] = (byte)(PIN(fade_color.blue, 0.0f, 1.0f) * 255.0f + 0.5f);
	elapsed = game_time_get() - presentation->fade_start_time;
	presentation->fade_elapsed = (short)PIN(elapsed, 0, SHORT_MAX);
	if (camera)
	{
		presentation->camera_position = camera->position;
		distributed_vector_pack(&camera->forward, DISTRIBUTED_UNIT_SCALE, &presentation->camera_forward);
		distributed_vector_pack(&camera->up, DISTRIBUTED_UNIT_SCALE, &presentation->camera_up);
		presentation->camera_field_of_view =
			(word)PIN(camera->field_of_view * FIELD_OF_VIEW_SCALE + 0.5f, 1, UNSIGNED_SHORT_MAX);
	}
	distributed_send(&message, _distributed_message_coop_presentation, 1, (word)sizeof(message), _distributed_to_clients);

	/* the scripts' sounds, each in SOUND_SENDS ticks' messages */
	if (coop_sounds.count)
	{
		struct distributed_coop_sounds_message sounds;
		short index;

		csmemcpy(sounds.sounds, coop_sounds.sounds, coop_sounds.count * sizeof(sounds.sounds[0]));
		distributed_send(&sounds, _distributed_message_coop_sounds, coop_sounds.count,
			(word)(sizeof(sounds.header) + coop_sounds.count * sizeof(sounds.sounds[0])), _distributed_to_clients);
		for (index = 0; index < coop_sounds.count; index++)
		{
			if (++coop_sounds.sends[index] < SOUND_SENDS)
				continue;
			coop_sounds.sounds[index] = coop_sounds.sounds[coop_sounds.count - 1];
			coop_sounds.sends[index] = coop_sounds.sends[coop_sounds.count - 1];
			coop_sounds.count--;
			index--;
		}
	}
}

/* (a client, after each tick) the cinematic it started ended if the host
has gone quiet */
void network_coop_client_tick(
	void)
{
	if (coop_presentation.cinematic_started &&
		game_time_get() - coop_presentation.heard_time > PRESENTATION_SILENCE_TICKS)
	{
		client_cinematic_end();
	}
}

word network_coop_presentation_entry_size(
	void)
{
	return sizeof(struct distributed_coop_presentation);
}

word network_coop_sound_entry_size(
	void)
{
	return sizeof(struct distributed_coop_sound);
}

/* (a client) the host's scripts' sounds, each played once, in their order */
void network_coop_handle_sounds(
	void const *entries,
	short count)
{
	struct distributed_coop_sound const *sounds = entries;
	short index;

	if (!coop_game())
		return;
	for (index = 0; index < count; index++)
	{
		struct distributed_coop_sound const *sound = &sounds[index];
		long object_index = sound->object_index != NONE && network_objects_client_has(sound->object_index) ?
			sound->object_index : NONE;
		real scale = PIN(sound->scale, 0.0f, 1.0f);

		/* (a number not after the last played is one already played) */
		if (coop_sounds.played_any && (short)(sound->number - coop_sounds.played_number) <= 0)
			continue;
		coop_sounds.played_number = sound->number;
		coop_sounds.played_any = TRUE;
		switch (sound->kind)
		{
		case _coop_sound_impulse:
			if (tag_of_group(sound->definition_index, SOUND_DEFINITION_TAG))
				scripted_sound_new(sound->definition_index, object_index, scale);
			break;
		case _coop_sound_looping_start:
			if (tag_of_group(sound->definition_index, LOOPING_SOUND_DEFINITION_TAG))
				scripted_looping_sound_start(sound->definition_index, object_index, scale);
			break;
		case _coop_sound_looping_stop:
			if (tag_of_group(sound->definition_index, LOOPING_SOUND_DEFINITION_TAG))
				scripted_looping_sound_stop(sound->definition_index);
			break;
		default:
			break;
		}
	}
}

/* (a client) the host's presentation, shown here */
void network_coop_handle_presentation(
	void const *entries)
{
	struct distributed_coop_presentation const *presentation = entries;
	boolean cinematic = TEST_FLAG(presentation->flags, _presentation_cinematic_bit);

	if (!coop_game())
		return;
	coop_presentation.heard_time = game_time_get();

	/* the cinematic: started, seen through the host's camera, ended */
	if (cinematic && !coop_presentation.cinematic_started && !cinematic_in_progress())
	{
		cinematic_start();
		scripted_camera_enable(TRUE);
		coop_presentation.cinematic_started = TRUE;
	}
	else if (!cinematic)
	{
		client_cinematic_end();
	}
	if (coop_presentation.cinematic_started)
	{
		real_vector3d forward, up;

		cinematic_show_letterbox(TEST_FLAG(presentation->flags, _presentation_letterbox_bit));
		distributed_vector_unpack(&presentation->camera_forward, DISTRIBUTED_UNIT_SCALE, &forward);
		distributed_vector_unpack(&presentation->camera_up, DISTRIBUTED_UNIT_SCALE, &up);
		if (distributed_point_valid(&presentation->camera_position, UNIT_WORLD_BOUND) &&
			distributed_axes_make_valid(&forward, &up))
		{
			scripted_camera_set_camera_point_relative(&presentation->camera_position, &forward, &up,
				(real)presentation->camera_field_of_view / FIELD_OF_VIEW_SCALE, 0, NONE);
		}
	}

	/* the fade, once for each the host makes, begun as long ago as the host's */
	if (presentation->fade_start_time != coop_presentation.fade_start_time)
	{
		real_rgb_color color;

		color.red = presentation->fade_color[0] / 255.0f;
		color.green = presentation->fade_color[1] / 255.0f;
		color.blue = presentation->fade_color[2] / 255.0f;
		player_effect_port_screen_fade_set(&color, presentation->fade_ticks,
			TEST_FLAG(presentation->flags, _presentation_fading_out_bit),
			game_time_get() - presentation->fade_elapsed);
		coop_presentation.fade_start_time = presentation->fade_start_time;
	}
}
