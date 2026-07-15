// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ship.h"
#include "bullet.h"
#include "events.h"
#include "particles.h"
#include "scrap.h"

/** The hull is a triangle in local space (origin = center), tip up
 *  before rotation; rotation is clockwise since screen Y grows down.
 *  @verbatim
 *      v0 (tip)
 *      /\
 *     /  \
 *    /____\
 *  v1      v2
 *  @endverbatim */
Entity ship_spawn(World *world, Vec2f position) {
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_PLAYER | C_WIREFRAME |
                    C_GLOW | C_COLLIDER;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = 0.0f,
  };

  world->velocities[e] = (Velocity){
    .value     = vec2f_new(0.0f, 0.0f),
    .damping   = SHIP_DAMPING,
    .max_speed = SHIP_MAX_SPEED,
  };

  world->colliders[e] = (Collider){ .radius = SHIP_SIZE * 0.7f };

  world->players[e] = (Player){
    .rot_speed      = SHIP_ROT_SPEED,
    .thrust_force   = SHIP_THRUST_FORCE,
    .exhaust_offset = SHIP_SIZE * 0.66f,
    .flame_length   = SHIP_SIZE * 0.8f,
    .muzzle_offset  = SHIP_SIZE + 4.0f,  // just past the tip
    .fire_interval  = BULLET_COOLDOWN,
    .fire_cooldown  = 0.0f,
    .damage_timer   = SHIP_SPAWN_PROTECTION,
    .fuel           = SHIP_FUEL_MAX,
    .fuel_max       = SHIP_FUEL_MAX,
    .magnet_radius  = SCRAP_MAGNET_RADIUS,
    .hp             = SHIP_MAX_HP,
    .max_hp         = SHIP_MAX_HP,
    .debt           = SHIP_START_DEBT,
    .thrusting      = false,
  };

  Wireframe *wf = &world->wireframes[e];
  wf->points[0] = vec2f_new(0.0f, -SHIP_SIZE);
  wf->points[1] = vec2f_new(-SHIP_SIZE * 0.66f, SHIP_SIZE * 0.66f);
  wf->points[2] = vec2f_new(SHIP_SIZE * 0.66f, SHIP_SIZE * 0.66f);
  wf->point_count = 3;
  wf->color      = (SDL_Color){ 180, 200, 255, 255 };
  wf->glow_color = (SDL_Color){ 255, 255, 255, 255 };  // neon white halo

  return e;
}

/** The classic vector-game end: the hull breaks into its own three
 *  edges, which tumble apart and fade, over a spark burst. */
void ship_explode(World *world, Entity e) {
  Vec2f position = world->transforms[e].position;
  Vec2f velocity = world->velocities[e].value;
  float rad = DEG_TO_RAD(world->transforms[e].angle);

  static const Vec2f VERTS[3] = {
    { 0.0f, -SHIP_SIZE },
    { -SHIP_SIZE * 0.66f, SHIP_SIZE * 0.66f },
    { SHIP_SIZE * 0.66f, SHIP_SIZE * 0.66f },
  };

  for (int i = 0; i < 3; i++) {
    Vec2f a = VERTS[i];
    Vec2f b = VERTS[(i + 1) % 3];
    Vec2f mid = vec2f_mul(vec2f_add(a, b), 0.5f);
    Vec2f mid_world = vec2f_rotate(mid, rad);

    Entity edge = entity_create(world);
    world->masks[edge] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME |
                         C_GLOW | C_LIFETIME;

    world->transforms[edge] = (Transform){
      .position = vec2f_add(position, mid_world),
      .angle    = world->transforms[e].angle,
    };

    // Each edge drifts outward from the hull center and tumbles
    Vec2f outward = vec2f_mul(mid_world, 1.0f / vec2f_length(mid_world));
    world->velocities[edge] = (Velocity){
      .value = vec2f_add(velocity,
                         vec2f_mul(outward, 30.0f + world_randf(world) * 40.0f)),
      .damping = 2.0f,
      .spin  = (world_randf(world) < 0.5f ? -1.0f : 1.0f) *
               (90.0f + world_randf(world) * 160.0f),
    };

    float life = 1.1f + world_randf(world) * 0.3f;
    world->lifetimes[edge] = (Lifetime){ .remaining = life, .initial = life };

    Wireframe *wf = &world->wireframes[edge];
    wf->points[0] = vec2f_sub(a, mid);
    wf->points[1] = vec2f_sub(b, mid);
    wf->point_count = 2;
    wf->color      = (SDL_Color){ 180, 200, 255, 255 };
    wf->glow_color = (SDL_Color){ 255, 255, 255, 255 };
  }

  debris_burst(world, position, velocity, 10 + world_rand(world, 5),
               (SDL_Color){ 200, 220, 255, 255 }, 1.6f);
  events_emit(EV_PLAYER_DIED, position);
  entity_destroy(world, e);
}
