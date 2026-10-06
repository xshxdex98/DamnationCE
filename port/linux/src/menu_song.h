/*
MENU_SONG.H

A menus theme's own song (menu_song.c): played in place of the game's menu
music, mixed into the game's sound by dsound_sdl.c.
*/

#ifndef MENU_SONG_H
#define MENU_SONG_H

/* (the game's thread) Whether a theme has a song: music/<theme>.wav beside
config.toml, loaded the first time it is asked for. */
int menu_song_load(char const *theme);
/* the loaded song fading in (from its start, if it had stopped) */
void menu_song_play(void);
/* the song fading out, then stopping */
void menu_song_stop(void);
/* whether it is playing, fading out included */
int menu_song_playing(void);
/* the music's volume read again when the settings change; called every frame */
void menu_song_update(void);

/* (the mixer's thread) the song added to frames of 48 kHz stereo output */
void menu_song_mix(float *output, unsigned long frames);

#endif
