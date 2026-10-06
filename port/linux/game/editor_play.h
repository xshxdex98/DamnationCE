/*
EDITOR_PLAY.H

The level editor's live view (editor_play.c).
*/

#ifndef __EDITOR_PLAY_H
#define __EDITOR_PLAY_H

struct player_starting_location;

/* once the main menu has loaded (main_load_ui_scenario): starts the map the
level editor asked for, if it asked, as the menus start one */
void editor_play_main_menu_loaded(
	void);

/* whether the editor's live view shows the level with no player in it, its
scripts paused (game.c), its players unspawned (players.c) and the camera
the editor's (director.c) */
boolean editor_play_editing(
	void);

/* where PLAY puts the player (player_spawn), once, or NULL */
struct player_starting_location const *editor_play_spawn_location(
	void);

/* a console line (hs.c's hs_compile_and_evaluate): TRUE if it was one of the
editor's ("editor ...") in its live view, which it has done */
boolean editor_play_command(
	char const *line);

#endif
