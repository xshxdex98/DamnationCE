/*
MACOS_LOOSE_SOUNDS.C

The 64-bit build's stand-in for the sounds of tag files played over a map's
(port/linux/game/loose_sounds.c, audio.loose_sounds), compiled with the
host's own ABI (the game's boolean is a byte).

loose_sounds.c points the map's sound tags at what it reads, which only the
32-bit builds can do: here those are Xbox addresses. Until it is written for
them, every sound plays from the map.
*/

void loose_sounds_tags_loaded(void)
{
}

void loose_sounds_tags_unloaded(void)
{
}

/* (never: the sound cache reads the map's) */
unsigned char loose_sounds_read(void const *permutation, void *buffer)
{
	(void)permutation;
	(void)buffer;
	return 0;
}

void loose_sounds_reload(void)
{
}

void loose_sounds_enable(unsigned char enabled)
{
	(void)enabled;
}
