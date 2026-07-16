// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Minimal ECS: flat component arrays indexed by entity id.
 */

#ifndef _SD_ECS_H
#define _SD_ECS_H
#include <stdbool.h>
#include "components.h"

#define MAX_ENTITIES 4096

/** Entity handle: an index into the World arrays. Ids are recycled. */
typedef Uint16 Entity;

/** All game state. Component slots are valid only when the matching
 *  bit is set in masks[]; destroyed ids go on a free list for reuse. */
typedef struct World {
  ComponentMask masks[MAX_ENTITIES];

  // Component storage
  Transform transforms[MAX_ENTITIES];
  Transform prev_transforms[MAX_ENTITIES];  /**< Pre-step copy for render interpolation. */
  Velocity  velocities[MAX_ENTITIES];
  Player    players[MAX_ENTITIES];
  Wireframe wireframes[MAX_ENTITIES];
  Collider  colliders[MAX_ENTITIES];
  Asteroid  asteroids[MAX_ENTITIES];
  Lifetime  lifetimes[MAX_ENTITIES];
  Bullet    bullets[MAX_ENTITIES];
  Pirate    pirates[MAX_ENTITIES];
  Frozen    frozens[MAX_ENTITIES];
  Signal    signals[MAX_ENTITIES];
  Mine      mines[MAX_ENTITIES];
  Freighter freighters[MAX_ENTITIES];

  Factions factions;  /**< Run-scoped standings and clan heat. */

  // Entity allocation
  Entity free_list[MAX_ENTITIES];
  int    free_count;
  Entity high_water;  /**< Ids in [0, high_water) have been handed out. */

  Uint64 rng;  /**< Simulation RNG stream; seeded per run, saved, replayable. */
} World;

/** Zero the world; every entity slot becomes free. */
void world_init(World *world);

/** Allocate an entity id with an empty mask. Asserts when full. */
Entity entity_create(World *world);

/** Release an entity; its id may be reused by a later create. */
void entity_destroy(World *world, Entity e);

/** True when the entity has every component in mask. */
static inline bool entity_has(const World *world, Entity e, ComponentMask mask) {
  return (world->masks[e] & mask) == mask;
}

/** Simulation randomness. All world-mutating code must use these (never
 *  the global SDL_rand) so runs are reproducible from the seed.
 *  @return Uniform value in [0, n). */
static inline Sint32 world_rand(World *world, Sint32 n) {
  return SDL_rand_r(&world->rng, n);
}

/** @return Uniform value in [0, 1). */
static inline float world_randf(World *world) {
  return SDL_randf_r(&world->rng);
}

#endif
