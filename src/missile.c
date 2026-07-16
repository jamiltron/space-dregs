// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "missile.h"
#include "math2d.h"

/** A slim dart riding the bullet contact path (damage 4), with a
 *  lifetime for the motor burn. Guidance runs in system_missiles;
 *  allegiance rides the Bullet.hostile flag. */
Entity missile_spawn(World *world, Vec2f position, float angle, bool hostile) {
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_COLLIDER | C_BULLET | C_LIFETIME | C_MISSILE;

  world->transforms[e] = (Transform){ .position = position, .angle = angle };
  world->velocities[e] = (Velocity){
    .value = vec2f_mul(vec2f_dir(DEG_TO_RAD(angle)), MISSILE_SPEED),
  };
  world->colliders[e] = (Collider){ .radius = 3.0f };
  world->bullets[e] = (Bullet){ .hostile = hostile, .damage = MISSILE_DAMAGE };
  world->lifetimes[e] = (Lifetime){
    .remaining = MISSILE_LIFETIME,
    .initial   = MISSILE_LIFETIME,
  };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -7.0f);
  wf->points[1] = vec2f_new(2.5f, 5.0f);
  wf->points[2] = vec2f_new(0.0f, 3.0f);
  wf->points[3] = vec2f_new(-2.5f, 5.0f);
  wf->point_count = 4;
  if (hostile) {
    wf->color      = (SDL_Color){ 255, 120, 120, 255 };
    wf->glow_color = (SDL_Color){ 255, 70, 70, 255 };
  } else {
    wf->color      = (SDL_Color){ 240, 240, 255, 255 };
    wf->glow_color = (SDL_Color){ 255, 200, 120, 255 };
  }

  return e;
}
