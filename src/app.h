// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Top-level application state and the logical window size.
 */

#ifndef _SD_APP_H
#define _SD_APP_H
#include <SDL3/SDL.h>
#include <stdbool.h>
#include "chunks.h"
#include "distress.h"
#include "ecs.h"
#include "glow.h"
#include "quest.h"
#include "starfield.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

typedef enum GameState {
  STATE_MENU,
  STATE_PLAYING,
  STATE_PAUSED,
  STATE_GAME_OVER,
} GameState;

/** Everything the frame loop touches. One global instance in main.c. */
typedef struct App {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *scene;  /**< Supersampled frame target, downsampled to screen. */
  World world;
  Chunks chunks;
  Quest quest;
  Distress distress;
  Starfield starfield;
  Glow glow;
  GameState state;
  Entity player;
  Vec2f camera;       
  float time;         /**< Accumulated unpaused game time. */
  float death_timer;  /**< Explosion settle time before the game over screen. */
  bool quest_board;   /**< Docked quest-board panel is open. */
  bool running;
} App;

#endif
