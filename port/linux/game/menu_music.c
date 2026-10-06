/*
MENU_MUSIC.C

The main menu's music by theme: the game's own (TITLE_MUSIC) in Glassed and
Vanilla, and in Cairo its song (port/linux/src/menu_song.c), if a player
has put one beside config.toml. Changing themes at the main menu fades the
music playing out, then the other in.

The game's menu music fades by its scale, which its sounds turn into their
gain (0 to 1); it keeps playing, silent, while the theme's song plays.
*/

#include "cseries.h"
#include "interface/ui_widget.h"
#include "sound/game_sound.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_groups.h"

#include "halo_menus.h"
#include "../src/menu_song.h"

/* ---------- constants */

#define TITLE_MUSIC "sound\\music\\title1\\title1"
#define FADE_MILLISECONDS 1500

enum song
{
	SONG_NONE,
	SONG_GAME,
	SONG_THEME,
};

enum phase
{
	PHASE_FADING_IN,
	PHASE_PLAYING,
	PHASE_FADING_OUT,
};

/* ---------- globals */

static struct
{
	enum song song;
	enum phase phase;
	unsigned long phase_time;
} music = { SONG_NONE, PHASE_PLAYING, 0 };

/* ---------- private code */

/* the song the menus should play now */
static enum song song_wanted(void)
{
	if (!ui_main_menu_music_active())
		return SONG_NONE;
	if (halo_menus_theme() == HALO_MENU_THEME_CAIRO && menu_song_load("cairo"))
		return SONG_THEME;
	return SONG_GAME;
}

/* how far a fade has got, 0 to 1 */
static real fade_done(unsigned long now)
{
	return MIN(1.0f, (real)(now - music.phase_time) / FADE_MILLISECONDS);
}

/* the game's menu music's scale: its gain while it fades or plays, 0 while
the theme's song plays */
static void game_music_scale(unsigned long now)
{
	long title = tag_loaded(LOOPING_SOUND_DEFINITION_TAG, TITLE_MUSIC);
	real scale = 0.0f;

	if (title == NONE || !ui_main_menu_music_active())
		return;
	if (music.song == SONG_GAME)
	{
		switch (music.phase)
		{
		case PHASE_FADING_IN: scale = fade_done(now); break;
		case PHASE_FADING_OUT: scale = 1.0f - fade_done(now); break;
		default: scale = 1.0f; break;
		}
	}
	scripted_looping_sound_set_scale(title, scale);
}

static void phase_begin(enum phase phase, unsigned long now)
{
	music.phase = phase;
	music.phase_time = now;
}

/* ---------- public code */

/* (ui_widget.c, every frame the menus are drawn, and as the menus' music
starts) */
void menu_music_update(void)
{
	unsigned long now = system_milliseconds();
	enum song wanted = song_wanted();

	menu_song_update();
	if (wanted == music.song)
	{
		/* (asked back while fading out: in again from where it got to) */
		if (music.phase == PHASE_FADING_OUT)
		{
			phase_begin(PHASE_FADING_IN, now - (unsigned long)((1.0f - fade_done(now)) * FADE_MILLISECONDS));
			if (music.song == SONG_THEME)
				menu_song_play();
		}
		else if (music.phase == PHASE_FADING_IN && fade_done(now) >= 1.0f)
		{
			phase_begin(PHASE_PLAYING, now);
		}
	}
	else if (music.phase != PHASE_FADING_OUT && music.song != SONG_NONE)
	{
		phase_begin(PHASE_FADING_OUT, now);
		if (music.song == SONG_THEME)
			menu_song_stop();
	}
	else if (music.song == SONG_NONE ||
		(music.song == SONG_GAME ? fade_done(now) >= 1.0f : !menu_song_playing()))
	{
		/* the one before faded out (the game's own starts at full, as
		it always has, unless it follows the theme's) */
		boolean after_theme = music.song == SONG_THEME;

		music.song = wanted;
		phase_begin(wanted == SONG_GAME && !after_theme ? PHASE_PLAYING : PHASE_FADING_IN, now);
		if (wanted == SONG_THEME)
			menu_song_play();
	}
	game_music_scale(now);
}
