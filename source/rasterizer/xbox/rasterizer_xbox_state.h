/*
RASTERIZER_XBOX_STATE.H

The Xbox rasterizer's state, for files that don't use rasterizer_xbox.h's
Direct3D declarations.
*/

#ifndef __RASTERIZER_XBOX_STATE_H
#define __RASTERIZER_XBOX_STATE_H
#pragma once

#include "cseries.h"

void rasterizer_set_stencil_mode(
	long stencil_mode);
void rasterizer_profile_begin(
	short profile);
void rasterizer_profile_end(
	short profile);

#endif /* __RASTERIZER_XBOX_STATE_H */
