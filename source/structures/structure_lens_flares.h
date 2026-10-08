/*
STRUCTURE_LENS_FLARES.H
*/

#ifndef __STRUCTURE_LENS_FLARES_H
#define __STRUCTURE_LENS_FLARES_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- structures */

struct structure_bsp;

struct structure_lens_flare
{
	struct tag_reference lens_flare;
};

typedef char structure_lens_flare_size_assert[
	sizeof(struct structure_lens_flare) == 0x10 ? 1 : -1];

struct structure_lens_flare_marker
{
	real_point3d position;
	char direction[3];
	byte lens_flare_index;
};

typedef char structure_lens_flare_marker_size_assert[
	sizeof(struct structure_lens_flare_marker) == 0x10 ? 1 : -1];

/* ---------- prototypes/STRUCTURE_LENS_FLARES.C */

boolean build_structure_lens_flares(
	struct structure_bsp *structure_bsp);
void structure_lens_flares_place(
	void);

#endif // __STRUCTURE_LENS_FLARES_H
