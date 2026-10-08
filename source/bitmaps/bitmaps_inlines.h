/*
BITMAPS_INLINES.H

file has inline function assertions.
*/

#ifndef __BITMAPS_INLINES_H
#define __BITMAPS_INLINES_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "bitmaps/bitmaps.h"
#include "math/integer_math.h"
#include "math/real_math.h"
#include "effects/particles.h"

/* ---------- constants */

/* ---------- macros */

#define match_assert_valid_real_rgb_color(file, line, rgb) \
	match_vassert( \
		file, \
		line, \
		valid_real_rgb_color(rgb), \
		csprintf( \
			temporary, \
			"%s: assert_valid_real_rgb_color(%f, %f, %f)", \
			#rgb, \
			(*rgb).red, \
			(*rgb).green, \
			(*rgb).blue))

/* ---------- structures */

/* ---------- prototypes/EXAMPLE.C */

/* ---------- globals */

/* ---------- public code */

__inline pixel32 real_argb_color_to_pixel32(
	real_argb_color const *color)
{
	real scale = 255.0f;
	pixel32 result;

	match_vassert(
		"..\\bitmaps\\bitmaps_inlines.h",
		89,
		valid_real_argb_color(color),
		csprintf(
			temporary,
			"%s: assert_valid_real_argb_color(%f, %f, %f, %f)",
			"color",
			color->alpha,
			color->red,
			color->green,
			color->blue));

	{
		long alpha;
		long red;
		long green;
		long blue;

		result = (pixel32)(
			(long)__builtin_rint((double)color->blue * scale) |
			((long)__builtin_rint((double)color->green * scale) << 8) |
			((long)__builtin_rint((double)color->red * scale) << 16) |
			((long)__builtin_rint((double)color->alpha * scale) << 24));
	}

	{
		pixel32 verify;

		verify = (pixel32)(
			((long)__builtin_rint((double)color->blue * scale) & 0xff) |
			(((long)__builtin_rint((double)color->green * scale) & 0xff) << 8) |
			(((long)__builtin_rint((double)color->red * scale) & 0xff) << 16) |
			((long)__builtin_rint((double)color->alpha * scale) << 24));

		match_assert(
			"..\\bitmaps\\bitmaps_inlines.h",
			188,
			verify == result);
	}

	return result;
}

__inline pixel32 real_rgb_color_to_pixel32(
	real_rgb_color const *color)
{
	pixel32 result;
	real scale = (real)UNSIGNED_CHAR_MAX;

	match_assert_valid_real_rgb_color("..\\bitmaps\\bitmaps_inlines.h", 0xC9, color);

	result = (pixel32)(
		((long)__builtin_rint((double)color->blue * scale) & 0xff) |
		(((long)__builtin_rint((double)color->green * scale) & 0xff) << 8) |
		(((long)__builtin_rint((double)color->red * scale) & 0xff) << 16));

	return result;
}

__inline pixel32 real_alpha_to_pixel32(
	real alpha)
{
	real scale = 255.0f;
	pixel32 result;

	match_assert(
		"..\\bitmaps\\bitmaps_inlines.h",
		291,
		alpha>=0.0f && alpha<=1.0f);

	result = (pixel32)((long)__builtin_rint((double)alpha * scale) << 24);

	return result;
}
#endif // __BITMAPS_INLINES_H
