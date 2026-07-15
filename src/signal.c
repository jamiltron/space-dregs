// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "signal.h"

/** A slim four-point antenna star with a cool cyan glow. */
Entity signal_spawn(World *world, Vec2f position, bool contract) {
  Entity e = entity_create(world);
  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW | C_SIGNAL;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = world_randf(world) * 360.0f,
  };
  world->velocities[e] = (Velocity){ .damping = 0.0f, .spin = 24.0f };
  world->signals[e] = (Signal){ .contract = contract };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -10.0f);
  wf->points[1] = vec2f_new(3.0f, 0.0f);
  wf->points[2] = vec2f_new(0.0f, 10.0f);
  wf->points[3] = vec2f_new(-3.0f, 0.0f);
  wf->point_count = 4;
  wf->color      = (SDL_Color){ 120, 230, 255, 255 };
  wf->glow_color = (SDL_Color){ 100, 220, 255, 255 };

  return e;
}
