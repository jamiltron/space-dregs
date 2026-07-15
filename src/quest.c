// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "quest.h"
#include "events.h"
#include "pirate.h"
#include "station.h"

#define QUEST_PICKUP_DIST 26.0f
#define QUEST_COMPLETE_BANNER 3.0f

void quest_reset(Quest *q) { SDL_memset(q, 0, sizeof(*q)); }

/** The fetch objective: a big magenta diamond adrift in deep space. */
static void spawn_beacon(World *world, Vec2f pos) {
  Entity e = entity_create(world);
  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_QUEST_TARGET;

  world->transforms[e] = (Transform){ .position = pos,
                                      .angle = world_randf(world) * 360.0f };
  world->velocities[e] = (Velocity){ .damping = 0.0f, .spin = 45.0f };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -9.0f);
  wf->points[1] = vec2f_new(6.0f, 0.0f);
  wf->points[2] = vec2f_new(0.0f, 9.0f);
  wf->points[3] = vec2f_new(-6.0f, 0.0f);
  wf->point_count = 4;
  wf->color      = (SDL_Color){ 230, 140, 255, 255 };  // quest magenta
  wf->glow_color = (SDL_Color){ 230, 120, 255, 255 };
}

/** Create the active quest's objective entity at target_pos. */
static void spawn_target(Quest *q, World *world) {
  switch (q->type) {
  case QUEST_FETCH:
    if (!q->carrying) spawn_beacon(world, q->target_pos);
    break;
  case QUEST_BOUNTY: {
    Entity p = pirate_spawn(world, q->target_pos, PIRATE_DART);
    world->masks[p] |= C_QUEST_TARGET;
    world->pirates[p].hp = PIRATE_HP * 2;  // a name worth a contract
    world->wireframes[p].glow_color = (SDL_Color){ 230, 120, 255, 255 };
    break;
  }
  case QUEST_DELIVER: {
    Entity st = station_spawn(
        world, q->target_pos,
        station_palette(world_rand(world, STATION_PALETTE_COUNT)));
    world->masks[st] |= C_QUEST_TARGET;
    break;
  }
  default:
    break;
  }
}

/** The entity carrying C_QUEST_TARGET, or MAX_ENTITIES when gone. */
static Entity find_target(World *world) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (entity_has(world, e, C_QUEST_TARGET | C_TRANSFORM)) return e;
  }
  return MAX_ENTITIES;
}

static int reward_for(World *world, QuestType t) {
  switch (t) {
  case QUEST_FETCH:   return 150 + world_rand(world, 100);
  case QUEST_BOUNTY:  return 250 + world_rand(world, 150);
  case QUEST_DELIVER: return 200 + world_rand(world, 150);
  default:            return 0;
  }
}

/** The board always deals two contracts, each type drawn independently
 *  at random — duplicates allowed (two deliveries, different pay). */
static void roll_offers(Quest *q, World *world) {
  q->offer_count = 2;
  for (int i = 0; i < q->offer_count; i++) {
    QuestType t = (QuestType)(1 + world_rand(world, 3));
    q->offers[i] = t;
    q->offer_rewards[i] = reward_for(world, t);
  }
}

bool quest_try_accept(Quest *q, World *world, Entity player, int index) {
  if (q->type != QUEST_NONE) return false;
  if (index < 0 || index >= q->offer_count) return false;
  if (!station_docked(world, player)) return false;

  Vec2f base = world->transforms[player].position;
  Vec2f dir = vec2f_dir(world_randf(world) * TWO_PI);
  float dist = 0.0f;

  q->type = q->offers[index];
  q->reward = q->offer_rewards[index];
  q->offer_count = 0;
  q->carrying = false;

  switch (q->type) {
  case QUEST_FETCH:   dist = 2200.0f + world_randf(world) * 1800.0f; break;
  case QUEST_BOUNTY:  dist = 1800.0f + world_randf(world) * 1500.0f; break;
  case QUEST_DELIVER:
    dist = 2800.0f + world_randf(world) * 2200.0f;
    q->carrying = true;
    break;
  default:
    break;
  }

  q->target_pos = vec2f_add(base, vec2f_mul(dir, dist));
  spawn_target(q, world);
  events_emit(EV_QUEST_ACCEPTED, base);
  return true;
}

void quest_restore(Quest *q, World *world) {
  if (q->type != QUEST_NONE) spawn_target(q, world);
}

void quest_grant_bounty(Quest *q, World *world, Vec2f from) {
  if (q->type != QUEST_NONE) return;

  q->type = QUEST_BOUNTY;
  q->reward = 300 + world_rand(world, 200);  // pays over board bounties
  q->carrying = false;
  q->offer_count = 0;

  Vec2f dir = vec2f_dir(world_randf(world) * TWO_PI);
  q->target_pos = vec2f_add(from,
                            vec2f_mul(dir, 1200.0f + world_randf(world) * 1200.0f));
  spawn_target(q, world);
}

void quest_abandon(Quest *q, World *world) {
  if (q->type == QUEST_NONE) return;

  Entity target = find_target(world);
  if (target != MAX_ENTITIES) {
    switch (q->type) {
    case QUEST_FETCH:
      entity_destroy(world, target);  // the beacon is gone
      break;
    case QUEST_BOUNTY:
      // The mark keeps flying, just nobody's paying for it anymore
      world->masks[target] &= ~C_QUEST_TARGET;
      world->wireframes[target].glow_color = PIRATE_GLOW_COLOR;
      break;
    case QUEST_DELIVER:
      world->masks[target] &= ~C_QUEST_TARGET;  // station stays; cargo doesn't
      break;
    default:
      break;
    }
  }

  q->type = QUEST_NONE;
  q->carrying = false;
  q->offer_count = 0;  // fresh board immediately
}

/** Pay out, flash the banner, and free the contract slot; the next
 *  docked frame rolls a fresh board. */
static void complete(Quest *q, World *world, Entity player) {
  world->players[player].money += q->reward;
  if (q->type == QUEST_BOUNTY) {
    world->players[player].bounties_done++;  // guild reputation
  }
  q->complete_timer = QUEST_COMPLETE_BANNER;
  q->type = QUEST_NONE;
  q->offer_count = 0;
  events_emit(EV_QUEST_COMPLETE, world->transforms[player].position);
}

void quest_update(Quest *q, World *world, Entity player, float dt) {
  if (q->complete_timer > 0.0f) q->complete_timer -= dt;
  if (!entity_has(world, player, C_PLAYER | C_TRANSFORM)) return;

  if (q->type == QUEST_NONE) {
    if (station_docked(world, player)) {
      if (q->offer_count == 0) roll_offers(q, world);
    } else {
      q->offer_count = 0;  // rerolls on the next dock
    }
    return;
  }

  Vec2f player_pos = world->transforms[player].position;
  Entity target = find_target(world);
  if (target != MAX_ENTITIES) {
    q->target_pos = world->transforms[target].position;
  }

  switch (q->type) {
  case QUEST_FETCH:
    if (!q->carrying) {
      if (target == MAX_ENTITIES) { q->type = QUEST_NONE; break; }  // lost
      Vec2f d = vec2f_sub(q->target_pos, player_pos);
      if (vec2f_length(d) < QUEST_PICKUP_DIST) {
        entity_destroy(world, target);
        q->carrying = true;
        events_emit(EV_QUEST_ITEM, q->target_pos);
      }
    } else if (station_docked(world, player)) {
      complete(q, world, player);
    }
    break;

  case QUEST_BOUNTY:
    // The mark carries C_QUEST_TARGET; when nothing does, it's dead
    if (target == MAX_ENTITIES) complete(q, world, player);
    break;

  case QUEST_DELIVER: {
    if (target == MAX_ENTITIES) { q->type = QUEST_NONE; break; }
    Vec2f d = vec2f_sub(q->target_pos, player_pos);
    if (vec2f_length(d) < STATION_DOCK_RADIUS) {
      world->masks[target] &= ~C_QUEST_TARGET;  // becomes a normal station
      complete(q, world, player);
    }
    break;
  }

  default:
    break;
  }
}

bool quest_compass_target(const Quest *q, Vec2f *out) {
  if (q->type == QUEST_NONE) return false;
  if (q->type == QUEST_FETCH && q->carrying) return false;  // any station now

  *out = q->target_pos;
  return true;
}

const char *quest_offer_label(QuestType t) {
  switch (t) {
  case QUEST_FETCH:   return "RECOVER BEACON";
  case QUEST_BOUNTY:  return "BOUNTY CONTRACT";
  case QUEST_DELIVER: return "DELIVERY RUN";
  default:            return "";
  }
}

const char *quest_status_label(const Quest *q) {
  switch (q->type) {
  case QUEST_FETCH:   return q->carrying ? "RETURN TO STATION" : "FIND BEACON";
  case QUEST_BOUNTY:  return "HUNT THE MARK";
  case QUEST_DELIVER: return "DELIVER CARGO";
  default:            return "";
  }
}
