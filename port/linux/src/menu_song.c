/*
MENU_SONG.C

A menus theme's own song (menu_song.h), played in place of the game's menu
music: music/<theme>.wav beside config.toml (the Cairo theme's,
music/cairo.wav), which a player puts there, mixed into the game's sound
(dsound_sdl.c) at the music's volume.

It fades in and out, and loops as the game's menu music does: each time
round it fades in from its start and out at its end.
*/

#include "menu_song.h"
#include "platform.h"
#include "port_config.h"

#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* ---------- constants */

enum
{
	/* dsound_sdl.c's output, which the song is converted to as it loads */
	SONG_RATE = 48000,
	SONG_CHANNELS = 2,
};

#define FADE_SECONDS 1.5f       /* playing or stopping */
#define LOOP_FADE_SECONDS 3.0f  /* at each end, going round */

/* ---------- globals */

static struct
{
	/* (the mixer's thread reads what the game's thread sets) */
	pthread_mutex_t lock;
	/* the theme whose song is loaded ("" for none yet), and its samples:
	SONG_CHANNELS interleaved at SONG_RATE, or NULL if it has none */
	char theme[32];
	Sint16 *samples;
	unsigned long frames;
	unsigned long position;
	/* the fade: gain moves toward target, 0 or 1 */
	float gain, target;
	float volume;
	int playing;
} song = { PTHREAD_MUTEX_INITIALIZER };

/* ---------- private code */

/* a WAV file as SONG_CHANNELS 16-bit samples at SONG_RATE, or NULL */
static Sint16 *song_read(char const *path, unsigned long *frames)
{
	SDL_AudioSpec spec, wanted = { SDL_AUDIO_S16, SONG_CHANNELS, SONG_RATE };
	Uint8 *data, *converted = NULL;
	Uint32 size;
	int converted_size = 0;

	if (!SDL_LoadWAV(path, &spec, &data, &size))
		return NULL;
	if (!SDL_ConvertAudioSamples(&spec, data, (int)size, &wanted, &converted, &converted_size))
	{
		platform_log("menu song: cannot convert %s (%s)", path, SDL_GetError());
		converted = NULL;
	}
	SDL_free(data);
	*frames = converted ? (unsigned long)converted_size / (SONG_CHANNELS * sizeof(Sint16)) : 0;
	return (Sint16 *)converted;
}

/* ---------- public code */

int menu_song_load(char const *theme)
{
	char path[1200];
	unsigned long frames = 0;
	Sint16 *samples;

	if (!strcmp(song.theme, theme))
		return song.samples != NULL;
	config_folder(path, sizeof(path));
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "music/%s.wav", theme);
	samples = song_read(path, &frames);
	if (samples)
		platform_log("menu song: %s, %lu seconds", path, frames / SONG_RATE);
	pthread_mutex_lock(&song.lock);
	SDL_free(song.samples);
	song.samples = frames ? samples : NULL;
	song.frames = frames;
	song.position = 0;
	song.gain = song.target = 0.0f;
	song.playing = 0;
	snprintf(song.theme, sizeof(song.theme), "%s", theme);
	pthread_mutex_unlock(&song.lock);
	return song.samples != NULL;
}

void menu_song_play(void)
{
	pthread_mutex_lock(&song.lock);
	if (song.samples)
	{
		if (!song.playing)
		{
			song.position = 0;
			song.gain = 0.0f;
			song.playing = 1;
		}
		song.target = 1.0f;
	}
	pthread_mutex_unlock(&song.lock);
}

void menu_song_stop(void)
{
	pthread_mutex_lock(&song.lock);
	song.target = 0.0f;
	pthread_mutex_unlock(&song.lock);
}

int menu_song_playing(void)
{
	int playing;

	pthread_mutex_lock(&song.lock);
	playing = song.playing;
	pthread_mutex_unlock(&song.lock);
	return playing;
}

void menu_song_update(void)
{
	static unsigned long read_at = (unsigned long)-1;

	/* (read here, not on the mixer's thread: the config may read its file) */
	if (read_at != config_changes())
	{
		read_at = config_changes();
		song.volume = (float)(config_real("audio.music_volume") * config_real("audio.volume"));
	}
}

void menu_song_mix(float *output, unsigned long frames)
{
	float step = 1.0f / (FADE_SECONDS * SONG_RATE);
	float loop_fade = LOOP_FADE_SECONDS * SONG_RATE;
	unsigned long frame;

	pthread_mutex_lock(&song.lock);
	for (frame = 0; frame < frames && song.playing; frame++)
	{
		float edge, scale;
		short channel;

		song.gain += song.gain < song.target ? step : -step;
		if (song.gain >= 1.0f && song.target >= 1.0f)
			song.gain = 1.0f;
		if (song.gain <= 0.0f && song.target <= 0.0f)
		{
			song.playing = 0;
			break;
		}
		/* (each time round: in from the start, out at the end) */
		edge = SDL_min(1.0f, SDL_min((float)song.position, (float)(song.frames - song.position)) / loop_fade);
		scale = song.gain * edge * song.volume / 32768.0f;
		for (channel = 0; channel < SONG_CHANNELS; channel++)
			output[frame * SONG_CHANNELS + channel] += song.samples[song.position * SONG_CHANNELS + channel] * scale;
		if (++song.position >= song.frames)
			song.position = 0;
	}
	pthread_mutex_unlock(&song.lock);
}
