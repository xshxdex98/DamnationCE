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

The devices (doors, elevators, switches, lights) are the host's: what moves
or powers them is a device group's value (devices.c), which the scripts, a
door a unit walks up to and a switch a player uses (relayed to the host)
set on the host alone; a client sets none itself. The host sends each
group's value when it changes, in a few ticks' messages, and a few of them
every tick besides, round them all, for a client that missed one or has
just joined. A group is named by its index if it is one of the scenario's
(the same everywhere), else by the device it is its own of: the device's
name, or its index and tag (the map placed it at the same index
everywhere). A client's devices move to the value as the host's did, or are
put straight there where the host's were (a script's immediate set).

The objects the scripts create and destroy by name (scenery and devices:
network_objects.c sends the rest) are the host's too: twice a second it
sends which named objects it has, and a client that has seen the same
difference twice running, on the same structure BSP, creates or deletes its
own to match.
*/

/* ---------- headers */

#include "cseries.h"
#include "cache/cache_files.h"
#include "camera/camera_scripting.h"
#include "camera/observer.h"
#include "cutscene/cinematics.h"
#include "devices/devices.h"
#include "effects/player_effects.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
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
	/* the device groups a map has at most (devices.c's), a change of one
	sent in this many ticks' messages, and how many each tick has besides */
	MAXIMUM_DEVICE_GROUPS = 1024,
	DEVICE_GROUP_SENDS = 3,
	DEVICE_GROUP_REFRESHES_PER_TICK = 8,
	/* how often the host sends which named objects it has */
	OBJECT_NAMES_INTERVAL_TICKS = TICKS_PER_SECOND / 2,
	OBJECT_NAME_BYTES = MAXIMUM_OBJECT_NAMES_PER_SCENARIO / 8,
	/* the objects network_objects.c makes the host's everywhere */
	NETWORKED_OBJECT_TYPES = _object_mask_biped | _object_mask_vehicle | _object_mask_weapon | _object_mask_equipment,
};

/* struct distributed_coop_device_group roles: which of its device's groups */
enum
{
	_device_group_role_power,
	_device_group_role_position,
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

/* a device group's value: one of the scenario's (group_index), or a device's
own (group_index NONE: the device by its name, else its index and tag) */
struct distributed_coop_device_group
{
	short group_index;
	short name_index;
	long object_index;
	long definition_index;
	byte role;
	byte flags;
	/* how many times the host's devices were put straight there (a byte's worth) */
	byte snaps;
	/* whether it is sent as a change (not one of those sent round them all) */
	byte changed;
	real value;
};

struct distributed_coop_device_groups_message
{
	struct distributed_message_header header;
	struct distributed_coop_device_group groups[MAXIMUM_DEVICE_GROUPS];
};

/* which of the scenario's object names the host has an object of */
struct distributed_coop_object_names
{
	short structure_bsp_index;
	short name_count;
	byte present[OBJECT_NAME_BYTES];
};

struct distributed_coop_object_names_message
{
	struct distributed_message_header header;
	struct distributed_coop_object_names names;
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

/* (the host) each of its device groups as last sent, by the group's index
here, the ticks' messages a change is still to go in, and the next to go
round; how many times each group's devices were put straight there. (A
client) the snaps of each of its groups it has seen. */
static struct
{
	boolean noted;
	real values[MAXIMUM_DEVICE_GROUPS];
	byte flags[MAXIMUM_DEVICE_GROUPS];
	byte sent_snaps[MAXIMUM_DEVICE_GROUPS];
	byte sends[MAXIMUM_DEVICE_GROUPS];
	byte snaps[MAXIMUM_DEVICE_GROUPS];
	boolean seen[MAXIMUM_DEVICE_GROUPS];
	short refresh_next;
} coop_devices;

/* (a client) the named objects it had differ from the host's last time */
static struct
{
	byte differing[OBJECT_NAME_BYTES];
} coop_object_names;

/* (the host) this tick's device groups to send */
static struct distributed_coop_device_groups_message coop_device_groups_message;

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

/* whether the object is a player's unit or holds one (hs_library_external.c's
test, which deletes no player's) */
static boolean object_holds_player(
	long object_index)
{
	long child_index;

	if (player_index_from_unit_index(object_index) != NONE)
		return TRUE;
	for (child_index = object_get(object_index)->object.first_child_object_index; child_index != NONE;
		child_index = object_get(child_index)->object.next_object_index)
	{
		if (object_holds_player(child_index))
			return TRUE;
	}

	return FALSE;
}

/* the named object's type, or NONE (a name no object has) */
static short object_name_type(
	short name_index)
{
	struct scenario *scenario = global_scenario_get();

	if (name_index < 0 || name_index >= scenario->object_names.count)
		return NONE;

	return TAG_BLOCK_GET_ELEMENT(&scenario->object_names, name_index, struct scenario_object_name)->runtime_object_type;
}

/* (the host) a device group as sent: its value, flags and snaps; returns
whether it is one to send */
static boolean device_group_entry(
	short group_index,
	struct distributed_coop_device_group *entry)
{
	word flags;
	boolean runtime;

	if (group_index < 0 || group_index >= MAXIMUM_DEVICE_GROUPS ||
		!device_group_network_get(group_index, &entry->value, &flags, &runtime))
	{
		return FALSE;
	}
	entry->flags = (byte)flags;
	entry->snaps = coop_devices.snaps[group_index];
	entry->changed = FALSE;

	return TRUE;
}

/* (the host) every device group, the scenario's then the devices' own, in
the message; returns how many */
static short device_group_entries(
	struct distributed_coop_device_group *entries,
	short *group_indices)
{
	struct scenario *scenario = global_scenario_get();
	struct object_iterator iterator;
	struct device_datum *device;
	short count = 0, group_index;

	for (group_index = 0; group_index < scenario->device_groups.count && count < MAXIMUM_DEVICE_GROUPS; group_index++)
	{
		struct distributed_coop_device_group *entry = &entries[count];

		entry->group_index = group_index;
		entry->name_index = NONE;
		entry->object_index = NONE;
		entry->definition_index = NONE;
		entry->role = _device_group_role_power;
		if (device_group_entry(group_index, entry))
			group_indices[count++] = group_index;
	}
	object_iterator_new(&iterator, _object_mask_device, 0);
	while ((device = object_iterator_next(&iterator)) != NULL)
	{
		short roles[2];
		short role;

		roles[_device_group_role_power] = device->device.power_group_index;
		roles[_device_group_role_position] = device->device.position_group_index;
		for (role = 0; role < NUMBEROF(roles) && count < MAXIMUM_DEVICE_GROUPS; role++)
		{
			struct distributed_coop_device_group *entry = &entries[count];
			real value;
			word flags;
			boolean runtime;

			/* (the scenario's groups are sent by their index, above) */
			if (!device_group_network_get(roles[role], &value, &flags, &runtime) || !runtime)
				continue;
			entry->group_index = NONE;
			entry->name_index = device->object.name_index;
			entry->object_index = iterator.index;
			entry->definition_index = device->definition_index;
			entry->role = (byte)role;
			if (device_group_entry(roles[role], entry))
				group_indices[count++] = roles[role];
		}
	}

	return count;
}

/* (a client) the group of its own a host's entry names, or NONE */
static short device_group_find(
	struct distributed_coop_device_group const *entry)
{
	struct device_datum *device;
	long object_index;
	real value;
	word flags;
	boolean runtime;
	short group_index;

	if (entry->group_index != NONE)
	{
		group_index = entry->group_index;
		return group_index >= 0 && group_index < global_scenario_get()->device_groups.count &&
			device_group_network_get(group_index, &value, &flags, &runtime) && !runtime ? group_index : NONE;
	}
	object_index = entry->name_index != NONE ? object_index_from_name_index(entry->name_index) : entry->object_index;
	device = object_index != NONE ? object_try_and_get_and_verify_type(object_index, _object_mask_device) : NULL;
	/* (an unnamed one is at the host's index; should its identifier differ
	here, the device at that index alone will do) */
	if (!device && entry->name_index == NONE && object_index != NONE)
	{
		struct object_iterator iterator;
		struct device_datum *candidate;

		object_iterator_new(&iterator, _object_mask_device, 0);
		while (!device && (candidate = object_iterator_next(&iterator)) != NULL)
		{
			if ((iterator.index & 0xFFFF) == (object_index & 0xFFFF))
				device = candidate;
		}
	}
	if (!device || device->definition_index != entry->definition_index ||
		(entry->name_index == NONE && device->object.name_index != NONE))
	{
		return NONE;
	}
	switch (entry->role)
	{
	case _device_group_role_power: group_index = device->device.power_group_index; break;
	case _device_group_role_position: group_index = device->device.position_group_index; break;
	default: return NONE;
	}

	return group_index >= 0 && group_index < MAXIMUM_DEVICE_GROUPS &&
		device_group_network_get(group_index, &value, &flags, &runtime) && runtime ? group_index : NONE;
}

/* (the host) its device groups that changed, and a few more round them all */
static void host_send_device_groups(
	void)
{
	static short group_indices[MAXIMUM_DEVICE_GROUPS];
	struct distributed_coop_device_group *entries = coop_device_groups_message.groups;
	short count = device_group_entries(entries, group_indices);
	short index, sent = 0;

	if (coop_devices.refresh_next >= count)
		coop_devices.refresh_next = 0;
	for (index = 0; index < count; index++)
	{
		struct distributed_coop_device_group *entry = &entries[index];
		short group_index = group_indices[index];
		short refresh = (short)((index - coop_devices.refresh_next + count) % count);

		/* (the map's groups as it loaded them are every machine's: none sent) */
		if (!coop_devices.noted ||
			entry->value != coop_devices.values[group_index] ||
			entry->flags != coop_devices.flags[group_index] ||
			entry->snaps != coop_devices.sent_snaps[group_index])
		{
			coop_devices.values[group_index] = entry->value;
			coop_devices.flags[group_index] = entry->flags;
			coop_devices.sent_snaps[group_index] = entry->snaps;
			coop_devices.sends[group_index] = coop_devices.noted ? DEVICE_GROUP_SENDS : 0;
		}
		entry->changed = coop_devices.sends[group_index] != 0;
		if (entry->changed)
			coop_devices.sends[group_index]--;
		else if (refresh >= DEVICE_GROUP_REFRESHES_PER_TICK)
			continue;
		entries[sent++] = *entry;
	}
	coop_devices.noted = TRUE;
	coop_devices.refresh_next = count ? (short)((coop_devices.refresh_next + DEVICE_GROUP_REFRESHES_PER_TICK) % count) : 0;
	if (sent)
	{
		distributed_send(&coop_device_groups_message, _distributed_message_coop_device_groups, sent,
			(word)(sizeof(coop_device_groups_message.header) + sent * sizeof(entries[0])), _distributed_to_clients);
	}
}

/* (the host) which named objects it has */
static void host_send_object_names(
	void)
{
	struct distributed_coop_object_names_message message;
	struct distributed_coop_object_names *names = &message.names;
	short name_index;

	csmemset(names, 0, sizeof(*names));
	names->structure_bsp_index = global_structure_bsp_index_get();
	names->name_count = (short)MIN(global_scenario_get()->object_names.count, MAXIMUM_OBJECT_NAMES_PER_SCENARIO);
	for (name_index = 0; name_index < names->name_count; name_index++)
	{
		if (object_index_from_name_index(name_index) != NONE)
			names->present[name_index / 8] |= (byte)(1 << (name_index % 8));
	}
	distributed_send(&message, _distributed_message_coop_object_names, 1, (word)sizeof(message), _distributed_to_clients);
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
	csmemset(&coop_devices, 0, sizeof(coop_devices));
	csmemset(&coop_object_names, 0, sizeof(coop_object_names));
}

/* whether this machine's devices are the host's (a co-op client's), which
devices.c then sets no value of itself */
boolean network_coop_devices_remote(
	void)
{
	return game_connection() == _game_connection_network_client && coop_game();
}

/* (the host) a device group's devices put straight at its value (devices.c) */
void network_coop_note_device_snap(
	short group_index)
{
	if (game_connection() == _game_connection_network_server && coop_game() &&
		group_index >= 0 && group_index < MAXIMUM_DEVICE_GROUPS)
	{
		coop_devices.snaps[group_index]++;
	}
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
	host_send_device_groups();
	if (game_time_get() % OBJECT_NAMES_INTERVAL_TICKS == 0)
		host_send_object_names();

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

word network_coop_device_group_entry_size(
	void)
{
	return sizeof(struct distributed_coop_device_group);
}

word network_coop_object_names_entry_size(
	void)
{
	return sizeof(struct distributed_coop_object_names);
}

/* (a client) the host's device groups' values, its own set to them */
void network_coop_handle_device_groups(
	void const *entries,
	short count)
{
	struct distributed_coop_device_group const *groups = entries;
	short index;

	if (!coop_game())
		return;
	for (index = 0; index < count; index++)
	{
		struct distributed_coop_device_group const *entry = &groups[index];
		short group_index = device_group_find(entry);
		real value, here;
		word flags;
		boolean runtime, snap;

		if (group_index == NONE || !device_group_network_get(group_index, &here, &flags, &runtime))
			continue;
		value = PIN(entry->value, 0.0f, 1.0f);
		/* (put straight there as the host's were, or where a group sent round
		them all is not as the host has it: a client that missed its change,
		or has just joined; a change moves there) */
		snap = (coop_devices.seen[group_index] && entry->snaps != coop_devices.snaps[group_index]) ||
			(!entry->changed && value != here);
		if (snap || value != here || entry->flags != (byte)flags)
			device_group_network_set(group_index, value, entry->flags, snap);
		coop_devices.snaps[group_index] = entry->snaps;
		coop_devices.seen[group_index] = TRUE;
	}
}

/* (a client) the host's named objects: each it has that the host had not,
and lacks that the host had, the last two times on the same structure BSP,
deleted or created (scenery and devices: network_objects.c has the rest) */
void network_coop_handle_object_names(
	void const *entries)
{
	struct distributed_coop_object_names const *names = entries;
	short name_index;

	if (!coop_game())
		return;
	if (names->structure_bsp_index != global_structure_bsp_index_get() ||
		names->name_count != MIN(global_scenario_get()->object_names.count, MAXIMUM_OBJECT_NAMES_PER_SCENARIO))
	{
		csmemset(&coop_object_names, 0, sizeof(coop_object_names));
		return;
	}
	for (name_index = 0; name_index < names->name_count; name_index++)
	{
		byte bit = (byte)(1 << (name_index % 8));
		boolean there = TEST_FLAG(names->present[name_index / 8], name_index % 8);
		long object_index = object_index_from_name_index(name_index);
		short type = object_name_type(name_index);

		if (type < 0 || type >= NUMBER_OF_OBJECT_TYPES || TEST_FLAG(NETWORKED_OBJECT_TYPES, type) ||
			there == (object_index != NONE))
		{
			coop_object_names.differing[name_index / 8] &= (byte)~bit;
			continue;
		}
		if (!(coop_object_names.differing[name_index / 8] & bit))
		{
			coop_object_names.differing[name_index / 8] |= bit;
			continue;
		}
		coop_object_names.differing[name_index / 8] &= (byte)~bit;
		if (there)
			object_new_by_name(name_index);
		else if (!object_holds_player(object_index))
			object_delete(object_index);
	}
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
