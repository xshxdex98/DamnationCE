/*
EDITOR_PLAY.C

The level editor's live view (OpenCE-Tools' level window), as Sapien's: the
editor starts the game in its view on the level it edits, built to a map of
its own, and the game runs it, seen through the editor's flying camera
(director.c's editor mode) with no player in it and its scripts paused.
PLAY puts the player in at the camera, as the game plays; STOP takes them
out and gives the flying camera back. The editor says what it wants in the
game's environment:

    HALO_PLAY_MAP   the map's file name, without .map
    HALO_EDITOR     1 for the live view
    HALO_EMBEDDED   1 for a window the editor puts in its view
                    (port/linux/src/sdl_platform.c)

and sends it commands over the telnet console (debug.telnet_console, which
it turns on), each a line:

    editor view x y z yaw pitch   the flying camera there
    editor play                   the player in at the camera
    editor stop                   back to the flying camera
    editor reload <map>           the map, built again, loaded in its place
    editor camera                 answers "editor camera x y z yaw pitch"
*/

#include "cseries.h"
#include "math/real_math.h"
#include "camera/director.h"
#include "camera/editor_flying_camera.h"
#include "camera/observer.h"
#include "game/players.h"
#include "main/main.h"
#include "networking/telnet_console.h"
#include "objects/objects.h"
#include "scenario/scenario_definitions.h"

#include "editor_play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (players.c's) */
void network_player_detach_unit(long player_index);
/* (menu_functions.c's: a map started as the menus start one) */
void menu_start_map(char const *map_name, short difficulty, short controller);

/* how far below the flying camera PLAY puts the player's feet (world units:
a standing biped's eyes) */
#define PLAY_EYE_HEIGHT 0.62f

static struct
{
	boolean live;               /* the editor's live view (HALO_EDITOR) */
	boolean playing;            /* PLAY: the player is in */
	boolean spawn_pending;      /* PLAY: the player not yet put in */
	struct player_starting_location spawn;
} editor_play;

/* the camera local player 0 sees through */
static void editor_play_camera(
	real_point3d *position,
	real_euler_angles2d *angles)
{
	struct observer_result const *camera = observer_get_camera(0);

	*position = camera->position;
	euler_angles2d_from_vector3d(angles, &camera->forward);
}

static void editor_play_reply_camera(
	void)
{
	real_point3d position;
	real_euler_angles2d angles;
	char reply[128];

	editor_play_camera(&position, &angles);
	snprintf(reply, sizeof(reply), "editor camera %.3f %.3f %.3f %.4f %.4f",
		position.x, position.y, position.z, angles.yaw, angles.pitch);
	telnet_console_print(reply);
}

static void editor_play_play(
	void)
{
	real_euler_angles2d angles;

	if (editor_play.playing)
		return;
	csmemset(&editor_play.spawn, 0, sizeof(editor_play.spawn));
	editor_play_camera(&editor_play.spawn.position, &angles);
	editor_play.spawn.position.z -= PLAY_EYE_HEIGHT;
	editor_play.spawn.facing = angles.yaw;
	editor_play.spawn_pending = TRUE;
	editor_play.playing = TRUE;
	director_set_editing(FALSE);
}

/* (the player's unit taken away, the flying camera where they looked) */
static void editor_play_stop(
	void)
{
	long player_index = local_player_get_player_index(0);
	long unit_index = player_index != NONE ? player_get(player_index)->unit_index : NONE;
	real_point3d position;
	real_euler_angles2d angles;

	if (!editor_play.playing)
		return;
	editor_play_camera(&position, &angles);
	if (unit_index != NONE)
	{
		network_player_detach_unit(player_index);
		object_delete(unit_index);
	}
	editor_play.playing = FALSE;
	editor_play.spawn_pending = FALSE;
	/* (the flying camera, made anew, starts at its focus: editor_camera_new) */
	editor_camera_set_focus(&position, &angles);
	director_set_editing(TRUE);
}

static void editor_play_reload(
	char const *map_name)
{
	real_point3d position;
	real_euler_angles2d angles;

	/* (the flying camera keeps its place on the new map: editor_camera_new) */
	editor_play_camera(&position, &angles);
	editor_camera_set_focus(&position, &angles);
	editor_play.playing = FALSE;
	editor_play.spawn_pending = FALSE;
	main_set_map_name(map_name);
}

/* ---------- public code */

void editor_play_main_menu_loaded(
	void)
{
	static boolean started = FALSE;
	char const *map = getenv("HALO_PLAY_MAP");
	char const *live = getenv("HALO_EDITOR");

	if (started || !map || !*map)
		return;
	started = TRUE;
	editor_play.live = live && !strcmp(live, "1");
	menu_start_map(map, main_get_difficulty(), 0);
}

boolean editor_play_editing(
	void)
{
	return editor_play.live && !editor_play.playing;
}

struct player_starting_location const *editor_play_spawn_location(
	void)
{
	if (!editor_play.spawn_pending)
		return NULL;
	editor_play.spawn_pending = FALSE;
	return &editor_play.spawn;
}

boolean editor_play_command(
	char const *line)
{
	real_point3d position;
	real_euler_angles2d angles;
	char map_name[64];

	if (!editor_play.live || strncmp(line, "editor ", 7))
		return FALSE;
	line += 7;
	if (sscanf(line, "view %f %f %f %f %f",
		&position.x, &position.y, &position.z, &angles.yaw, &angles.pitch) == 5)
	{
		editor_camera_set_focus(&position, &angles);
		editor_camera_set_position(&position, &angles);
	}
	else if (!strcmp(line, "play"))
		editor_play_play();
	else if (!strcmp(line, "stop"))
		editor_play_stop();
	else if (sscanf(line, "reload %63s", map_name) == 1)
		editor_play_reload(map_name);
	else if (!strcmp(line, "camera"))
		editor_play_reply_camera();
	return TRUE;
}
