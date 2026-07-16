// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "asteroid.h"
#include "events.h"
#include "particles.h"
#include "scrap.h"

// Brighter green flash than the asteroid bodies
#define CHIP_COLOR (SDL_Color){ 110, 190, 125, 255 }
// Rich rocks spark gold like the ore they carry
#define RICH_CHIP_COLOR (SDL_Color){ 255, 210, 110, 255 }

/** Each asteroid is a lumpy polygon: points evenly spaced around a
 *  circle with per-vertex radius jitter. They drift in a random
 *  direction (no damping, no speed cap) and tumble slowly. */
Entity asteroid_spawn(World *world, Vec2f position, float radius) {
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_COLLIDER;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = world_randf(world) * 360.0f,
  };

  Vec2f drift_dir = vec2f_dir(world_randf(world) * TWO_PI);
  float speed = ASTEROID_MIN_SPEED +
                world_randf(world) * (ASTEROID_MAX_SPEED - ASTEROID_MIN_SPEED);

  world->velocities[e] = (Velocity){
    .value     = vec2f_mul(drift_dir, speed),
    .damping   = 0.0f,  // asteroids coast forever
    .max_speed = 0.0f,
    .spin      = (world_randf(world) * 2.0f - 1.0f) * ASTEROID_MAX_SPIN,
  };

  Wireframe *wf = &world->wireframes[e];
  int points = ASTEROID_MIN_POINTS +
               world_rand(world, ASTEROID_MAX_POINTS - ASTEROID_MIN_POINTS + 1);

  for (int i = 0; i < points; i++) {
    float ang = (float)i / (float)points * TWO_PI;
    float r = radius * (0.7f + world_randf(world) * 0.45f);
    wf->points[i] = vec2f_mul(vec2f_dir(ang), r);
  }
  wf->point_count = points;
  wf->color      = (SDL_Color){ 60, 125, 75, 255 };  // darkish green
  wf->glow_color = (SDL_Color){ 60, 125, 75, 255 };  // unused until C_GLOW

  world->colliders[e] = (Collider){ .radius = radius * 0.9f };
  world->asteroids[e] = (Asteroid){
    .generation = 0,
    .hp         = 1 + (int)(radius / 15.0f),  // ~2 small, ~4 big
    .radius     = radius,
  };
  world->masks[e] |= C_ASTEROID;

  return e;
}

void asteroid_make_rich(World *world, Entity e) {
  world->asteroids[e].kind = ASTEROID_RICH;
  world->asteroids[e].hp += ASTEROID_RICH_HP_BONUS;

  Wireframe *wf = &world->wireframes[e];
  wf->color      = (SDL_Color){ 205, 170, 70, 255 };
  wf->glow_color = (SDL_Color){ 255, 200, 90, 255 };
  world->masks[e] |= C_GLOW;  // plain rocks don't glow; rich ones beckon
}

/** Debris sprays from the impact point; children and scrap come from
 *  the body itself. */
void asteroid_hit(World *world, Entity e, int damage, Vec2f impact) {
  Asteroid ast = world->asteroids[e];
  Vec2f position = world->transforms[e].position;
  Vec2f velocity = world->velocities[e].value;

  SDL_Color chip = ast.kind == ASTEROID_RICH ? RICH_CHIP_COLOR : CHIP_COLOR;

  world->asteroids[e].hp -= damage;
  if (world->asteroids[e].hp > 0) {
    world->wireframes[e].flash = 0.07f;
    debris_burst(world, impact, velocity, 1 + damage + world_rand(world, 2),
                 chip, 1.0f);
    events_emit(EV_ASTEROID_CHIPPED, impact);
    return;
  }

  if (ast.generation < ASTEROID_GENERATIONS - 1) {
    int children = 2 + world_rand(world, 3);
    float base_angle = world_randf(world) * TWO_PI;

    for (int i = 0; i < children; i++) {
      float ang = base_angle + (float)i / (float)children * TWO_PI +
                  (world_randf(world) - 0.5f) * 0.6f;
      Vec2f dir = vec2f_dir(ang);
      float child_radius = ast.radius * (0.5f + world_randf(world) * 0.15f);

      Entity child = asteroid_spawn(
          world, vec2f_add(position, vec2f_mul(dir, child_radius)),
          child_radius);

      world->asteroids[child].generation = ast.generation + 1;
      if (ast.kind == ASTEROID_RICH) asteroid_make_rich(world, child);
      world->velocities[child].value =
          vec2f_add(velocity, vec2f_mul(dir, 40.0f + world_randf(world) * 50.0f));
    }

    debris_burst(world, impact, velocity, 3 + world_rand(world, 3), chip, 1.0f);
    events_emit(EV_ASTEROID_BROKE, impact);
  } else {
    int payout = ast.kind == ASTEROID_RICH
                     ? ASTEROID_RICH_SCRAP_BASE +
                           world_rand(world, ASTEROID_RICH_SCRAP_SPREAD)
                     : 2 + world_rand(world, 2);
    debris_burst(world, impact, velocity, 6 + world_rand(world, 5), chip, 1.0f);
    scrap_scatter(world, position, velocity, payout);
    events_emit(EV_ASTEROID_DESTROYED, position);
  }

  entity_destroy(world, e);
}
