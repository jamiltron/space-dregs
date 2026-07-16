// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "faction.h"
#include "events.h"
#include "pirate.h"
#include "station.h"

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
  Factions *f = &world->factions;
  int before = f->standing[id];
  int s = before + delta;
  if (s > FACTION_STANDING_MAX) s = FACTION_STANDING_MAX;
  if (s < -FACTION_STANDING_MAX) s = -FACTION_STANDING_MAX;
  f->standing[id] = s;

  if (s != before) {
    // A change while one is still displayed merges into a single total
    f->delta[id] = (f->delta_timer[id] > 0.0f ? f->delta[id] : 0) + (s - before);
    f->delta_timer[id] = f->delta[id] != 0 ? FACTION_DELTA_SECS : 0.0f;
  }

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

static void spawn_hunters(World *world, Vec2f ppos) {
  Vec2f dir = vec2f_dir(world_randf(world) * TWO_PI);
  float dist = FACTION_HUNTER_DIST_MIN +
               world_randf(world) * FACTION_HUNTER_DIST_SPREAD;
  Vec2f center = vec2f_add(ppos, vec2f_mul(dir, dist));

  PirateArchetype pack[4] = { PIRATE_DART, PIRATE_DART };
  int count = 2;
  if (world->factions.heat >= FACTION_HEAT_TIER2) pack[count++] = PIRATE_BRUTE;
  if (world->factions.heat >= FACTION_HEAT_TIER3) {
    pack[count++] = PIRATE_BATTLESHIP;
  }

  for (int i = 0; i < count; i++) {
    float a = world_randf(world) * TWO_PI;
    Entity p = pirate_spawn(
        world, vec2f_add(center, vec2f_mul(vec2f_dir(a), 90.0f)), pack[i]);
    world->masks[p] |= C_HUNTER;
    world->pirates[p].provoked = true;
  }

  events_emit(EV_HUNTERS_INBOUND, center);
}

void faction_update(World *world, Entity player, float dt) {
  Factions *f = &world->factions;

  f->heat -= FACTION_HEAT_DECAY * dt;
  if (f->heat < 0.0f) f->heat = 0.0f;

  for (int i = 0; i < FACTION_COUNT; i++) {
    if (f->delta_timer[i] > 0.0f) f->delta_timer[i] -= dt;
  }

  if (!entity_has(world, player, C_PLAYER | C_TRANSFORM)) return;
  Vec2f ppos = world->transforms[player].position;

  bool live_pack = false;
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_HUNTER | C_PIRATE | C_TRANSFORM)) continue;

    float d = vec2f_length(vec2f_sub(world->transforms[e].position, ppos));
    if (d > FACTION_HUNTER_LEASH) {
      world->masks[e] &= ~C_HUNTER;
    } else {
      live_pack = true;
    }
  }

  bool eligible = !live_pack && f->heat >= FACTION_HEAT_HUNT &&
                  !station_docked(world, player);
  if (!eligible) {
    f->hunter_timer = -1.0f;  // re-rolled on the next eligible frame
    return;
  }
  if (f->hunter_timer < 0.0f) {
    f->hunter_timer = FACTION_HUNTER_DELAY_MIN +
                      world_randf(world) * FACTION_HUNTER_DELAY_SPREAD;
    return;
  }

  f->hunter_timer -= dt;
  if (f->hunter_timer <= 0.0f) {
    spawn_hunters(world, ppos);
    f->hunter_timer = -1.0f;
  }
}

void faction_on_pirate_kill(World *world, int archetype, bool was_hunter) {
  faction_add(world, FACTION_CLANS, -FACTION_KILL_STANDING);
  if (was_hunter) return;

  bool capital = archetype == PIRATE_BATTLESHIP ||
                 archetype == PIRATE_MOTHERSHIP;
  world->factions.heat += capital ? FACTION_CAPITAL_HEAT : FACTION_KILL_HEAT;
}
