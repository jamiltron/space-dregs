// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Line/point drawing that survives the supersampled scene.
 *
 *  SDL primitives are one *device* pixel wide, so in the SCENE_SS
 *  buffer a plain line covers only 1/SS of a logical pixel and dims
 *  when downsampled. These helpers stroke twice with a half-pixel
 *  offset (full logical coverage) and draw points as 1x1 logical
 *  rects, keeping the vector art at full brightness at any scale.
 */

#ifndef _SD_DRAW_H
#define _SD_DRAW_H
#include <SDL3/SDL.h>

static inline void draw_line(SDL_Renderer *r, float x1, float y1,
                             float x2, float y2) {
  SDL_RenderLine(r, x1, y1, x2, y2);
  SDL_RenderLine(r, x1 + 0.5f, y1 + 0.5f, x2 + 0.5f, y2 + 0.5f);
}

static inline void draw_lines(SDL_Renderer *r, const SDL_FPoint *pts, int n) {
  for (int i = 0; i + 1 < n; i++) {
    draw_line(r, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y);
  }
}

static inline void draw_point(SDL_Renderer *r, float x, float y) {
  SDL_RenderFillRect(r, &(SDL_FRect){ x, y, 1.0f, 1.0f });
}

#endif
