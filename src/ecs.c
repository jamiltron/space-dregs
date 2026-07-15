// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ecs.h"

void world_init(World *world) {
  SDL_memset(world, 0, sizeof(*world));
}

Entity entity_create(World *world) {
  Entity e;

  if (world->free_count > 0) {
    e = world->free_list[--world->free_count];
  } else {
    SDL_assert(world->high_water < MAX_ENTITIES);
    e = world->high_water++;
  }

  world->masks[e] = C_NONE;
  world->wireframes[e].flash = 0.0f;  // recycled ids must not inherit a flash
  return e;
}

void entity_destroy(World *world, Entity e) {
  world->masks[e] = C_NONE;
  world->free_list[world->free_count++] = e;
}
