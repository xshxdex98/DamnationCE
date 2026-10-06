/*
EDITOR_PLAY.C

Playing a map from the level editor (OpenCE-Tools' PLAY and FLY), in its
view: the game it starts goes to the map it saved once the main menu has
loaded, as New Game does (main_set_map_name: the menu fades, the map is
copied to the cache and loaded), and for FLY into the flying camera, which
Backspace turns to in any game (director.c). The editor says what it wants
in the environment of the game it starts, so nothing of it is kept in
config.toml:

    HALO_PLAY_MAP   the map's file name, without .map
    HALO_PLAY_FLY   1 to start in the flying camera
    HALO_PLAY_START "x y z facing": where the player starts in a map
                    with no starting location for them (players.c)
    HALO_EMBEDDED   1 for a window the editor puts in its view
                    (port/linux/src/sdl_platform.c)
*/

#include "cseries.h"
#include "math/real_math.h"
#include "main/main.h"
#include "scenario/scenario_definitions.h"

#include "editor_play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static boolean environment_is_on(
	char const *name)
{
	char const *value = getenv(name);

	return value && !strcmp(value, "1");
}

void editor_play_main_menu_loaded(
	void)
{
	static boolean asked = FALSE;
	char const *map = getenv("HALO_PLAY_MAP");

	if (!asked && map && *map)
		main_set_map_name(map);
	asked = TRUE;

	return;
}

boolean editor_play_take_flying_start(
	void)
{
	static boolean taken = FALSE;

	if (taken || !environment_is_on("HALO_PLAY_FLY"))
		return FALSE;
	taken = TRUE;

	return TRUE;
}

struct player_starting_location const *editor_play_starting_location(
	void)
{
	static struct player_starting_location start;
	char const *value = getenv("HALO_PLAY_START");

	if (!value || sscanf(value, "%f %f %f %f",
		&start.position.x, &start.position.y, &start.position.z, &start.facing) != 4)
	{
		return NULL;
	}

	return &start;
}
