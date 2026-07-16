// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "distress.h"
#include "events.h"
#include "particles.h"
#include "pirate.h"
#include "scrap.h"
#include "station.h"

// Warm tan sparks as raider fire chews the freighter's hull
#define FREIGHTER_CHIP_COLOR (SDL_Color){ 240, 205, 150, 255 }

void distress_reset(Distress *d) {
  SDL_memset(d, 0, sizeof(*d));
  d->roll_timer = DISTRESS_FIRST_DELAY;
}

static void go_idle(Distress *d, World *world) {
  d->state = DISTRESS_IDLE;
  d->roll_timer = DISTRESS_INTERVAL_MIN +
                  world_randf(world) * DISTRESS_INTERVAL_SPREAD;
}

/** The freighter (tagged, non-pirate), or MAX_ENTITIES when gone. */
static Entity find_freighter(World *world) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (entity_has(world, e, C_DISTRESS | C_TRANSFORM) &&
        !entity_has(world, e, C_PIRATE)) {
      return e;
    }
  }
  return MAX_ENTITIES;
}

static int count_raiders(World *world) {
  int n = 0;
  for (Entity e = 0; e < world->high_water; e++) {
    if (entity_has(world, e, C_DISTRESS | C_PIRATE)) n++;
  }
  return n;
}

/** Untag every scene member; surviving raiders rejoin the wild. */
static void strip_tags(World *world) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (entity_has(world, e, C_DISTRESS)) world->masks[e] &= ~C_DISTRESS;
  }
}

/** A boxy tan hauler adrift at pos, nose along a slow drift heading. */
static Entity freighter_spawn(World *world, Vec2f pos) {
  const float s = 26.0f;
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_COLLIDER | C_DISTRESS;

  float heading = world_randf(world) * 360.0f;
  world->transforms[e] = (Transform){ .position = pos, .angle = heading };
  world->velocities[e] = (Velocity){
    .value = vec2f_mul(vec2f_dir(DEG_TO_RAD(heading)), 14.0f),
    .damping = 0.0f,
  };
  world->colliders[e] = (Collider){ .radius = s * 0.8f };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(-s * 0.35f, -s);
  wf->points[1] = vec2f_new(s * 0.35f, -s);
  wf->points[2] = vec2f_new(s * 0.55f, -s * 0.55f);
  wf->points[3] = vec2f_new(s * 0.55f, s * 0.6f);
  wf->points[4] = vec2f_new(s * 0.8f, s * 0.8f);
  wf->points[5] = vec2f_new(s * 0.8f, s * 1.05f);
  wf->points[6] = vec2f_new(-s * 0.8f, s * 1.05f);
  wf->points[7] = vec2f_new(-s * 0.8f, s * 0.8f);
  wf->points[8] = vec2f_new(-s * 0.55f, s * 0.6f);
  wf->points[9] = vec2f_new(-s * 0.55f, -s * 0.55f);
  wf->point_count = 10;
  wf->color      = (SDL_Color){ 225, 200, 150, 255 };
  wf->glow_color = (SDL_Color){ 230, 200, 140, 255 };

  return e;
}

/** Ring the scene with tagged raiders. */
static void spawn_raiders(World *world, Vec2f center, int count,
                          const PirateArchetype *types) {
  for (int i = 0; i < count; i++) {
    float a = world_randf(world) * TWO_PI;
    float r = 120.0f + world_randf(world) * 80.0f;
    Entity p = pirate_spawn(world, vec2f_add(center, vec2f_mul(vec2f_dir(a), r)),
                            types[i]);
    world->masks[p] |= C_DISTRESS;
  }
}

/** Place the scene off the player's position and populate it. */
static void start_call(Distress *d, World *world, Entity player) {
  Vec2f ppos = world->transforms[player].position;
  Vec2f dir = vec2f_dir(world_randf(world) * TWO_PI);
  float dist = DISTRESS_DIST_MIN + world_randf(world) * DISTRESS_DIST_SPREAD;
  Vec2f pos = vec2f_add(ppos, vec2f_mul(dir, dist));

  // Keep the scene out of sanctuaries, or the raiders instantly flee
  Entity st = station_nearest(world, pos);
  if (st != MAX_ENTITIES &&
      vec2f_length(vec2f_sub(pos, world->transforms[st].position)) <
          STATION_SAFE_ZONE + 200.0f) {
    pos = vec2f_add(pos, vec2f_mul(dir, 900.0f));
  }

  d->ambush = world_randf(world) < DISTRESS_AMBUSH_CHANCE;
  d->pos = pos;
  d->timer = DISTRESS_TIMER;
  d->spark_timer = 0.0f;
  d->state = DISTRESS_CALLED;

  if (d->ambush) {
    const PirateArchetype pack[4] = { PIRATE_DART, PIRATE_DART,
                                      PIRATE_DRONE, PIRATE_DRONE };
    spawn_raiders(world, pos, 4, pack);
  } else {
    freighter_spawn(world, pos);
    const PirateArchetype pack[3] = { PIRATE_DART, PIRATE_DART, PIRATE_DART };
    spawn_raiders(world, pos, 3, pack);
  }

  events_emit(EV_DISTRESS_CALL, pos);
}

/** The raiders won: the freighter breaks up, leaving thin salvage. */
static void wreck(Distress *d, World *world, Entity freighter) {
  if (freighter != MAX_ENTITIES) {
    Vec2f pos = world->transforms[freighter].position;
    debris_burst(world, pos, world->velocities[freighter].value,
                 16 + world_rand(world, 6), FREIGHTER_CHIP_COLOR, 1.6f);
    scrap_scatter(world, pos, vec2f_new(0.0f, 0.0f), DISTRESS_WRECK_SCRAP);
    entity_destroy(world, freighter);
  }
  strip_tags(world);
  events_emit(EV_DISTRESS_LOST, d->pos);
  go_idle(d, world);
}

/** Raiders cleared with the freighter alive: pay out and send it home. */
static void rescue(Distress *d, World *world, Entity player, Entity freighter) {
  world->players[player].money +=
      DISTRESS_REWARD_BASE + world_rand(world, DISTRESS_REWARD_SPREAD);
  scrap_scatter(world, world->transforms[freighter].position,
                world->velocities[freighter].value, DISTRESS_RESCUE_SCRAP);

  // It burns for the horizon and quietly despawns far off-screen
  float heading = world_randf(world) * 360.0f;
  world->transforms[freighter].angle = heading;
  world->velocities[freighter].value =
      vec2f_mul(vec2f_dir(DEG_TO_RAD(heading)), 190.0f);
  world->masks[freighter] &= ~C_DISTRESS;
  world->masks[freighter] |= C_LIFETIME;
  world->lifetimes[freighter] = (Lifetime){ .remaining = 30.0f,
                                            .initial = 30.0f };

  events_emit(EV_DISTRESS_SAVED, d->pos);
  go_idle(d, world);
}

void distress_update(Distress *d, World *world, Entity player, float dt) {
  if (!entity_has(world, player, C_PLAYER | C_TRANSFORM)) return;
  Vec2f ppos = world->transforms[player].position;

  switch (d->state) {
  case DISTRESS_IDLE:
    if (station_docked(world, player)) return;  // no maydays while shopping
    d->roll_timer -= dt;
    if (d->roll_timer <= 0.0f) start_call(d, world, player);
    return;

  case DISTRESS_CALLED: {
    Entity f = MAX_ENTITIES;
    if (!d->ambush) {
      f = find_freighter(world);
      if (f == MAX_ENTITIES) {  // stale state (e.g. after a load)
        strip_tags(world);
        go_idle(d, world);
        return;
      }
      d->pos = world->transforms[f].position;

      // Raider fire chews the hull while anyone is watching
      d->spark_timer -= dt;
      if (d->spark_timer <= 0.0f && !entity_has(world, f, C_FROZEN)) {
        d->spark_timer = 0.7f;
        debris_burst(world, d->pos, world->velocities[f].value,
                     2 + world_rand(world, 2), FREIGHTER_CHIP_COLOR, 0.8f);
        world->wireframes[f].flash = 0.06f;
      }
    }

    if (vec2f_length(vec2f_sub(d->pos, ppos)) < DISTRESS_SCENE_RADIUS) {
      d->state = DISTRESS_FIGHT;  // the raiders have a new problem
      events_emit(d->ambush ? EV_DISTRESS_AMBUSH : EV_DISTRESS_ARRIVED,
                  d->pos);
      return;
    }

    d->timer -= dt;
    if (d->timer <= 0.0f) wreck(d, world, f);
    return;
  }

  case DISTRESS_FIGHT: {
    Entity f = MAX_ENTITIES;
    if (!d->ambush) {
      f = find_freighter(world);
      if (f == MAX_ENTITIES) {
        strip_tags(world);
        go_idle(d, world);
        return;
      }
      d->pos = world->transforms[f].position;
    }

    float pd = vec2f_length(vec2f_sub(d->pos, ppos));
    if (!d->ambush && pd > DISTRESS_SCENE_RADIUS * 1.5f) {
      d->state = DISTRESS_CALLED;  // walked away: the clock resumes
      return;
    }
    if (d->ambush && pd > DISTRESS_LEASH) {
      strip_tags(world);  // bait declined; the pack melts into the field
      go_idle(d, world);
      return;
    }

    if (count_raiders(world) == 0) {
      if (d->ambush) {
        world->players[player].money += DISTRESS_AMBUSH_REWARD;
        events_emit(EV_DISTRESS_CLEARED, d->pos);
        go_idle(d, world);
      } else {
        rescue(d, world, player, f);
      }
    }
    return;
  }
  }
}

bool distress_compass_target(const Distress *d, Vec2f *out) {
  if (d->state == DISTRESS_IDLE) return false;
  *out = d->pos;
  return true;
}
