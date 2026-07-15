// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include "starfield.h"
#include "draw.h"

/** Galaga-ish palette: mostly cool whites/blues, a few warm ones. */
static const SDL_Color STAR_COLORS[] = {
  { 255, 255, 255, 255 },
  { 200, 210, 255, 255 },
  { 150, 170, 255, 255 },
  { 255, 220, 150, 255 },
  { 255, 140, 140, 255 },
};
#define STAR_COLOR_COUNT (int)(sizeof(STAR_COLORS) / sizeof(STAR_COLORS[0]))

void starfield_init(Starfield *sf, float bounds_w, float bounds_h) {
  sf->bounds_w = bounds_w;
  sf->bounds_h = bounds_h;

  for (int i = 0; i < STAR_COUNT; i++) {
    Star *star = &sf->stars[i];
    int layer = i % STAR_LAYERS;

    star->position = vec2f_new(SDL_randf() * bounds_w, SDL_randf() * bounds_h);
    star->parallax = 0.25f + layer * 0.25f;            // deeper layers shift less
    star->flicker_speed = 0.1f + SDL_randf() * 0.25f;  // full cycle every ~3-10s
    star->flicker_phase = SDL_randf() * TWO_PI;
    star->color = STAR_COLORS[SDL_rand(STAR_COLOR_COUNT)];
    star->color.a = (Uint8)(90 + layer * 80);          // closer = brighter
  }
}

void starfield_render(const Starfield *sf, SDL_Renderer *renderer, Vec2f camera) {
  float time = SDL_GetTicks() / 1000.0f;

  for (int i = 0; i < STAR_COUNT; i++) {
    const Star *star = &sf->stars[i];

    // Parallax shift against the camera, wrapped into view
    float x = fmodf(star->position.x - camera.x * star->parallax, sf->bounds_w);
    float y = fmodf(star->position.y - camera.y * star->parallax, sf->bounds_h);
    if (x < 0.0f) x += sf->bounds_w;
    if (y < 0.0f) y += sf->bounds_h;

    // Slow flicker
    float pulse = 0.55f + 0.45f * sinf(TWO_PI * star->flicker_speed * time +
                                       star->flicker_phase);
    Uint8 alpha = (Uint8)(star->color.a * pulse);

    SDL_SetRenderDrawColor(renderer, star->color.r, star->color.g,
                           star->color.b, alpha);
    draw_point(renderer, x, y);
  }
}
