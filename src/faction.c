// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "faction.h"
#include "events.h"
#include "pirate.h"

/** Dialogue beats when a standing crosses a tier line. */
static void threshold_events(FactionId id, int before, int after) {
  Vec2f none = { 0 };
  if (id == FACTION_GUILD) {
    if (before < FACTION_TIER && after >= FACTION_TIER) {
      events_emit(EV_GUILD_RESPECTED, none);
    } else if (before > -FACTION_TIER && after <= -FACTION_TIER) {
      events_emit(EV_GUILD_OUTCAST, none);
    }
  } else if (id == FACTION_CLANS) {
    if (before < FACTION_TIER && after >= FACTION_TIER) {
      events_emit(EV_CLANS_RESPECTED, none);
    }
  }
}

void faction_add(World *world, FactionId id, int delta) {
  int before = world->factions.standing[id];
  int s = before + delta;
  if (s > FACTION_STANDING_MAX) s = FACTION_STANDING_MAX;
  if (s < -FACTION_STANDING_MAX) s = -FACTION_STANDING_MAX;
  world->factions.standing[id] = s;
  threshold_events(id, before, s);
}

float faction_price_scale(const World *world) {
  int g = world->factions.standing[FACTION_GUILD];
  if (g >= FACTION_TIER_STRONG) return 0.8f;
  if (g >= FACTION_TIER) return 0.9f;
  if (g <= -FACTION_TIER_STRONG) return 2.0f;
  if (g <= -FACTION_TIER) return 1.5f;
  return 1.0f;
}

bool faction_board_open(const World *world) {
  return world->factions.standing[FACTION_GUILD] > -FACTION_TIER;
}

float faction_sense_scale(const World *world) {
  int c = world->factions.standing[FACTION_CLANS];
  if (c >= FACTION_TIER * 2) return 0.0f;
  if (c >= FACTION_TIER) return 0.5f;
  return 1.0f;
}

void faction_on_pirate_kill(World *world, int archetype, bool was_hunter) {
  faction_add(world, FACTION_CLANS, -FACTION_KILL_STANDING);
  if (was_hunter) return;

  bool capital = archetype == PIRATE_BATTLESHIP ||
                 archetype == PIRATE_MOTHERSHIP;
  world->factions.heat += capital ? FACTION_CAPITAL_HEAT : FACTION_KILL_HEAT;
}
