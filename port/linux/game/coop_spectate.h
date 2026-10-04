/*
COOP_SPECTATE.H

A dead player of a co-op game over the network watching the living
(coop_spectate.c).
*/

#ifndef __COOP_SPECTATE_H
#define __COOP_SPECTATE_H

struct dead_camera;

/* Whether the game is co-op over the network: a network game no game
engine runs (a campaign map). */
boolean coop_spectating(
	void);

/* The unit a dead local player watches (a living teammate's; A moves to the
next), or NONE when it is alive or no one is. */
long coop_spectate_unit(
	short local_player_index);

/* The dead camera watching it, put behind it as it turns. */
void coop_spectate_camera(
	struct dead_camera *camera);

/* The HUD's line of whom a dead local player watches. */
void coop_spectate_draw(
	short local_player_index);

#endif
