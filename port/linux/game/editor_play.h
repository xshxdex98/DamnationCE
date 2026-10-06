/*
EDITOR_PLAY.H

Playing a map from the level editor (editor_play.c).
*/

#ifndef __EDITOR_PLAY_H
#define __EDITOR_PLAY_H

/* at start-up (console_startup): goes straight into the map the level
editor asked for, if it asked */
void editor_play_startup(
	void);

/* TRUE once, the first time it is asked once the player is in the map, if
the level editor asked for the flying camera (director.c turns to it) */
boolean editor_play_take_flying_start(
	void);

#endif
