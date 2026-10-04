/*
NETWORK_COOP.C

Campaign co-op over the network (port/linux/NETCODE.md).

A network game on a campaign map, with no game engine running, is co-op.
Only the host runs the map's scripts (game.c), so anything the scripts do
that the clients need to see is sent from here:

- Presentation, every tick: whether a cinematic is playing, the letterbox,
  the camera, the screen fade, the HUD settings the scripts control (what
  is shown, the mission timer), and the cutscene skip vote. A client starts
  and stops its own cinematic to match, and looks through the host's camera
  meanwhile. If the host goes quiet for two seconds, the client ends the
  cinematic so its players aren't stuck.
- Events, which happen once: script sounds (dialogue, music, ambience),
  chapter titles, help and objective text, "Checkpoint" messages, screen
  shake, nav points, and custom animations on units and scenery. Each is
  sent in three ticks' messages in case one is lost, and numbered so a
  client applies each exactly once, in order. The engine calls the
  network_coop_note_ functions (network_coop.h) to queue them.
- Devices (doors, elevators, switches, lights). A device moves toward its
  device group's value (devices.c). On a client nothing sets those values
  but this file: doors a player walks up to and switches a player uses are
  decided by the host (the player's action is relayed to it). The host
  sends a group's value for three ticks when it changes, plus a few groups
  every tick in rotation, so a client that lost a message or joined late
  catches up. A scenario group is identified by its index, which is the
  same on every machine. A device's own group is identified by the device.
- Named objects (scenery and devices) the scripts create or destroy.
  network_objects.c already handles units, vehicles, weapons and equipment.
  Twice a second the host sends which object names currently exist. A
  client that sees the same difference twice in a row, on the same BSP,
  creates or deletes its copy.

Skipping a cutscene is a vote. Pressing the skip key during a skippable
cutscene votes (a client sends its vote to the host every tick), and the
host skips once more than half the machines have voted. The skip itself is
the campaign's: the host reverts to the state saved when the cutscene began
and the script carries on past it. Clients catch up through the object,
device and name syncs. The host keeps its clock running through the revert,
because the netcode needs it to only go forward (network_coop_skip_reverted).

Objects (scenery and devices) that aren't synced by network_objects.c are
found on a client by their scenario name, or, unnamed, by their object
index and tag, since the map placed them at the same index everywhere.
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "camera/camera_scripting.h"
#include "camera/director.h"
#include "camera/observer.h"
#include "cutscene/cinematics.h"
#include "devices/devices.h"
#include "effects/effect_definitions.h"
#include "effects/player_effects.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "game/player_queues_new.h"
#include "hs/hs.h"
#include "hs/hs_library_external.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"
#include "interface/hud_unit.h"
#include "interface/hud_weapon.h"
#include "main/main.h"
#include "models/model_animation_definitions.h"
#include "models/model_definitions.h"
#include "models/models.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "objects/scenery.h"
#include "saved games/game_state.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "sound/game_sound.h"
#include "sound/sound_definitions.h"
#include "units/units.h"
#include "coop_spectate.h"
#include "network_coop.h"
#include "network_distributed.h"

/* ---------- constants */

enum
{
	/* a client ends the cinematic after this long without hearing from the host */
	PRESENTATION_SILENCE_TICKS = 2 * TICKS_PER_SECOND,
	/* the field of view (radians) is sent as a word, scaled by this */
	FIELD_OF_VIEW_SCALE = 10000,

	MAXIMUM_QUEUED_EVENTS = 128,
	/* each event is sent in this many ticks' messages */
	EVENT_SENDS = 3,

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

	/* the host forgets a client's skip vote it hasn't heard again for this long */
	SKIP_VOTE_HELD_TICKS = TICKS_PER_SECOND,
	/* After a skip the cutscene stays skippable until the script reaches
	cinematic_skip_stop; votes still in flight must not skip it again. */
	SKIP_COOLDOWN_TICKS = 2 * TICKS_PER_SECOND,
};

/* distributed_coop_event.kind */
enum
{
	_coop_event_sound,
	_coop_event_title,
	_coop_event_hud,
	_coop_event_player_effect,
	_coop_event_nav_point,
	_coop_event_unit_animation,
	_coop_event_scenery_animation,
	_coop_event_effect,
};

/* distributed_coop_event.type of an effect: at a cutscene flag (value), or on
an object's marker (value: its index in the object's model's markers) */
enum
{
	_coop_effect_at_flag,
	_coop_effect_on_marker,
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
	_presentation_skippable_bit,
	_presentation_show_hud_bit,
	_presentation_show_help_text_bit,
	_presentation_timer_paused_bit,
	_presentation_timer_enabled_bit,
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
	/* the skip vote: machines that voted, out of how many */
	byte skip_votes;
	byte skip_voters;
	/* the scripts' HUD settings (hud_unit.c, hud_weapon.c) */
	long unit_hud_flags;
	long weapon_hud_flags;
	/* the script timer (hud_messaging.c); the clocks agree, so its
	reference time means the same on every machine */
	long timer_reference_time;
	short timer_ticks;
	short timer_flash_cutoff;
	short timer_x;
	short timer_y;
	short timer_corner;
	word pad;
};

struct distributed_coop_presentation_message
{
	struct distributed_message_header header;
	struct distributed_coop_presentation presentation;
};

/* Something that happens once. Which fields mean what depends on the kind:

	sound             type: _coop_sound_ kind; tag_index; object_index (the source, or NONE); reals[0]: scale
	title             value: title index; reals[0]: delay in seconds
	hud               type: _coop_hud_ kind; value
	player_effect     type: _coop_player_effect_ kind; reals: the function's arguments
	nav_point         type: _coop_nav_point_ kind; value: nav index (NONE to deactivate);
	                  target: team or unit; object_index: cutscene flag or object; reals[0]: offset
	unit_animation    object_index: unit; tag_index: graph (NONE: stop); value: animation;
	                  frame; interpolate
	scenery_animation object_index, name_index, definition_index: the scenery; tag_index: graph;
	                  value: animation; frame */
struct distributed_coop_event
{
	byte kind;
	byte type;
	word number;
	short value;
	short frame;
	short name_index;
	byte interpolate;
	byte pad;
	long object_index;
	long target;
	long tag_index;
	long definition_index;
	real reals[3];
};

struct distributed_coop_events_message
{
	struct distributed_message_header header;
	struct distributed_coop_event events[MAXIMUM_QUEUED_EVENTS];
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

/* a client's vote to skip the cinematic, sent every tick while it stands */
struct distributed_coop_skip_vote
{
	byte voted;
	byte pad[3];
};

struct distributed_coop_skip_vote_message
{
	struct distributed_message_header header;
	struct distributed_coop_skip_vote vote;
};

/* ---------- globals */

/* client: the cinematic it started to match the host's */
static struct
{
	boolean cinematic_started;
	/* showing the host's camera to a player with nothing else to look at */
	boolean following_host;
	long heard_time;
	long fade_start_time;
} coop_presentation;

/* host: events still to be sent; client: the last one applied */
static struct
{
	struct distributed_coop_event events[MAXIMUM_QUEUED_EVENTS];
	short sends[MAXIMUM_QUEUED_EVENTS];
	short count;
	word next_number;
	word applied_number;
	boolean applied_any;
} coop_events;

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

/* The cutscene skip vote. The host keeps when it last heard each client's
vote; a client keeps the host's last tally. */
static struct
{
	boolean voted;
	boolean offered;
	short votes;
	short voters;
	long client_vote_times[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	/* host: no voting before this game time (set after a skip) */
	long cooldown_until;
	/* host: the vote passed and main_skip_cinematic was called once */
	boolean requested;
	/* host: the save the script makes as the cutscene becomes skippable is
	written (a skip reverts to it) */
	boolean skip_save_written;
} skip_vote;

/* ---------- private code */

static boolean coop_game(
	void)
{
	return network_coop_active();
}

static boolean coop_host(
	void)
{
	return game_connection() == _game_connection_network_server && coop_game();
}

static boolean coop_client(
	void)
{
	return game_connection() == _game_connection_network_client && coop_game();
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

/* Client: finds the local copy of an object the host named, of one of the
types in type_mask, and returns its index (or NONE). An unnamed object sits
at the same object index on every machine; if its salt differs here,
matching the absolute index is enough, as long as the tag matches too. */
static long object_find(
	short name_index,
	long object_index,
	long definition_index,
	unsigned long type_mask)
{
	struct object_datum *object;

	if (name_index != NONE)
		object_index = object_index_from_name_index(name_index);
	if (object_index == NONE)
		return NONE;
	object = object_try_and_get_and_verify_type(object_index, type_mask);
	if (!object && name_index == NONE)
	{
		struct object_iterator iterator;
		struct object_datum *candidate;

		object_iterator_new(&iterator, type_mask, 0);
		while (!object && (candidate = object_iterator_next(&iterator)) != NULL)
		{
			if ((iterator.index & 0xFFFF) == (object_index & 0xFFFF))
			{
				object = candidate;
				object_index = iterator.index;
			}
		}
	}
	if (!object || object->definition_index != definition_index ||
		(name_index == NONE && object->object.name_index != NONE))
	{
		return NONE;
	}

	return object_index;
}

/* host: a new event, filled in by the caller; NULL if it isn't wanted or the queue is full */
static struct distributed_coop_event *event_new(
	byte kind)
{
	struct distributed_coop_event *event;

	if (!coop_host())
		return NULL;
	if (coop_events.count == MAXIMUM_QUEUED_EVENTS)
	{
		error(_error_silent, "co-op: event queue full; an event of kind %d is not sent", kind);
		return NULL;
	}
	event = &coop_events.events[coop_events.count];
	csmemset(event, 0, sizeof(*event));
	event->kind = kind;
	event->number = ++coop_events.next_number;
	event->object_index = NONE;
	event->target = NONE;
	event->tag_index = NONE;
	event->definition_index = NONE;
	event->name_index = NONE;
	coop_events.sends[coop_events.count] = 0;
	coop_events.count++;

	return event;
}

static void host_send_events(
	void)
{
	struct distributed_coop_events_message message;
	short index;

	if (!coop_events.count)
		return;
	csmemcpy(message.events, coop_events.events, coop_events.count * sizeof(message.events[0]));
	distributed_send(&message, _distributed_message_coop_events, coop_events.count,
		(word)(sizeof(message.header) + coop_events.count * sizeof(message.events[0])), _distributed_to_clients);
	/* drop the ones sent EVENT_SENDS times, keeping the rest in order */
	for (index = 0; index < coop_events.count; )
	{
		if (++coop_events.sends[index] < EVENT_SENDS)
		{
			index++;
			continue;
		}
		coop_events.count--;
		csmemmove(&coop_events.events[index], &coop_events.events[index + 1],
			(coop_events.count - index) * sizeof(coop_events.events[0]));
		csmemmove(&coop_events.sends[index], &coop_events.sends[index + 1],
			(coop_events.count - index) * sizeof(coop_events.sends[0]));
	}
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

/* client: the local group a host entry refers to, or NONE */
static short device_group_find(
	struct distributed_coop_device_group const *entry)
{
	struct device_datum *device;
	long device_index;
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
	device_index = object_find(entry->name_index, entry->object_index, entry->definition_index, _object_mask_device);
	if (device_index == NONE)
		return NONE;
	device = object_get_and_verify_type(device_index, _object_mask_device);
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

static void skip_vote_clear(
	void)
{
	short index;

	skip_vote.voted = FALSE;
	skip_vote.votes = 0;
	for (index = 0; index < NUMBEROF(skip_vote.client_vote_times); index++)
		skip_vote.client_vote_times[index] = NONE;
}

/* host: counts the votes to skip the cinematic, and skips when more than
half the machines have voted */
static void host_count_skip_votes(
	void)
{
	long machine_indices[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	short machine_count = distributed_client_machines(machine_indices, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
	long now = game_time_get();
	boolean skippable = cinematic_in_progress() && cinematic_can_be_skipped();
	boolean was_offered = skip_vote.offered;
	short index;

	/* Not until the save the script makes as the cutscene becomes skippable
	is written: reverting before then would go back to the save before it.
	A save asked for later in the cutscene doesn't hold the vote up (the
	skip cancels it, as in a solo game). */
	if (!skippable)
		skip_vote.skip_save_written = FALSE;
	else if (!main_saving_map())
		skip_vote.skip_save_written = TRUE;
	skip_vote.offered = skippable && skip_vote.skip_save_written && now >= skip_vote.cooldown_until;
	if (skip_vote.offered && !was_offered)
		error(_error_silent, "co-op: the cutscene can be skipped");
	if (!skip_vote.offered)
	{
		skip_vote_clear();
		skip_vote.voters = 0;
		skip_vote.requested = FALSE;
		return;
	}
	/* asked for already: main.c skips at the end of this frame */
	if (skip_vote.requested)
		return;
	/* the host votes too, unless it is a dedicated server with no player */
	skip_vote.voters = (short)(machine_count + (local_player_get_next(NONE) != NONE ? 1 : 0));
	skip_vote.votes = skip_vote.voted ? 1 : 0;
	for (index = 0; index < machine_count; index++)
	{
		long vote_time = skip_vote.client_vote_times[machine_indices[index]];

		if (vote_time != NONE && now - vote_time <= SKIP_VOTE_HELD_TICKS)
			skip_vote.votes++;
	}
	if (skip_vote.votes * 2 > skip_vote.voters)
	{
		error(_error_silent, "co-op: skipping the cutscene (%d of %d voted)", skip_vote.votes, skip_vote.voters);
		skip_vote.requested = TRUE;
		main_skip_cinematic();
	}
}

/* host: the presentation sent to every client this tick */
static void host_presentation(
	struct distributed_coop_presentation *presentation)
{
	struct observer_result const *camera = observer_get_camera(0);
	struct hud_timer_state timer;
	real_rgb_color fade_color;
	boolean fading_out;
	long elapsed;

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

	SET_FLAG(presentation->flags, _presentation_skippable_bit, skip_vote.offered);
	presentation->skip_votes = (byte)MIN(skip_vote.votes, 255);
	presentation->skip_voters = (byte)MIN(skip_vote.voters, 255);

	SET_FLAG(presentation->flags, _presentation_show_hud_bit, hud_scripted_globals->show_hud);
	SET_FLAG(presentation->flags, _presentation_show_help_text_bit, hud_scripted_globals->show_hud_help_text);
	presentation->unit_hud_flags = hud_unit_port_script_flags();
	presentation->weapon_hud_flags = hud_weapon_port_script_flags();
	hud_messaging_port_timer_get(&timer);
	SET_FLAG(presentation->flags, _presentation_timer_paused_bit, timer.paused);
	SET_FLAG(presentation->flags, _presentation_timer_enabled_bit, timer.enabled);
	presentation->timer_reference_time = timer.reference_time;
	presentation->timer_ticks = timer.ticks;
	presentation->timer_flash_cutoff = timer.flash_cutoff;
	presentation->timer_x = timer.x;
	presentation->timer_y = timer.y;
	presentation->timer_corner = timer.corner;
}

static void client_cinematic_end(
	void)
{
	if (!coop_presentation.cinematic_started)
		return;
	coop_presentation.cinematic_started = FALSE;
	director_script_camera(FALSE);
	cinematic_stop();
}

static void client_apply_hud(
	struct distributed_coop_event const *event)
{
	switch (event->type)
	{
	case _coop_hud_help_text:
		if (event->value >= 0 && event->value < hud_messaging_port_message_count())
			scripted_hud_set_state_message(event->value);
		break;
	case _coop_hud_objective:
		if (event->value >= 0 && event->value < hud_messaging_port_message_count())
			scripted_hud_set_objective(event->value);
		break;
	case _coop_hud_help_flash:
		scripted_hud_set_flashing_state(event->value != 0);
		break;
	case _coop_hud_messages_clear:
		scripted_hud_messages_clear();
		break;
	case _coop_hud_checkpoint:
		hud_autosave(event->value != 0);
		break;
	default:
		break;
	}
}

static void client_apply_player_effect(
	struct distributed_coop_event const *event)
{
	real const *reals = event->reals;

	switch (event->type)
	{
	case _coop_player_effect_translation:
		scripted_player_effect_set_translation(reals[0], reals[1], reals[2]);
		break;
	case _coop_player_effect_rotation:
		scripted_player_effect_set_rotation(reals[0], reals[1], reals[2]);
		break;
	case _coop_player_effect_start:
		scripted_player_effect_start(PIN(reals[0], 0.0f, 1.0f), PIN(reals[1], 0.0f, 60.0f));
		break;
	case _coop_player_effect_stop:
		scripted_player_effect_stop(PIN(reals[0], 0.0f, 60.0f));
		break;
	default:
		break;
	}
}

static void client_apply_nav_point(
	struct distributed_coop_event const *event)
{
	struct scenario *scenario = global_scenario_get();
	boolean activate = event->value != NONE;
	boolean flag = event->type == _coop_nav_point_team_flag || event->type == _coop_nav_point_unit_flag;
	boolean team = event->type == _coop_nav_point_team_flag || event->type == _coop_nav_point_team_object;
	real offset = event->reals[0];

	if (activate && (event->value < 0 || event->value >= hud_globals->waypoint.arrows.count))
		return;
	if (flag && (event->object_index < 0 || event->object_index >= scenario->cutscene_flags.count))
		return;
	if (team && (event->target < 0 || event->target >= NUMBER_OF_SOLO_CAMPAIGN_TEAMS))
		return;
	if (!team && !object_try_and_get_and_verify_type(event->target, _object_mask_unit))
		return;
	switch (event->type)
	{
	case _coop_nav_point_team_flag:
		if (activate)
			hud_activate_team_nav_point_with_flag(event->value, (short)event->target, (short)event->object_index, offset);
		else
			hud_deactivate_team_nav_point_with_flag((short)event->target, (short)event->object_index);
		break;
	case _coop_nav_point_team_object:
		if (activate)
			hud_activate_team_nav_point_with_object(event->value, (short)event->target, event->object_index, offset);
		else
			hud_deactivate_team_nav_point_with_object((short)event->target, event->object_index);
		break;
	case _coop_nav_point_unit_flag:
		if (activate)
			hud_unit_activate_nav_point_with_flag(event->value, event->target, (short)event->object_index, offset);
		else
			hud_unit_deactivate_nav_point_with_flag(event->target, (short)event->object_index);
		break;
	case _coop_nav_point_unit_object:
		if (activate)
			hud_unit_activate_nav_point_with_object(event->value, event->target, event->object_index, offset);
		else
			hud_unit_deactivate_nav_point_with_object(event->target, event->object_index);
		break;
	default:
		break;
	}
}

static void client_apply_unit_animation(
	struct distributed_coop_event const *event)
{
	if (!distributed_object_index_valid(event->object_index) || !network_objects_client_has(event->object_index) ||
		!object_try_and_get_and_verify_type(event->object_index, _object_mask_unit))
	{
		return;
	}
	if (event->tag_index == NONE)
		unit_stop_custom_animation(event->object_index);
	else if (distributed_graph_animation(event->tag_index, event->value))
		unit_port_play_user_animation(event->object_index, event->tag_index, event->value, event->interpolate, event->frame);
}

static void client_apply_scenery_animation(
	struct distributed_coop_event const *event)
{
	long scenery_index = object_find(event->name_index, event->object_index, event->definition_index,
		_object_mask_scenery);
	struct animation *animation = distributed_graph_animation(event->tag_index, event->value);

	/* scenery animations have no random permutations, so the name finds the same one */
	if (scenery_index != NONE && animation)
		scenery_animation_start_at_frame(scenery_index, event->tag_index, animation->name, event->frame);
}

static void client_apply_sound(
	struct distributed_coop_event const *event)
{
	long object_index = event->object_index != NONE && network_objects_client_has(event->object_index) ?
		event->object_index : NONE;
	real scale = PIN(event->reals[0], 0.0f, 1.0f);

	switch (event->type)
	{
	case _coop_sound_impulse:
		if (distributed_tag_of_group(event->tag_index, SOUND_DEFINITION_TAG))
			scripted_sound_new(event->tag_index, object_index, scale);
		break;
	case _coop_sound_looping_start:
		if (distributed_tag_of_group(event->tag_index, LOOPING_SOUND_DEFINITION_TAG))
			scripted_looping_sound_start(event->tag_index, object_index, scale);
		break;
	case _coop_sound_looping_stop:
		if (distributed_tag_of_group(event->tag_index, LOOPING_SOUND_DEFINITION_TAG))
			scripted_looping_sound_stop(event->tag_index);
		break;
	default:
		break;
	}
}

static void client_apply_effect(
	struct distributed_coop_event const *event)
{
	long object_index;
	struct object_datum *object;
	struct model *model;

	if (!distributed_tag_of_group(event->tag_index, EFFECT_DEFINITION_TAG))
		return;
	if (event->type == _coop_effect_at_flag)
	{
		if (event->value >= 0 && event->value < global_scenario_get()->cutscene_flags.count)
			hs_effect_new(event->tag_index, event->value);
		return;
	}
	object_index = object_find(event->name_index, event->object_index, event->definition_index, _object_mask_all);
	object = object_index != NONE ? object_get(object_index) : NULL;
	model = object && object_definition_get(object->definition_index)->object.model.index != NONE ?
		model_definition_get(object_definition_get(object->definition_index)->object.model.index) : NULL;
	if (model && event->value >= 0 && event->value < model->markers.count)
	{
		hs_effect_new_from_object_marker(event->tag_index, object_index,
			TAG_BLOCK_GET_ELEMENT(&model->markers, event->value, struct model_marker)->name);
	}
}

static void client_apply_event(
	struct distributed_coop_event const *event)
{
	switch (event->kind)
	{
	case _coop_event_sound:
		client_apply_sound(event);
		break;
	case _coop_event_title:
		if (event->value >= 0 && event->value < global_scenario_get()->cutscene_chapter_titles.count)
			cinematic_set_title_delayed(event->value, PIN(event->reals[0], 0.0f, 60.0f));
		break;
	case _coop_event_hud:
		client_apply_hud(event);
		break;
	case _coop_event_player_effect:
		client_apply_player_effect(event);
		break;
	case _coop_event_nav_point:
		client_apply_nav_point(event);
		break;
	case _coop_event_unit_animation:
		client_apply_unit_animation(event);
		break;
	case _coop_event_scenery_animation:
		client_apply_scenery_animation(event);
		break;
	case _coop_event_effect:
		client_apply_effect(event);
		break;
	default:
		break;
	}
}

/* client: the HUD settings in the host's presentation */
static void client_apply_hud_state(
	struct distributed_coop_presentation const *presentation)
{
	struct hud_timer_state timer;

	hud_scripted_globals->show_hud = TEST_FLAG(presentation->flags, _presentation_show_hud_bit);
	hud_scripted_globals->show_hud_help_text = TEST_FLAG(presentation->flags, _presentation_show_help_text_bit);
	hud_unit_port_set_script_flags(presentation->unit_hud_flags);
	hud_weapon_port_set_script_flags(presentation->weapon_hud_flags);
	timer.reference_time = presentation->timer_reference_time;
	timer.ticks = presentation->timer_ticks;
	timer.flash_cutoff = presentation->timer_flash_cutoff;
	timer.x = presentation->timer_x;
	timer.y = presentation->timer_y;
	timer.corner = presentation->timer_corner;
	timer.paused = TEST_FLAG(presentation->flags, _presentation_timer_paused_bit);
	timer.enabled = TEST_FLAG(presentation->flags, _presentation_timer_enabled_bit);
	hud_messaging_port_timer_set(&timer);
}

/* ---------- public code */

void network_coop_new_game(
	void)
{
	csmemset(&coop_presentation, 0, sizeof(coop_presentation));
	csmemset(&coop_events, 0, sizeof(coop_events));
	csmemset(&host_devices, 0, sizeof(host_devices));
	csmemset(&client_devices, 0, sizeof(client_devices));
	csmemset(client_names_differing, 0, sizeof(client_names_differing));
	skip_vote_clear();
	skip_vote.offered = FALSE;
	skip_vote.voters = 0;
	skip_vote.cooldown_until = 0;
	skip_vote.skip_save_written = FALSE;
}

/* A network game on a campaign scenario with no game engine. Checking the
scenario matters: the main menu's scene keeps running while a network game
is set up, also with no game engine, and it is not co-op. */
boolean network_coop_active(
	void)
{
	short connection = game_connection();

	return (connection == _game_connection_network_server || connection == _game_connection_network_client) &&
		global_scenario && global_scenario->type == _scenario_type_solo && !game_engine_running();
}

boolean network_coop_devices_remote(
	void)
{
	return coop_client();
}

void network_coop_note_device_snap(
	short group_index)
{
	if (coop_host() && group_index >= 0 && group_index < MAXIMUM_DEVICE_GROUPS)
		host_devices.snap_counts[group_index]++;
}

void network_coop_note_sound(
	short kind,
	long definition_index,
	long object_index,
	real scale)
{
	struct distributed_coop_event *event = event_new(_coop_event_sound);

	if (!event)
		return;
	event->type = (byte)kind;
	event->tag_index = definition_index;
	event->object_index = object_index;
	event->reals[0] = scale;
}

void network_coop_note_title(
	short title_index,
	real delay)
{
	struct distributed_coop_event *event = event_new(_coop_event_title);

	if (!event)
		return;
	event->value = title_index;
	event->reals[0] = delay;
}

void network_coop_note_hud(
	short kind,
	short value)
{
	struct distributed_coop_event *event = event_new(_coop_event_hud);

	if (!event)
		return;
	event->type = (byte)kind;
	event->value = value;
}

void network_coop_note_player_effect(
	short kind,
	real a,
	real b,
	real c)
{
	struct distributed_coop_event *event = event_new(_coop_event_player_effect);

	if (!event)
		return;
	event->type = (byte)kind;
	event->reals[0] = a;
	event->reals[1] = b;
	event->reals[2] = c;
}

void network_coop_note_nav_point(
	short kind,
	short nav_index,
	long target,
	long marker,
	real vertical_offset)
{
	struct distributed_coop_event *event = event_new(_coop_event_nav_point);

	if (!event)
		return;
	event->type = (byte)kind;
	event->value = nav_index;
	event->target = target;
	event->object_index = marker;
	event->reals[0] = vertical_offset;
}

void network_coop_note_unit_animation(
	long unit_index,
	long animation_graph_index,
	short animation_index,
	boolean interpolate)
{
	struct distributed_coop_event *event = event_new(_coop_event_unit_animation);

	if (!event)
		return;
	event->object_index = unit_index;
	event->tag_index = animation_graph_index;
	event->value = animation_index;
	event->interpolate = (byte)interpolate;
}

/* unit_custom_animation_at_frame starts the animation and then sets the
frame in the same call, so the start is still queued and unsent: the frame
goes into it */
void network_coop_note_unit_animation_frame(
	long unit_index,
	short frame_index)
{
	short index;

	if (!coop_host())
		return;
	for (index = coop_events.count - 1; index >= 0; index--)
	{
		struct distributed_coop_event *event = &coop_events.events[index];

		if (event->kind == _coop_event_unit_animation && event->object_index == unit_index)
		{
			if (coop_events.sends[index] == 0)
				event->frame = frame_index;
			return;
		}
	}
}

void network_coop_note_scenery_animation(
	long object_index,
	long animation_graph_index,
	short animation_index,
	short frame_index)
{
	struct object_datum *object = object_try_and_get(object_index);
	struct distributed_coop_event *event;

	if (!object || !(event = event_new(_coop_event_scenery_animation)))
		return;
	event->object_index = object_index;
	event->name_index = object->object.name_index;
	event->definition_index = object->definition_index;
	event->tag_index = animation_graph_index;
	event->value = animation_index;
	event->frame = frame_index;
}

void network_coop_note_effect(
	long effect_definition_index,
	short cutscene_flag_index)
{
	struct distributed_coop_event *event = event_new(_coop_event_effect);

	if (!event)
		return;
	event->type = _coop_effect_at_flag;
	event->tag_index = effect_definition_index;
	event->value = cutscene_flag_index;
}

void network_coop_note_object_effect(
	long effect_definition_index,
	long object_index,
	char const *marker_name)
{
	struct object_datum *object = object_try_and_get(object_index);
	long model_index = object ? object_definition_get(object->definition_index)->object.model.index : NONE;
	short marker_index = model_index != NONE ? model_find_marker(model_index, marker_name) : NONE;
	struct distributed_coop_event *event;

	if (marker_index == NONE || !(event = event_new(_coop_event_effect)))
		return;
	event->type = _coop_effect_on_marker;
	event->tag_index = effect_definition_index;
	event->object_index = object_index;
	event->name_index = object->object.name_index;
	event->definition_index = object->definition_index;
	event->value = marker_index;
}

boolean network_coop_skip_offered(
	void)
{
	return coop_client() && skip_vote.offered && coop_presentation.cinematic_started;
}

boolean network_coop_vote_skip(
	void)
{
	if (!coop_host() && !coop_client())
		return FALSE;
	skip_vote.voted = TRUE;
	return TRUE;
}

/* The skip reverted the host's game state, clock included. The clock is
put back, since clients and the netcode expect it to only go forward, and
the script threads' wake times move by the same amount so they still wake
when they would have. The revert also renumbered the input queues from the
old clock (update_queues_reset_and_fill_with_lies); they are renumbered
from the restored one, or no tick would run until they caught up. And the
revert is stamped with the restored time, since the script's game_reverted
compares the stamp with the clock: otherwise it would play the cinematic
it was meant to skip. */
void network_coop_skip_reverted(
	long now)
{
	long ticks;

	if (!coop_host())
		return;
	ticks = now - game_time_get();
	if (ticks > 0)
	{
		game_time_set_distributed(now);
		update_queues_reset_and_fill_with_lies();
		game_state_port_restamp_revert_time();
		hs_runtime_port_shift_sleep_times(ticks);
	}
	error(_error_silent, "co-op: cutscene skipped; reverted %ld ticks, clock kept at %ld", ticks, now);
	skip_vote_clear();
	skip_vote.requested = FALSE;
	skip_vote.cooldown_until = game_time_get() + SKIP_COOLDOWN_TICKS;
}

boolean network_coop_skip_vote_status(
	short *votes,
	short *voters,
	boolean *voted)
{
	boolean offered = coop_host() ? skip_vote.offered : network_coop_skip_offered();

	if (!offered || skip_vote.voters <= 0)
		return FALSE;
	*votes = skip_vote.votes;
	*voters = skip_vote.voters;
	*voted = skip_vote.voted;
	return TRUE;
}

/* host, after each tick */
void network_coop_host_tick(
	void)
{
	struct distributed_coop_presentation_message message;

	if (!coop_game())
		return;
	host_count_skip_votes();
	host_presentation(&message.presentation);
	distributed_send(&message, _distributed_message_coop_presentation, 1, (word)sizeof(message), _distributed_to_clients);
	host_send_device_groups();
	if (game_time_get() % OBJECT_NAMES_INTERVAL_TICKS == 0)
		host_send_object_names();
	host_send_events();
}

/* client, after each tick */
void network_coop_client_tick(
	void)
{
	if (game_time_get() - coop_presentation.heard_time > PRESENTATION_SILENCE_TICKS)
	{
		client_cinematic_end();
		if (coop_presentation.following_host)
		{
			coop_presentation.following_host = FALSE;
			director_script_camera(FALSE);
		}
	}
	if (!network_coop_skip_offered())
		skip_vote.voted = FALSE;
	else if (skip_vote.voted)
	{
		struct distributed_coop_skip_vote_message message;

		csmemset(&message.vote, 0, sizeof(message.vote));
		message.vote.voted = TRUE;
		distributed_send(&message, _distributed_message_coop_skip_vote, 1, (word)sizeof(message), _distributed_to_host);
	}
}

word network_coop_presentation_entry_size(
	void)
{
	return sizeof(struct distributed_coop_presentation);
}

word network_coop_event_entry_size(
	void)
{
	return sizeof(struct distributed_coop_event);
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

word network_coop_skip_vote_entry_size(
	void)
{
	return sizeof(struct distributed_coop_skip_vote);
}

/* host: a client's skip vote */
void network_coop_handle_skip_vote(
	long machine_index,
	void const *entries)
{
	struct distributed_coop_skip_vote const *vote = entries;

	if (!coop_host() || machine_index < 0 || machine_index >= HALO_PORT_MAXIMUM_NETWORK_MACHINES)
		return;
	skip_vote.client_vote_times[machine_index] = vote->voted ? game_time_get() : NONE;
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

/* client: applies each event once, in order */
void network_coop_handle_events(
	void const *entries,
	short count)
{
	struct distributed_coop_event const *events = entries;
	short index;

	if (!coop_game())
		return;
	for (index = 0; index < count; index++)
	{
		/* already applied (the comparison handles the number wrapping) */
		if (coop_events.applied_any && (short)(events[index].number - coop_events.applied_number) <= 0)
			continue;
		coop_events.applied_number = events[index].number;
		coop_events.applied_any = TRUE;
		client_apply_event(&events[index]);
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
		director_script_camera(TRUE);
		coop_presentation.cinematic_started = TRUE;
		coop_presentation.following_host = FALSE;
	}
	else if (!cinematic)
	{
		client_cinematic_end();
	}
	/* A player with no unit and no living teammate to watch (waiting for the
	level's first checkpoint, with the host's unit unseen) would look out of
	the world from a dead camera at the origin: it sees the host's view. */
	if (!coop_presentation.cinematic_started &&
		coop_spectate_nothing_to_watch(0) != coop_presentation.following_host)
	{
		coop_presentation.following_host = !coop_presentation.following_host;
		director_script_camera(coop_presentation.following_host);
	}
	if (coop_presentation.cinematic_started || coop_presentation.following_host)
	{
		real_vector3d forward, up;

		if (coop_presentation.cinematic_started)
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

	skip_vote.offered = TEST_FLAG(presentation->flags, _presentation_skippable_bit);
	skip_vote.votes = presentation->skip_votes;
	skip_vote.voters = presentation->skip_voters;
	client_apply_hud_state(presentation);
}
