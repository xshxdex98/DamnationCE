/*
OVERLAY_SCREENS.H

What the screens the overlay draws over the menus share: Online Games
(browser_screen.c) and the map picker (map_screen.c). overlay_screens.c.
*/

#ifndef __OVERLAY_SCREENS_H
#define __OVERLAY_SCREENS_H

/* ---------- constants */

enum
{
	/* (event_manager.c's event types, which it keeps to itself) */
	OVERLAY_EVENT_LEFT_STICK = 1,
	OVERLAY_EVENT_BUTTON = 3,
};

/* ---------- structures */

/* a direction held down: the game reports it every frame it is held */
struct overlay_repeat
{
	boolean held;
	unsigned long next_time;
};

/* ---------- prototypes */

/* Whether a direction held (or not) moves this frame: once when it is first
pressed, then again after a moment, then steadily while it stays held. */
boolean overlay_repeat_step(
	struct overlay_repeat *repeat,
	boolean held);

/* UTF-16 text, at most `length` characters (it may stop sooner, at a 0), as
UTF-8 in `out` of `size` bytes. */
void overlay_utf8(
	unsigned short const *text,
	long length,
	char *out,
	long size);

/* A button and what it does ("=JOIN") along the foot of the screen, from x:
the x after it. */
float overlay_prompt(
	int button,
	char const *words,
	float x,
	unsigned int color);
float overlay_prompt_width(
	int button,
	char const *words);

/* The display index of a map, by its map name (levels\test\<name>\<name>):
an Xbox level's frame of the menus' level pictures, a Custom Edition map's
display index (custom_edition_maps.h), else the unknown level's frame. */
short overlay_map_display_index(
	char const *map_name);

/* A map's picture, by its display index, in a place of the screen, which the
overlay leaves to the game's drawing. */
void overlay_map_picture(
	short display_index,
	float x,
	float y,
	float width,
	float height);

#endif
