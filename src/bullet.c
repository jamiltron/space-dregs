// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bullet.h"

/** Lifetime handles despawn; the collision system decides what a hit
 *  means. Hostile (pirate) tracers burn red. */
Entity bullet_spawn(World *world, Vec2f position, Vec2f velocity, float angle,
                    bool hostile) {
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_COLLIDER | C_BULLET | C_LIFETIME;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = angle,
  };

  world->velocities[e] = (Velocity){
    .value     = velocity,
    .damping   = 0.0f,
    .max_speed = 0.0f,
    .spin      = 0.0f,
  };

  world->colliders[e] = (Collider){ .radius = BULLET_RADIUS };
  world->bullets[e] = (Bullet){ .hostile = hostile, .damage = 1 };

  world->lifetimes[e] = (Lifetime){
    .remaining = BULLET_LIFETIME,
    .initial   = BULLET_LIFETIME,
  };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -BULLET_LENGTH * 0.5f);
  wf->points[1] = vec2f_new(0.0f, BULLET_LENGTH * 0.5f);
  wf->point_count = 2;
  if (hostile) {
    wf->color      = (SDL_Color){ 255, 150, 110, 255 };
    wf->glow_color = (SDL_Color){ 255, 120, 80, 255 };
  } else {
    wf->color      = (SDL_Color){ 200, 230, 255, 255 };
    wf->glow_color = (SDL_Color){ 255, 255, 255, 255 };
  }

  return e;
}
