// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Runtime input map: game actions bound to scancodes.
 *
 *  Each action has a rebindable primary key and an optional fixed
 *  alternate (WASD). Menu navigation keys are not remappable.
 */

#ifndef _SD_INPUT_H
#define _SD_INPUT_H
#include <stdbool.h>
#include <SDL3/SDL.h>

typedef enum InputAction {
  ACT_LEFT,
  ACT_RIGHT,
  ACT_THRUST,
  ACT_FIRE,
  ACT_REFUEL,
  ACT_REPAIR,
  ACT_PAY_DEBT,
  ACT_MINE,
  ACT_MISSILE,
  ACT_COUNT
} InputAction;

/** Load the default bindings. */
void input_init(void);

/** True while any key bound to the action is held (polls SDL). */
bool input_down(InputAction a);

/** The action's primary (rebindable) scancode. */
SDL_Scancode input_get(InputAction a);

/** Rebind the action's primary scancode. */
void input_set(InputAction a, SDL_Scancode key);

/** Display name for menus ("THRUST", ...). */
const char *input_label(InputAction a);

#endif
