// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "particles.h"

/** Every particle here is the same thing: a short spinning line
 *  fragment that decelerates and fades out on its lifetime. */
static void spawn_mote(World *world, Vec2f position, Vec2f velocity,
                       float damping, float max_spin, float life, float len,
                       SDL_Color color) {
  Entity e = entity_create(world);
  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_LIFETIME;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = world_randf(world) * 360.0f,
  };

  world->velocities[e] = (Velocity){
    .value   = velocity,
    .damping = damping,
    .spin    = (world_randf(world) * 2.0f - 1.0f) * max_spin,
  };

  world->lifetimes[e] = (Lifetime){ .remaining = life, .initial = life };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -len * 0.5f);
  wf->points[1] = vec2f_new(0.0f, len * 0.5f);
  wf->point_count = 2;
  wf->color      = color;
  wf->glow_color = color;
}

/** Shared by asteroid chips (green), pirate sparks (red) and the ship
 *  explosion (pale blue). */
void debris_burst(World *world, Vec2f position, Vec2f base_velocity,
                  int count, SDL_Color color, float speed_scale) {
  for (int i = 0; i < count; i++) {
    Vec2f dir = vec2f_dir(world_randf(world) * TWO_PI);
    float speed = (60.0f + world_randf(world) * 120.0f) * speed_scale;

    spawn_mote(world, position,
               vec2f_add(base_velocity, vec2f_mul(dir, speed)),
               6.0f, 360.0f, 0.35f + world_randf(world) * 0.4f,
               3.0f + world_randf(world) * 4.0f, color);
  }
}

void exhaust_particle(World *world, Vec2f position, Vec2f velocity,
                      SDL_Color color) {
  spawn_mote(world, position, velocity, 10.0f, 240.0f,
             0.2f + world_randf(world) * 0.2f, 2.0f + world_randf(world) * 2.5f, color);
}
