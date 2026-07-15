// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Parallax starfield background.
 *
 *  Stars are fixed in field space with a slow flicker; they shift
 *  against the camera by a per-layer parallax factor (closer =
 *  brighter = shifts more) and wrap modulo the screen, so the field
 *  is endless and camera motion is what sells movement.
 */

#ifndef _SD_STARFIELD_H
#define _SD_STARFIELD_H
#include <SDL3/SDL.h>
#include "math2d.h"

#define STAR_COUNT 96
#define STAR_LAYERS 3

typedef struct Star {
  Vec2f position;       /**< Field space. */
  float parallax;       /**< Fraction of camera motion applied. */
  float flicker_speed;  /**< Hz. */
  float flicker_phase;
  SDL_Color color;      /**< Alpha encodes layer brightness. */
} Star;

typedef struct Starfield {
  Star stars[STAR_COUNT];
  float bounds_w;
  float bounds_h;
} Starfield;

/** Scatter the stars across the given screen bounds. */
void starfield_init(Starfield *sf, float bounds_w, float bounds_h);

/** Draw the field for the given camera position. */
void starfield_render(const Starfield *sf, SDL_Renderer *renderer, Vec2f camera);

#endif
