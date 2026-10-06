/*
EDITOR_PLAY.C

Playing a map from the level editor (OpenCE-Tools' PLAY and FLY): the game
it starts goes straight into the map it saved, as the Editing Kit's
init.txt does with map_name, and for FLY into the flying camera, which
Backspace turns to in any game (director.c). The editor says what it wants
in the environment of the game it starts, so nothing of it is kept in
config.toml:

    HALO_PLAY_MAP   the map's file name, without .map
    HALO_PLAY_FLY   1 to start in the flying camera
*/

#include "cseries.h"
#include "main/main.h"

#include "editor_play.h"

#include <stdlib.h>
#include <string.h>

void editor_play_startup(
	void)
{
	char const *map = getenv("HALO_PLAY_MAP");

	if (map && *map)
		main_set_map_name(map);

	return;
}

boolean editor_play_take_flying_start(
	void)
{
	static boolean taken = FALSE;
	char const *fly = getenv("HALO_PLAY_FLY");

	if (taken || !fly || strcmp(fly, "1"))
		return FALSE;
	taken = TRUE;

	return TRUE;
}
