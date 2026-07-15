// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Bloom via render targets (SDL_Renderer has no shader hook).
 *
 *  Glow sources draw into an offscreen texture, which is blurred by
 *  walking down a chain of half-size targets (each 2x linear downscale
 *  is a box blur) and accumulating back up. Compositing happens from
 *  the 1/2-res level so the final upscale is only 2x — no blockiness.
 *
 *  Per frame, between background and entity rendering:
 *    glow_begin(); ...draw glow sources...; glow_end();
 */

#ifndef _SD_GLOW_H
#define _SD_GLOW_H
#include <stdbool.h>
#include <SDL3/SDL.h>

#define GLOW_LEVELS 4  /**< Full, 1/2, 1/4, 1/8 resolution. */

typedef struct Glow {
  SDL_Texture *levels[GLOW_LEVELS];
} Glow;

/** Create the render-target chain. @return false on failure. */
bool glow_init(Glow *glow, SDL_Renderer *renderer, int w, int h);

/** Redirect rendering into the (cleared) glow source buffer. */
void glow_begin(Glow *glow, SDL_Renderer *renderer);

/** Blur the sources and composite them additively onto restore_target
 *  (the scene texture, or NULL for the backbuffer). */
void glow_end(Glow *glow, SDL_Renderer *renderer, SDL_Texture *restore_target);

void glow_destroy(Glow *glow);

#endif
