// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Screen shake: impacts kick the camera, decaying exponentially.
 */

#ifndef _SD_FX_H
#define _SD_FX_H
#include "math2d.h"

/** Report an impact; the strongest pending kick wins (px). */
void fx_shake(float amount);

/** Decay the shake and return this frame's camera jitter offset. */
Vec2f fx_shake_offset(float dt);

#endif
