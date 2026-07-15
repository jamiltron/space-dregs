// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scrap.h"

/** Scattered pieces settle and tumble in place until the player
 *  drifts close enough for the magnet (system_scrap) to reel them in. */
Entity scrap_spawn(World *world, Vec2f position, Vec2f velocity) {
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_SCRAP | C_COLLIDER;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = world_randf(world) * 360.0f,
  };

  world->velocities[e] = (Velocity){
    .value     = velocity,
    .damping   = 12.0f,  // settle quickly after the scatter
    .max_speed = 0.0f,
    .spin      = (world_randf(world) < 0.5f ? -1.0f : 1.0f) *
                 (60.0f + world_randf(world) * 80.0f),
  };

  // Tiny body: bounces off rocks instead of sitting inside them,
  // but far too light to chip anything
  world->colliders[e] = (Collider){ .radius = SCRAP_SIZE * 0.9f };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -SCRAP_SIZE);
  wf->points[1] = vec2f_new(SCRAP_SIZE * 0.6f, 0.0f);
  wf->points[2] = vec2f_new(0.0f, SCRAP_SIZE);
  wf->points[3] = vec2f_new(-SCRAP_SIZE * 0.6f, 0.0f);
  wf->point_count = 4;
  wf->color      = (SDL_Color){ 255, 200, 80, 255 };   // golden
  wf->glow_color = (SDL_Color){ 255, 210, 110, 255 };

  return e;
}

void scrap_scatter(World *world, Vec2f position, Vec2f base_velocity, int count) {
  for (int i = 0; i < count; i++) {
    Vec2f dir = vec2f_dir(world_randf(world) * TWO_PI);
    Vec2f vel = vec2f_add(base_velocity,
                          vec2f_mul(dir, 40.0f + world_randf(world) * 80.0f));
    scrap_spawn(world, position, vel);
  }
}
