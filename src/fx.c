// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL3/SDL.h>
#include "fx.h"

static float shake_mag = 0.0f;

void fx_shake(float amount) {
  if (amount > shake_mag) shake_mag = amount;
}

Vec2f fx_shake_offset(float dt) {
  if (shake_mag <= 0.0f) return vec2f_new(0.0f, 0.0f);

  shake_mag -= shake_mag * 5.0f * dt;  // exponential settle
  if (shake_mag < 0.15f) {
    shake_mag = 0.0f;
    return vec2f_new(0.0f, 0.0f);
  }

  return vec2f_new((SDL_randf() * 2.0f - 1.0f) * shake_mag,
                   (SDL_randf() * 2.0f - 1.0f) * shake_mag);
}
