/*
EDITOR_PLAY.H

Playing a map from the level editor (editor_play.c).
*/

#ifndef __EDITOR_PLAY_H
#define __EDITOR_PLAY_H

/* once the main menu has loaded (main_load_ui_scenario): goes to the map
the level editor asked for, if it asked, as New Game does */
void editor_play_main_menu_loaded(
	void);

/* TRUE once, the first time it is asked once the player is in the map, if
the level editor asked for the flying camera (director.c turns to it) */
boolean editor_play_take_flying_start(
	void);

#endif
