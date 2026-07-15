// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input.h"

typedef struct Binding {
  SDL_Scancode primary;  // rebindable
  SDL_Scancode alt;      // fixed convenience alternate
} Binding;

static Binding bindings[ACT_COUNT];

static const char *LABELS[ACT_COUNT] = {
  [ACT_LEFT]   = "TURN LEFT",
  [ACT_RIGHT]  = "TURN RIGHT",
  [ACT_THRUST] = "THRUST",
  [ACT_FIRE]   = "FIRE",
  [ACT_REFUEL] = "REFUEL",
  [ACT_REPAIR] = "REPAIR",
  [ACT_PAY_DEBT] = "PAY DEBT",
  [ACT_MINE]   = "DROP MINE",
  [ACT_MISSILE] = "FIRE MISSILE",
};

void input_init(void) {
  bindings[ACT_LEFT]   = (Binding){ SDL_SCANCODE_LEFT, SDL_SCANCODE_A };
  bindings[ACT_RIGHT]  = (Binding){ SDL_SCANCODE_RIGHT, SDL_SCANCODE_D };
  bindings[ACT_THRUST] = (Binding){ SDL_SCANCODE_UP, SDL_SCANCODE_W };
  bindings[ACT_FIRE]   = (Binding){ SDL_SCANCODE_SPACE, SDL_SCANCODE_UNKNOWN };
  bindings[ACT_REFUEL] = (Binding){ SDL_SCANCODE_F, SDL_SCANCODE_UNKNOWN };
  bindings[ACT_REPAIR] = (Binding){ SDL_SCANCODE_R, SDL_SCANCODE_UNKNOWN };
  bindings[ACT_PAY_DEBT] = (Binding){ SDL_SCANCODE_P, SDL_SCANCODE_UNKNOWN };
  bindings[ACT_MINE]   = (Binding){ SDL_SCANCODE_X, SDL_SCANCODE_UNKNOWN };
  bindings[ACT_MISSILE] = (Binding){ SDL_SCANCODE_LSHIFT, SDL_SCANCODE_UNKNOWN };
}

bool input_down(InputAction a) {
  const bool *keystate = SDL_GetKeyboardState(NULL);
  const Binding *b = &bindings[a];
  return keystate[b->primary] ||
         (b->alt != SDL_SCANCODE_UNKNOWN && keystate[b->alt]);
}

SDL_Scancode input_get(InputAction a) { return bindings[a].primary; }

void input_set(InputAction a, SDL_Scancode key) { bindings[a].primary = key; }

const char *input_label(InputAction a) { return LABELS[a]; }
