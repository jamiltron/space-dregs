// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Dialogue bar: one typewriter-revealed line at the bottom of the
 *  screen. Purely presentation — it reacts to game events (docking,
 *  signals, milestones) and picks lines with presentation randomness,
 *  so it never affects simulation determinism. Lines use only the
 *  glyphs the segment font has (A-Z 0-9 dash slash plus).
 */

#ifndef _SD_DIALOGUE_H
#define _SD_DIALOGUE_H
#include <SDL3/SDL.h>
#include "events.h"

/** Load line pools from assets/dialogue.txt (falls back if missing). */
void dialogue_init(void);

/** React to a drained game event (called by events_drain). */
void dialogue_on_event(EventType type, Vec2f pos);

/** Say the next run-opening line (they cycle); debt fills the amount. */
void dialogue_run_start(int debt);

/** Advance the typewriter and expiry; call once per frame. */
void dialogue_update(float dt);

/** Draw the current line, if any. */
void dialogue_render(SDL_Renderer *renderer);

#endif
