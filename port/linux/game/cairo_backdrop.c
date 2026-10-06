/*
CAIRO_BACKDROP.C

The Cairo menus theme's backdrop: streams of data and rulers sliding across
the whole window, as Halo 2's menus have, and each screen's header band
carried on to the window's left edge. tools/cairo_art.py draws the pieces
(skin/cairo/shell/bitmaps.xml lists them).

The menus' pictures are cut to the middle 640 units unless they are flat
strips, which are widened to the window (ui_widget.c), so what crosses the
whole window is drawn here: right after a screen's cover, the strip it
stands on, so that it lies behind everything else on the screen.
*/

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "rasterizer/rasterizer.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"

#include "halo_menus.h"

#include <math.h>
#include <string.h>

/* ---------- constants */

/* the covers: the main menu's, and the PC screens' strips */
#define MAIN_MENU_COVER "pc\\shell\\panel"
static char const *const screen_covers[] = { "pc\\bitmaps\\gradient", "pc\\bitmaps\\gradient_big" };

/* the header band's strip, and its top (cairo_art.py's HEADER_TOP) */
#define BAND "pc\\shell\\cairo_band"
#define BAND_TOP 30.0f

/* the pieces' tints (0xAARRGGBB): the grid's blue for the streams, the
hairlines' for the rulers */
#define STREAM_TINT 0x568ED4
#define RULER_TINT 0x64A4E8

/* ---------- definitions */

/* a piece repeated across the window and sliding along: its picture, its
top, how fast it slides (units a second, rightward), and how opaque */
struct slide
{
	char const *bitmap;
	real y;
	real speed;
	unsigned char alpha;
	unsigned long tint;
};

static struct slide const main_menu_slides[] =
{
	{ "pc\\shell\\cairo_stream_0", 150.0f, -14.0f, 40, STREAM_TINT },
	{ "pc\\shell\\cairo_ruler", 188.0f, 6.0f, 80, RULER_TINT },
	{ "pc\\shell\\cairo_stream_1", 372.0f, 10.0f, 30, STREAM_TINT },
	{ "pc\\shell\\cairo_stream_0", 452.0f, -8.0f, 24, STREAM_TINT },
};

static struct slide const screen_slides[] =
{
	/* (the ruler under the line the header band stands on) */
	{ "pc\\shell\\cairo_ruler", 60.0f, 5.0f, 70, RULER_TINT },
	{ "pc\\shell\\cairo_stream_1", 160.0f, -9.0f, 18, STREAM_TINT },
	{ "pc\\shell\\cairo_stream_0", 386.0f, 12.0f, 18, STREAM_TINT },
	{ "pc\\shell\\cairo_stream_1", 456.0f, -10.0f, 26, STREAM_TINT },
};

/* ---------- private code */

static struct bitmap_data *picture(
	char const *name)
{
	long tag_index = tag_loaded(BITMAP_GROUP_TAG, name);

	return tag_index == NONE ? NULL : bitmap_group_get_bitmap_from_sequence(tag_index, 0, 0);
}

/* A picture over [left, right) at y, its own height, in color: repeated
across from u0 (in pictures' widths) if wrapped, else stretched. */
static void picture_draw(
	struct bitmap_data *bitmap,
	real left,
	real right,
	real y,
	real u0,
	boolean wrapped,
	pixel32 color)
{
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	struct dynamic_screen_vertex vertices[4];
	real u1 = wrapped ? u0 + (right - left) / bitmap->width : 1.0f;
	short index;

	for (index = 0; index < 4; index++)
	{
		boolean right_side = index == 1 || index == 2;
		boolean bottom = index >= 2;

		vertices[index].position.x = right_side ? right : left;
		vertices[index].position.y = bottom ? y + bitmap->height : y;
		vertices[index].texture_coordinates.x = right_side ? u1 : u0;
		vertices[index].texture_coordinates.y = bottom ? 1.0f : 0.0f;
		vertices[index].color = color;
	}
	memset(&parameters, 0, sizeof(parameters));
	parameters.map[0] = bitmap;
	parameters.map_wrapped[0] = wrapped;
	parameters.map_scale[0].i = parameters.map_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = parameters.map_texture_scale[0].j = 1.0f;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

static void slides_draw(
	struct slide const *slides,
	short count,
	real left,
	real right)
{
	/* (in double: the milliseconds outgrow a float's precision) */
	double seconds = (double)system_milliseconds() / 1000.0;
	short index;

	for (index = 0; index < count; index++)
	{
		struct bitmap_data *bitmap = picture(slides[index].bitmap);

		if (bitmap && bitmap->width > 0)
		{
			real slid = (real)fmod(seconds * slides[index].speed / bitmap->width, 1.0);

			picture_draw(bitmap, left, right, slides[index].y, left / bitmap->width - slid, TRUE,
				((pixel32)slides[index].alpha << 24) | slides[index].tint);
		}
	}
}

static boolean is_screen_cover(
	char const *name)
{
	short index;

	for (index = 0; index < NUMBEROF(screen_covers); index++)
	{
		if (!strcmp(name, screen_covers[index]))
			return TRUE;
	}
	return FALSE;
}

/* ---------- public code */

/* (ui_widget.c, after drawing a widget's picture widened to the window) */
void cairo_backdrop_render(
	long bitmap_tag_index)
{
	char const *name;
	real left = -(real)((halo_screen_width() - 640) / 2 + 1);
	real right = 640.0f - left;

	if (halo_menus_theme() != HALO_MENU_THEME_CAIRO || bitmap_tag_index == NONE)
		return;
	name = tag_get_name(bitmap_tag_index);
	if (!strcmp(name, MAIN_MENU_COVER))
	{
		slides_draw(main_menu_slides, NUMBEROF(main_menu_slides), left, right);
	}
	else if (is_screen_cover(name))
	{
		struct bitmap_data *band = picture(BAND);

		/* (the header band's picture begins at the 640 units' left) */
		if (band && left < 0.0f)
			picture_draw(band, left, 0.0f, BAND_TOP, 0.0f, FALSE, 0xFFFFFFFF);
		slides_draw(screen_slides, NUMBEROF(screen_slides), left, right);
	}
}
