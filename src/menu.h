// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Title menu and options screens (key rebinding, volumes,
 *  fullscreen). The caller routes keydown events here while in
 *  STATE_MENU and acts on the returned result.
 */

#ifndef _SD_MENU_H
#define _SD_MENU_H
#include <SDL3/SDL.h>

typedef enum MenuResult {
  MENU_NONE,      /**< Nothing for the caller to do. */
  MENU_START,     /**< Begin a new run. */
  MENU_CONTINUE,  /**< Load the saved run. */
  MENU_QUIT,
} MenuResult;

/** Remember the window (for the fullscreen toggle). */
void menu_init(SDL_Window *window);

/** Reset to the main screen. */
void menu_open(void);

/** Handle one keydown. */
MenuResult menu_handle_key(SDL_Scancode key);

/** Draw the menu over the (dimmed) world. */
void menu_render(SDL_Renderer *renderer);

#endif
