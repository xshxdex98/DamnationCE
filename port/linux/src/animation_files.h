/*
ANIMATION_FILES.H

The animation graphs the game carries (port/assets/animations, converted by
tools/player_animations.py), embedded by tools/embed_assets.py and loaded by
port/linux/game/player_animations.c.
*/

#ifndef ANIMATION_FILES_H
#define ANIMATION_FILES_H

/* an embedded graph, by its file name in port/assets/animations */
struct animation_file_embedded
{
	const char *name;
	const unsigned int *data;
	unsigned int size;
};

extern const struct animation_file_embedded animation_files_embedded[];
extern const unsigned int animation_files_embedded_count;

#endif
