/*
NETWORK_COOP.C

Campaign co-op over the network (port/linux/NETCODE.md).

A network game on a campaign map, with no game engine running, is co-op.
Only the host runs the map's scripts (game.c), so anything the scripts do
that the clients need to see is sent from here:

- Presentation, every tick: whether a cinematic is playing, the letterbox,
  the camera, and the screen fade. A client starts and stops its own
  cinematic to match, and looks through the host's camera meanwhile. If the
  host goes quiet for two seconds, the client ends the cinematic so its
  players aren't stuck.
- Script sounds (dialogue, music, ambience; hooked in game_sound.c). Each is
  sent in three ticks' messages in case one is lost, and numbered so a
  client plays each exactly once.
- Devices (doors, elevators, switches, lights). A device moves toward its
  device group's value (devices.c). On a client nothing sets those values
  but this file: doors a player walks up to and switches a player uses are
  decided by the host (the player's action is relayed to it). The host
  sends a group's value for three ticks when it changes, plus a few groups
  every tick in rotation, so a client that lost a message or joined late
  catches up. A scenario group is identified by its index, which is the
  same on every machine. A device's own group is identified by the device:
  its name, or its object index and tag.
- Named objects (scenery and devices) the scripts create or destroy.
  network_objects.c already handles units, vehicles, weapons and equipment.
  Twice a second the host sends which object names currently exist. A
  client that sees the same difference twice in a row, on the same BSP,
  creates or deletes its copy.
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
	/* a client ends the cinematic after this long without hearing from the host */
	PRESENTATION_SILENCE_TICKS = 2 * TICKS_PER_SECOND,
	/* the field of view (radians) is sent as a word, scaled by this */
	FIELD_OF_VIEW_SCALE = 10000,

	MAXIMUM_QUEUED_SOUNDS = 32,
	/* each script sound is sent in this many ticks' messages */
	SOUND_SENDS = 3,

	/* the size of devices.c's device group array */
	MAXIMUM_DEVICE_GROUPS = 1024,
	/* a changed group is sent in this many ticks' messages */
	DEVICE_GROUP_SENDS = 3,
	/* unchanged groups sent each tick, in rotation */
	DEVICE_GROUP_REFRESHES_PER_TICK = 8,

	OBJECT_NAMES_INTERVAL_TICKS = TICKS_PER_SECOND / 2,
	OBJECT_NAME_BYTES = MAXIMUM_OBJECT_NAMES_PER_SCENARIO / 8,
	/* the types network_objects.c syncs; the object names sync skips them */
	NETWORKED_OBJECT_TYPES = _object_mask_biped | _object_mask_vehicle | _object_mask_weapon | _object_mask_equipment,
};

/* which of a device's two groups an entry is */
enum
{
	_device_group_role_power,
	_device_group_role_position,
};

/* distributed_coop_presentation.flags */
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
	/* ticks since the fade began, capped at SHORT_MAX */
	short fade_elapsed;
	/* the host's game time when the fade began; a new value means a new fade */
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

struct distributed_coop_sound
{
	/* _coop_sound_impulse, _looping_start or _looping_stop */
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

/* A device group's state. group_index is set for a scenario group. For a
device's own group it is NONE, and the device is found by name_index, or
(unnamed) by object_index and definition_index. */
struct distributed_coop_device_group
{
	short group_index;
	short name_index;
	long object_index;
	long definition_index;
	byte role;
	byte flags;
	/* counts the host's immediate sets (wraps at 256) */
	byte snaps;
	/* sent because it changed, rather than in the rotation */
	byte changed;
	real value;
};

struct distributed_coop_device_groups_message
{
	struct distributed_message_header header;
	struct distributed_coop_device_group groups[MAXIMUM_DEVICE_GROUPS];
};

/* one bit per scenario object name: whether the host has that object */
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

/* client: the cinematic it started to match the host's */
static struct
{
	boolean cinematic_started;
	long heard_time;
	long fade_start_time;
} coop_presentation;

/* host: sounds still to be sent; client: the last one played */
static struct
{
	struct distributed_coop_sound sounds[MAXIMUM_QUEUED_SOUNDS];
	short sends[MAXIMUM_QUEUED_SOUNDS];
	short count;
	word next_number;
	word played_number;
	boolean played_any;
} coop_sounds;

/* host: each device group as last sent, indexed by group */
static struct
{
	/* false until the first tick, which records the map's starting state */
	boolean started;
	real values[MAXIMUM_DEVICE_GROUPS];
	byte flags[MAXIMUM_DEVICE_GROUPS];
	byte snaps[MAXIMUM_DEVICE_GROUPS];
	/* ticks this group is still to be sent as a change */
	byte sends[MAXIMUM_DEVICE_GROUPS];
	/* immediate sets so far (devices.c calls network_coop_note_device_snap) */
	byte snap_counts[MAXIMUM_DEVICE_GROUPS];
	short refresh_next;
	struct distributed_coop_device_groups_message message;
} host_devices;

/* client: the host's snap count last seen for each of its groups */
static struct
{
	byte snaps[MAXIMUM_DEVICE_GROUPS];
	boolean seen[MAXIMUM_DEVICE_GROUPS];
} client_devices;

/* client: the object names that differed from the host's last time */
static byte client_names_differing[OBJECT_NAME_BYTES];

/* ---------- private code */

static boolean coop_game(
	void)
{
	return !game_engine_running();
}

/* whether a tag index from the host really is a tag of that group */
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

/* whether the object is a player's unit or carries one (scripts never delete those) */
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

/* the object type a scenario object name refers to, or NONE */
static short object_name_type(
	short name_index)
{
	struct scenario *scenario = global_scenario_get();

	if (name_index < 0 || name_index >= scenario->object_names.count)
		return NONE;

	return TAG_BLOCK_GET_ELEMENT(&scenario->object_names, name_index, struct scenario_object_name)->runtime_object_type;
}

/* Fills in the group's value, flags and snap count. Returns FALSE if the
group doesn't exist. */
static boolean device_group_state(
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
	entry->snaps = host_devices.snap_counts[group_index];
	entry->changed = FALSE;

	return TRUE;
}

/* Host: lists every device group, scenario groups first, then each device's
own groups. group_indices gets each entry's group index. Returns the count. */
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
		if (device_group_state(group_index, entry))
			group_indices[count++] = group_index;
	}
	object_iterator_new(&iterator, _object_mask_device, 0);
	while ((device = object_iterator_next(&iterator)) != NULL)
	{
		short groups[2];
		short role;

		groups[_device_group_role_power] = device->device.power_group_index;
		groups[_device_group_role_position] = device->device.position_group_index;
		for (role = 0; role < NUMBEROF(groups) && count < MAXIMUM_DEVICE_GROUPS; role++)
		{
			struct distributed_coop_device_group *entry = &entries[count];
			real value;
			word flags;
			boolean runtime;

			/* scenario groups were listed above */
			if (!device_group_network_get(groups[role], &value, &flags, &runtime) || !runtime)
				continue;
			entry->group_index = NONE;
			entry->name_index = device->object.name_index;
			entry->object_index = iterator.index;
			entry->definition_index = device->definition_index;
			entry->role = (byte)role;
			if (device_group_state(groups[role], entry))
				group_indices[count++] = groups[role];
		}
	}

	return count;
}

/* Client: finds the local device that a host entry names. An unnamed device
sits at the same object index on every machine; if its salt differs here,
matching the absolute index is enough (the tag check below still applies). */
static struct device_datum *device_find(
	struct distributed_coop_device_group const *entry)
{
	struct device_datum *device;
	long object_index;

	object_index = entry->name_index != NONE ? object_index_from_name_index(entry->name_index) : entry->object_index;
	if (object_index == NONE)
		return NULL;
	device = object_try_and_get_and_verify_type(object_index, _object_mask_device);
	if (!device && entry->name_index == NONE)
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
		return NULL;
	}

	return device;
}

/* client: the local group a host entry refers to, or NONE */
static short device_group_find(
	struct distributed_coop_device_group const *entry)
{
	struct device_datum *device;
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
	device = device_find(entry);
	if (!device)
		return NONE;
	switch (entry->role)
	{
	case _device_group_role_power: group_index = device->device.power_group_index; break;
	case _device_group_role_position: group_index = device->device.position_group_index; break;
	default: return NONE;
	}

	return group_index >= 0 && group_index < MAXIMUM_DEVICE_GROUPS &&
		device_group_network_get(group_index, &value, &flags, &runtime) && runtime ? group_index : NONE;
}

/* host: sends the groups that changed, plus the next few in the rotation */
static void host_send_device_groups(
	void)
{
	static short group_indices[MAXIMUM_DEVICE_GROUPS];
	struct distributed_coop_device_group *entries = host_devices.message.groups;
	short count = device_group_entries(entries, group_indices);
	short index, sent = 0;

	if (host_devices.refresh_next >= count)
		host_devices.refresh_next = 0;
	for (index = 0; index < count; index++)
	{
		struct distributed_coop_device_group *entry = &entries[index];
		short group_index = group_indices[index];
		short rotation = (short)((index - host_devices.refresh_next + count) % count);

		if (!host_devices.started ||
			entry->value != host_devices.values[group_index] ||
			entry->flags != host_devices.flags[group_index] ||
			entry->snaps != host_devices.snaps[group_index])
		{
			host_devices.values[group_index] = entry->value;
			host_devices.flags[group_index] = entry->flags;
			host_devices.snaps[group_index] = entry->snaps;
			/* clients loaded the same map, so the starting state isn't sent */
			host_devices.sends[group_index] = host_devices.started ? DEVICE_GROUP_SENDS : 0;
		}
		entry->changed = host_devices.sends[group_index] != 0;
		if (entry->changed)
			host_devices.sends[group_index]--;
		else if (rotation >= DEVICE_GROUP_REFRESHES_PER_TICK)
			continue;
		entries[sent++] = *entry;
	}
	host_devices.started = TRUE;
	host_devices.refresh_next = count ? (short)((host_devices.refresh_next + DEVICE_GROUP_REFRESHES_PER_TICK) % count) : 0;
	if (sent)
	{
		distributed_send(&host_devices.message, _distributed_message_coop_device_groups, sent,
			(word)(sizeof(host_devices.message.header) + sent * sizeof(entries[0])), _distributed_to_clients);
	}
}

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

static void host_send_sounds(
	void)
{
	struct distributed_coop_sounds_message message;
	short index;

	if (!coop_sounds.count)
		return;
	csmemcpy(message.sounds, coop_sounds.sounds, coop_sounds.count * sizeof(message.sounds[0]));
	distributed_send(&message, _distributed_message_coop_sounds, coop_sounds.count,
		(word)(sizeof(message.header) + coop_sounds.count * sizeof(message.sounds[0])), _distributed_to_clients);
	/* drop the ones sent SOUND_SENDS times, moving the last into the gap */
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
	csmemset(&host_devices, 0, sizeof(host_devices));
	csmemset(&client_devices, 0, sizeof(client_devices));
	csmemset(client_names_differing, 0, sizeof(client_names_differing));
}

/* Whether this machine is a co-op client, whose devices only the host
moves. devices.c and players.c check this. */
boolean network_coop_devices_remote(
	void)
{
	return game_connection() == _game_connection_network_client && coop_game();
}

/* host: devices.c calls this when a group's devices are set immediately */
void network_coop_note_device_snap(
	short group_index)
{
	if (game_connection() == _game_connection_network_server && coop_game() &&
		group_index >= 0 && group_index < MAXIMUM_DEVICE_GROUPS)
	{
		host_devices.snap_counts[group_index]++;
	}
}

/* host: game_sound.c calls this for each sound the scripts play */
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

/* host, after each tick */
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
	host_send_sounds();
}

/* client, after each tick */
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
		/* Jump straight to the value when the host did, or when a rotation
		entry disagrees with us (we missed the change, or joined late).
		A change entry animates, as it did on the host. */
		snap = (client_devices.seen[group_index] && entry->snaps != client_devices.snaps[group_index]) ||
			(!entry->changed && value != here);
		if (snap || value != here || entry->flags != (byte)flags)
			device_group_network_set(group_index, value, entry->flags, snap);
		client_devices.snaps[group_index] = entry->snaps;
		client_devices.seen[group_index] = TRUE;
	}
}

/* Client: creates or deletes named scenery and devices to match the host.
A difference must show up twice in a row, on the same BSP, so a message
that crosses a BSP switch or a script's create can't cause a false one. */
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
		csmemset(client_names_differing, 0, sizeof(client_names_differing));
		return;
	}
	for (name_index = 0; name_index < names->name_count; name_index++)
	{
		byte *differing = &client_names_differing[name_index / 8];
		byte bit = (byte)(1 << (name_index % 8));
		boolean host_has = (names->present[name_index / 8] & bit) != 0;
		long object_index = object_index_from_name_index(name_index);
		short type = object_name_type(name_index);

		if (type < 0 || type >= NUMBER_OF_OBJECT_TYPES || TEST_FLAG(NETWORKED_OBJECT_TYPES, type) ||
			host_has == (object_index != NONE))
		{
			*differing &= (byte)~bit;
			continue;
		}
		if (!(*differing & bit))
		{
			*differing |= bit;
			continue;
		}
		*differing &= (byte)~bit;
		if (host_has)
			object_new_by_name(name_index);
		else if (!object_holds_player(object_index))
			object_delete(object_index);
	}
}

/* client: plays each script sound once, in order */
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

		/* already played (the comparison handles the number wrapping) */
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

void network_coop_handle_presentation(
	void const *entries)
{
	struct distributed_coop_presentation const *presentation = entries;
	boolean cinematic = TEST_FLAG(presentation->flags, _presentation_cinematic_bit);

	if (!coop_game())
		return;
	coop_presentation.heard_time = game_time_get();

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

	/* start each new fade, backdated to when the host started it */
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
