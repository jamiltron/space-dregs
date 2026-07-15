// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mine.h"
#include "asteroid.h"
#include "events.h"
#include "particles.h"
#include "pirate.h"
#include "ship.h"
#include "systems.h"

// Hot blast colors, same family as the warning paint
#define MINE_COLOR (SDL_Color){ 255, 80, 70, 255 }
#define MINE_GLOW_COLOR (SDL_Color){ 255, 60, 50, 255 }

/** An eight-point spiked star: unmistakably not scrap. */
Entity mine_spawn(World *world, Vec2f position, Vec2f velocity) {
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_COLLIDER | C_MINE;

  world->transforms[e] = (Transform){ .position = position };
  world->velocities[e] = (Velocity){
    .value   = velocity,
    .damping = MINE_DAMPING,
    .spin    = 45.0f,
  };
  world->colliders[e] = (Collider){ .radius = MINE_SIZE * 0.7f };
  world->mines[e] = (Mine){ .fuse = MINE_FUSE };

  Wireframe *wf = &world->wireframes[e];
  wf->point_count = 8;
  for (int i = 0; i < wf->point_count; i++) {
    float a = TWO_PI * (float)i / (float)wf->point_count;
    float r = (i % 2 == 0) ? MINE_SIZE : MINE_SIZE * 0.45f;
    wf->points[i] = vec2f_new(SDL_cosf(a) * r, SDL_sinf(a) * r);
  }
  wf->color      = MINE_COLOR;
  wf->glow_color = MINE_GLOW_COLOR;

  return e;
}

/** Victims are collected before any damage lands, so children and
 *  scrap spawned by the blast can't be caught in the same blast. */
void mine_explode(World *world, Entity e) {
  Vec2f center = world->transforms[e].position;

  // This mine is spent first, so chained mines can't re-trigger it
  debris_burst(world, center, world->velocities[e].value,
               16 + world_rand(world, 6), MINE_COLOR, 2.0f);
  events_emit(EV_MINE_EXPLODED, center);
  entity_destroy(world, e);

  Entity victims[64];
  int victim_count = 0;
  for (Entity v = 0; v < world->high_water; v++) {
    if (victim_count >= 64) break;
    if (!entity_has(world, v, C_TRANSFORM)) continue;
    if (entity_has(world, v, C_FROZEN)) continue;
    if (!(world->masks[v] & (C_PIRATE | C_ASTEROID | C_PLAYER | C_MINE)))
      continue;

    float d = vec2f_length(vec2f_sub(world->transforms[v].position, center));
    if (d < MINE_BLAST_RADIUS + world->colliders[v].radius) {
      victims[victim_count++] = v;
    }
  }

  for (int i = 0; i < victim_count; i++) {
    Entity v = victims[i];
    Vec2f vpos = world->transforms[v].position;

    if (entity_has(world, v, C_PIRATE)) {
      pirate_hit(world, v, MINE_DAMAGE_SHIPS, vpos);
    } else if (entity_has(world, v, C_ASTEROID)) {
      asteroid_hit(world, v, MINE_DAMAGE_SHIPS, vpos);
    } else if (entity_has(world, v, C_MINE)) {
      mine_explode(world, v);
    } else if (entity_has(world, v, C_PLAYER)) {
      Player *p = &world->players[v];
      if (p->damage_timer <= 0.0f) {
        player_take_damage(p, MINE_DAMAGE_PLAYER);
        p->damage_timer = SHIP_DAMAGE_COOLDOWN;
        world->wireframes[v].flash = 0.1f;
        events_emit(EV_PLAYER_HURT, vpos);
        if (p->hp <= 0) ship_explode(world, v);
      }
    }
  }
}
