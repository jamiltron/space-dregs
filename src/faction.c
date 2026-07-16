// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "faction.h"
#include "pirate.h"

void faction_add(World *world, FactionId id, int delta) {
  int s = world->factions.standing[id] + delta;
  if (s > FACTION_STANDING_MAX) s = FACTION_STANDING_MAX;
  if (s < -FACTION_STANDING_MAX) s = -FACTION_STANDING_MAX;
  world->factions.standing[id] = s;
}

void faction_on_pirate_kill(World *world, int archetype, bool was_hunter) {
  faction_add(world, FACTION_CLANS, -FACTION_KILL_STANDING);
  if (was_hunter) return;

  bool capital = archetype == PIRATE_BATTLESHIP ||
                 archetype == PIRATE_MOTHERSHIP;
  world->factions.heat += capital ? FACTION_CAPITAL_HEAT : FACTION_KILL_HEAT;
}
