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
// Tagged raiders glow this instead of hostile red, matching the arrow
#define DISTRESS_GLOW_COLOR (SDL_Color){ 255, 180, 60, 255 }
#define DRONE_GLOW_COLOR (SDL_Color){ 255, 190, 50, 255 }

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

/** Distance from pos to the nearest tagged raider (its position in
 *  *out); a huge value when none are left. */
static float nearest_raider(World *world, Vec2f pos, Vec2f *out) {
  float best = 1e30f;
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_DISTRESS | C_PIRATE | C_TRANSFORM)) continue;
    float d = vec2f_length(vec2f_sub(world->transforms[e].position, pos));
    if (d < best) {
      best = d;
      *out = world->transforms[e].position;
    }
  }
  return best;
}

/** Wild pirates that wander into the scene join the raider pack
 *  (quest marks keep their magenta identity and stay out of it). */
static void recruit_raiders(World *world, Vec2f center) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_PIRATE | C_TRANSFORM)) continue;
    if (entity_has(world, e, C_DISTRESS) ||
        entity_has(world, e, C_FROZEN) ||
        entity_has(world, e, C_QUEST_TARGET)) {
      continue;
    }
    if (vec2f_length(vec2f_sub(world->transforms[e].position, center)) <
        DISTRESS_SCENE_RADIUS) {
      world->masks[e] |= C_DISTRESS;
      world->wireframes[e].glow_color = DISTRESS_GLOW_COLOR;
    }
  }
}

/** Untag every scene member; surviving raiders rejoin the wild. */
static void strip_tags(World *world) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_DISTRESS)) continue;
    world->masks[e] &= ~C_DISTRESS;
    if (entity_has(world, e, C_PIRATE)) {
      world->wireframes[e].glow_color =
          world->pirates[e].archetype == PIRATE_DRONE ? DRONE_GLOW_COLOR
                                                      : PIRATE_GLOW_COLOR;
    }
  }
}

static void abandon(Distress *d, World *world) {
  strip_tags(world);
  events_emit(EV_DISTRESS_ABANDONED, d->pos);
  go_idle(d, world);
}

/** A boxy tan hauler adrift at pos, nose along a slow drift heading. */
static Entity freighter_spawn(World *world, Vec2f pos) {
  const float s = 26.0f;
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_COLLIDER | C_DISTRESS | C_FREIGHTER;
  world->freighters[e] = (Freighter){ .hp = FREIGHTER_HP };

  float heading = world_randf(world) * 360.0f;
  world->transforms[e] = (Transform){ .position = pos, .angle = heading };
  world->velocities[e] = (Velocity){
    .value = vec2f_mul(vec2f_dir(DEG_TO_RAD(heading)), 14.0f),
    .damping = 0.5f,  // raider bumps bleed off instead of accumulating
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

/** Ring the scene with tagged, amber-glowing raiders. */
static void spawn_raiders(World *world, Vec2f center, int count,
                          const PirateArchetype *types) {
  for (int i = 0; i < count; i++) {
    float a = world_randf(world) * TWO_PI;
    float r = 120.0f + world_randf(world) * 80.0f;
    Entity p = pirate_spawn(world, vec2f_add(center, vec2f_mul(vec2f_dir(a), r)),
                            types[i]);
    world->masks[p] |= C_DISTRESS;
    world->wireframes[p].glow_color = DISTRESS_GLOW_COLOR;
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
  d->visited = false;
  d->pos = pos;
  d->hull = FREIGHTER_HP;
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

/** Break the hauler apart: debris and loose cargo. */
static void freighter_break(World *world, Entity freighter) {
  Vec2f pos = world->transforms[freighter].position;
  debris_burst(world, pos, world->velocities[freighter].value,
               16 + world_rand(world, 6), FREIGHTER_CHIP_COLOR, 1.6f);
  scrap_scatter(world, pos, world->velocities[freighter].value,
                DISTRESS_WRECK_SCRAP);
  entity_destroy(world, freighter);
}

void freighter_hit(World *world, Entity e, int damage, Vec2f impact,
                   bool by_player) {
  Freighter *fr = &world->freighters[e];
  fr->hp -= (float)damage;
  if (by_player) fr->player_damage += (float)damage;
  if (fr->hp > 0.0f) {
    world->wireframes[e].flash = 0.07f;
    debris_burst(world, impact, world->velocities[e].value,
                 1 + damage, FREIGHTER_CHIP_COLOR, 1.0f);
    return;
  }

  // Blame is a share judgment, not a last-hit one
  bool blame = fr->player_damage >= FREIGHTER_BLAME_DAMAGE;
  Vec2f pos = world->transforms[e].position;
  freighter_break(world, e);
  // The state machine notices the missing hauler and cleans up
  events_emit(blame ? EV_FREIGHTER_KILLED : EV_DISTRESS_LOST, pos);
}

/** Raiders cleared with the freighter alive: pay out and send it home. */
static void rescue(Distress *d, World *world, Entity player, Entity freighter) {
  world->players[player].money +=
      DISTRESS_REWARD_BASE + world_rand(world, DISTRESS_REWARD_SPREAD);
  scrap_scatter(world, world->transforms[freighter].position,
                world->velocities[freighter].value, DISTRESS_RESCUE_SCRAP);

  // Untagged = departing; system_freighters steers it off and
  // despawns it once out of view
  float heading = world_randf(world) * 360.0f;
  world->transforms[freighter].angle = heading;
  world->velocities[freighter].value =
      vec2f_mul(vec2f_dir(DEG_TO_RAD(heading)), FREIGHTER_FLEE_SPEED);
  world->masks[freighter] &= ~C_DISTRESS;

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

  case DISTRESS_CALLED:
  case DISTRESS_FIGHT: {
    Entity f = MAX_ENTITIES;
    if (!d->ambush) {
      f = find_freighter(world);
      if (f == MAX_ENTITIES) {
        // Broken by someone (freighter_hit spoke), or stale after a load
        strip_tags(world);
        go_idle(d, world);
        return;
      }
      d->pos = world->transforms[f].position;
    }

    recruit_raiders(world, d->pos);
    d->raiders = count_raiders(world);

    // Rescue counts from any range: what matters is the raiders die
    // while the hauler still flies
    if (d->raiders == 0) {
      if (d->ambush) {
        world->players[player].money += DISTRESS_AMBUSH_REWARD;
        events_emit(EV_DISTRESS_CLEARED, d->pos);
        go_idle(d, world);
      } else {
        rescue(d, world, player, f);
      }
      return;
    }

    Vec2f raider_pos = d->pos;
    float raider_d = nearest_raider(world, ppos, &raider_pos);

    float pd = vec2f_length(vec2f_sub(d->pos, ppos));
    if (d->state == DISTRESS_CALLED) {
      // The trap also springs when the hunting pack reaches the player
      if (pd < DISTRESS_SCENE_RADIUS ||
          (d->ambush && raider_d < PIRATE_SENSE_RADIUS)) {
        d->state = DISTRESS_FIGHT;
        d->visited = true;
        events_emit(d->ambush ? EV_DISTRESS_AMBUSH : EV_DISTRESS_ARRIVED,
                    d->pos);
        return;
      }

      if (d->visited && pd > DISTRESS_LEASH) {
        abandon(d, world);
        return;
      }

      // Unchallenged raiders chew the hull (a phantom one on bait,
      // so the readout doesn't give the lie away)
      if (d->ambush) {
        d->hull -= DISTRESS_HULL_DRAIN * dt;
        if (d->hull <= 0.0f) {
          strip_tags(world);
          events_emit(EV_DISTRESS_LOST, d->pos);
          go_idle(d, world);
        }
        return;
      }

      Freighter *fr = &world->freighters[f];
      fr->hp -= DISTRESS_HULL_DRAIN * dt;
      d->hull = fr->hp;
      if (fr->hp <= 0.0f) {
        freighter_hit(world, f, 0, d->pos, false);  // emits per blame share
        strip_tags(world);
        go_idle(d, world);
        return;
      }

      d->spark_timer -= dt;
      if (d->spark_timer <= 0.0f && !entity_has(world, f, C_FROZEN)) {
        d->spark_timer = 0.7f;
        debris_burst(world, d->pos, world->velocities[f].value,
                     2 + world_rand(world, 2), FREIGHTER_CHIP_COLOR, 0.8f);
        world->wireframes[f].flash = 0.06f;
      }
      return;
    }

    // FIGHT: the drain is paused while the player is on scene
    if (d->ambush) {
      d->pos = raider_pos;  // compass tracks the pack
      if (raider_d > DISTRESS_LEASH) abandon(d, world);
      return;
    }

    d->hull = world->freighters[f].hp;
    if (pd > DISTRESS_LEASH) {
      abandon(d, world);
    } else if (pd > DISTRESS_SCENE_RADIUS * 1.5f) {
      d->state = DISTRESS_CALLED;  // off scene: the drain resumes
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
