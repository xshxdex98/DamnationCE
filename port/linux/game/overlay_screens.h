/*
OVERLAY_SCREENS.H

Helpers shared by the screens drawn over the menus: Online Games
(browser_screen.c), the map picker (map_screen.c) and the lobby
(lobby_screen.c).
*/

#ifndef __OVERLAY_SCREENS_H
#define __OVERLAY_SCREENS_H

/* ---------- constants */

enum
{
	/* event_manager.c's event types, which it keeps private */
	OVERLAY_EVENT_LEFT_STICK = 1,
	OVERLAY_EVENT_BUTTON = 3,

	/* the button bar along the foot of the 640x480 layout */
	OVERLAY_BUTTON_Y = 450,
	OVERLAY_BUTTON_HEIGHT = 22,

	/* the screens' frame: Glassed's band over the scene, and the line over
	the buttons in every theme */
	OVERLAY_FRAME_TOP = 66,
	OVERLAY_FRAME_BOTTOM = 446,
};

/* ---------- structures */

/* key repeat for a held direction (the game reports it every frame) */
struct overlay_repeat
{
	boolean held;
	unsigned long next_time;
};

/* overlay_buttons_draw's colors (0xRRGGBBAA) */
struct overlay_button_colors
{
	unsigned int fill, fill_lit, edge, text, text_lit, text_disabled;
	float radius;
};

/* How the screens drawn over the menus look in a theme (colors 0xRRGGBBAA):
Glassed darkens a band over the scene, Vanilla covers the screen in the
Xbox's blues, Cairo covers it in Halo 2's navy under a header band. */
struct overlay_palette
{
	/* this client's own screens (the map picker, the lobby's, the game list
	reaching the screen's edges), rather than the PC version's */
	boolean own_screens;
	/* Halo 2's frames: the title on a header band, panels cut at their
	corners with steel brackets, rows on slate bars behind brackets */
	boolean framed;
	unsigned int backdrop, backdrop_bottom, rule, title, panel, panel_edge, panel_head, head, row_selected, row_rule;
	unsigned int text, dim, label, prompt, connecting;
	float radius;
};

/* the same in both themes */
enum
{
	OVERLAY_COLOR_RED_TEAM = 0xFF6B6BFF,
	OVERLAY_COLOR_BLUE_TEAM = 0x6BB0FFFF,
	/* notices, and what can't be joined */
	OVERLAY_COLOR_NOTICE = 0x3CC8C0FF,
	OVERLAY_COLOR_GOOD = 0x5ED38CFF,
	OVERLAY_COLOR_FAIR = 0xE8C547FF,
	OVERLAY_COLOR_POOR = 0xE86A5AFF,
};

/* ---------- prototypes */

/* Whether a held direction should move this frame: once when first
pressed, again after a short delay, then steadily while held. */
boolean overlay_repeat_step(
	struct overlay_repeat *repeat,
	boolean held);

/* Converts UTF-16 text (at most `length` characters, or up to a 0) to UTF-8
in `out`, which holds `size` bytes. */
void overlay_utf8(
	unsigned short const *text,
	long length,
	char *out,
	long size);

/* A row of clickable buttons, each as wide as its label, starting at x.
`hovered` is lit (NONE for none); bit n of `disabled` greys out button n.
overlay_button_at returns the button at a point, or NONE. */
float overlay_buttons_width(
	char const *const *labels,
	short count);
/* the buttons' colors in the theme's palette (their text its prompt's) */
void overlay_button_colors_get(
	struct overlay_button_colors *colors);
void overlay_buttons_draw(
	char const *const *labels,
	short count,
	float x,
	float y,
	short hovered,
	unsigned long disabled,
	struct overlay_button_colors const *colors);
short overlay_button_at(
	char const *const *labels,
	short count,
	float x,
	float y,
	short point_x,
	short point_y);

/* the palette of the theme in use (display.theme) */
struct overlay_palette const *overlay_palette_current(
	void);

/* The parts every screen has, in the theme's look. What lies behind the
screen, and its title: at title_x, title_y, title_size high in Glassed and
Vanilla; on its header band in Cairo. */
void overlay_screen_frame(
	char const *title,
	float title_x,
	float title_y,
	float title_size);
/* a line of small print under the title (how many games there are): at x,
y in Glassed and Vanilla, after Cairo's header band */
void overlay_screen_subtitle(
	char const *text,
	float x,
	float y);
/* a panel things are set out on */
void overlay_panel(
	float x,
	float y,
	float width,
	float height);
/* A list's row: lit if chosen; else in Glassed and Vanilla shaded if
striped (every other row), and in Cairo on its slate bar. */
void overlay_row(
	float x,
	float y,
	float width,
	float height,
	boolean chosen,
	boolean striped);
/* one button, its label in its middle: lit (the pointer or the focus is on
it), or greyed when not usable */
void overlay_button_draw(
	char const *label,
	float x,
	float y,
	float width,
	float height,
	boolean lit,
	boolean usable,
	struct overlay_button_colors const *colors);

/* Draws UTF-8 text left-aligned at x, cut to `width` with an ellipsis if it
doesn't fit. */
void overlay_text_fitted(
	int font,
	float size,
	float x,
	float y,
	float width,
	unsigned int color,
	char const *text);

/* An Xbox multiplayer level's display name by its index in the game's
level order, or NULL past them */
char const *overlay_xbox_level_name(
	short level);
/* An Xbox multiplayer level's display name by its file name ("bloodgulch":
"Blood Gulch"), or NULL for any other map. */
char const *overlay_xbox_map_name(
	char const *file_name);

/* A map's display name, for a map path such as levels\test\<name>\<name>:
an Xbox level's, a Custom Edition map's or campaign level's own, else its
file name. */
void overlay_map_name(
	char const *map_name,
	char *out,
	long size);

/* The display index (custom_edition_maps.h) for a map path such as
levels\test\<name>\<name>. Unknown maps get the unknown level's frame. */
short overlay_map_display_index(
	char const *map_name);

/* Draws a map's picture, by display index, into a rectangle that the
overlay leaves clear for the game to draw. */
void overlay_map_picture(
	short display_index,
	float x,
	float y,
	float width,
	float height);

/* This client's screens: highlights the focused row of the gametype lists,
matching the overlay's own lists. Called by ui_widget.c after the menus are
drawn. */
void overlay_lit_row_render(
	void);

#endif
