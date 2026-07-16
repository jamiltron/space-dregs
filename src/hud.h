// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  In-game HUD: stats, compasses, dock panel and quest board.
 */

#ifndef _SD_HUD_H
#define _SD_HUD_H
#include <SDL3/SDL.h>
#include "distress.h"
#include "ecs.h"
#include "quest.h"

/** Draw the full HUD for a living player; no-op while dead.
 *  quest_board switches the docked panel to the contract board. */
void hud_render(World *world, Entity player, const Quest *quest,
                const Distress *distress, bool quest_board,
                SDL_Renderer *renderer);

#endif
