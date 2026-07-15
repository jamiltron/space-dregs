// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "glow.h"

#define GLOW_STRENGTH 110  // composite alpha, 0-255

static SDL_Texture *make_target(SDL_Renderer *renderer, int w, int h) {
  SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, w, h);
  if (tex) {
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
  }
  return tex;
}

bool glow_init(Glow *glow, SDL_Renderer *renderer, int w, int h) {
  for (int i = 0; i < GLOW_LEVELS; i++) {
    glow->levels[i] = make_target(renderer, w >> i, h >> i);
    if (!glow->levels[i]) {
      glow_destroy(glow);
      return false;
    }
  }
  return true;
}

void glow_begin(Glow *glow, SDL_Renderer *renderer) {
  SDL_SetRenderTarget(renderer, glow->levels[0]);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
}

void glow_end(Glow *glow, SDL_Renderer *renderer,
              SDL_Texture *restore_target) {
  for (int i = 1; i < GLOW_LEVELS; i++) {
    SDL_SetTextureBlendMode(glow->levels[i - 1], SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer, glow->levels[i]);
    SDL_RenderTexture(renderer, glow->levels[i - 1], NULL, NULL);
  }

  for (int i = GLOW_LEVELS - 1; i >= 2; i--) {
    SDL_SetTextureBlendMode(glow->levels[i], SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(renderer, glow->levels[i - 1]);
    SDL_RenderTexture(renderer, glow->levels[i], NULL, NULL);
  }

  SDL_SetRenderTarget(renderer, restore_target);
  SDL_SetTextureBlendMode(glow->levels[1], SDL_BLENDMODE_ADD);
  SDL_SetTextureAlphaMod(glow->levels[1], GLOW_STRENGTH);
  SDL_RenderTexture(renderer, glow->levels[1], NULL, NULL);
  SDL_SetTextureAlphaMod(glow->levels[1], 255);
}

void glow_destroy(Glow *glow) {
  for (int i = 0; i < GLOW_LEVELS; i++) {
    if (glow->levels[i]) {
      SDL_DestroyTexture(glow->levels[i]);
      glow->levels[i] = NULL;
    }
  }
}
