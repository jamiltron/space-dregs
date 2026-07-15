// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pirate.h"
#include "events.h"
#include "particles.h"
#include "scrap.h"

// Hot red sparks when a pirate takes a hit
#define PIRATE_CHIP_COLOR (SDL_Color){ 255, 130, 90, 255 }

static const PirateStats STATS[PIRATE_ARCHETYPE_COUNT] = {
  [PIRATE_DART] = {
    .size = PIRATE_SIZE,
    .rot_speed = PIRATE_ROT_SPEED,
    .thrust = PIRATE_THRUST,
    .max_speed = PIRATE_MAX_SPEED,
    .fire_interval = PIRATE_FIRE_INTERVAL,
    .sense_radius = PIRATE_SENSE_RADIUS,
    .damping = PIRATE_DAMPING,
    .hp = PIRATE_HP,
    .ram_damage = PIRATE_RAM_DAMAGE,
    .scrap_base = 2,
  },
  [PIRATE_BRUTE] = {
    .size = 24.0f,
    .rot_speed = 110.0f,
    .thrust = 150.0f,
    .max_speed = 170.0f,
    .fire_interval = 1.9f,
    .sense_radius = PIRATE_SENSE_RADIUS,
    .damping = PIRATE_DAMPING,
    .hp = 8,
    .ram_damage = 24,
    .scrap_base = 5,
  },
  [PIRATE_DRONE] = {
    .size = 11.0f,
    .rot_speed = 220.0f,
    .thrust = 130.0f,
    .max_speed = 130.0f,
    .fire_interval = 1.0f,  // unused: no gun
    .sense_radius = 420.0f,
    .damping = 2.5f,        // settles back to a standstill after knocks
    .hp = 2,
    .ram_damage = 30,
    .scrap_base = 1,
    .kamikaze = true,
  },
};

const PirateStats *pirate_stats(int archetype) {
  if (archetype < 0 || archetype >= PIRATE_ARCHETYPE_COUNT) archetype = 0;
  return &STATS[archetype];
}

/** Dart: a red chevron with a notched tail. */
static void dart_shape(Wireframe *wf, float size) {
  wf->points[0] = vec2f_new(0.0f, -size);
  wf->points[1] = vec2f_new(-size * 0.8f, size * 0.7f);
  wf->points[2] = vec2f_new(0.0f, size * 0.25f);
  wf->points[3] = vec2f_new(size * 0.8f, size * 0.7f);
  wf->point_count = 4;
  wf->color      = (SDL_Color){ 230, 100, 80, 255 };
  wf->glow_color = PIRATE_GLOW_COLOR;
}

/** Drone: a 12-sided ring — reads as a circle at play scale. */
static void drone_shape(Wireframe *wf, float size) {
  wf->point_count = 12;
  for (int i = 0; i < wf->point_count; i++) {
    float a = TWO_PI * (float)i / (float)wf->point_count;
    wf->points[i] = vec2f_new(SDL_cosf(a) * size, SDL_sinf(a) * size);
  }
  wf->color      = (SDL_Color){ 255, 210, 80, 255 };
  wf->glow_color = (SDL_Color){ 255, 190, 50, 255 };
}

/** Brute: a blunt-nosed hull with flared rear pods and a tail notch. */
static void brute_shape(Wireframe *wf, float size) {
  wf->points[0] = vec2f_new(-size * 0.35f, -size);
  wf->points[1] = vec2f_new(size * 0.35f, -size);
  wf->points[2] = vec2f_new(size * 0.7f, -size * 0.2f);
  wf->points[3] = vec2f_new(size * 0.95f, size * 0.85f);
  wf->points[4] = vec2f_new(size * 0.4f, size * 0.55f);
  wf->points[5] = vec2f_new(0.0f, size * 0.8f);
  wf->points[6] = vec2f_new(-size * 0.4f, size * 0.55f);
  wf->points[7] = vec2f_new(-size * 0.95f, size * 0.85f);
  wf->points[8] = vec2f_new(-size * 0.7f, -size * 0.2f);
  wf->point_count = 9;
  wf->color      = (SDL_Color){ 255, 150, 70, 255 };
  wf->glow_color = (SDL_Color){ 255, 120, 40, 255 };
}

/** Wander/chase/fire behavior lives in system_pirate; death and hit
 *  response in pirate_hit. Stats come from the archetype table. */
Entity pirate_spawn(World *world, Vec2f position, PirateArchetype type) {
  const PirateStats *st = pirate_stats((int)type);
  Entity e = entity_create(world);

  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW |
                    C_COLLIDER | C_PIRATE;

  world->transforms[e] = (Transform){
    .position = position,
    .angle    = world_randf(world) * 360.0f,
  };

  world->velocities[e] = (Velocity){
    .value     = vec2f_new(0.0f, 0.0f),
    .damping   = st->damping,
    .max_speed = st->max_speed,
  };

  world->colliders[e] = (Collider){ .radius = st->size * 0.85f };

  world->pirates[e] = (Pirate){
    .fire_cooldown = st->fire_interval,  // no instant snap shots
    .wander_timer  = 0.0f,
    .wander_dir    = world_randf(world) * 360.0f,
    .hp            = st->hp,
    .archetype     = (int)type,
  };

  Wireframe *wf = &world->wireframes[e];
  if (type == PIRATE_BRUTE) brute_shape(wf, st->size);
  else if (type == PIRATE_DRONE) drone_shape(wf, st->size);
  else dart_shape(wf, st->size);

  return e;
}

/** Sparks on a survivable hit; death bursts and pays out the base
 *  scrap plus any hoarded loot. */
void pirate_hit(World *world, Entity e, int damage, Vec2f impact) {
  Vec2f velocity = world->velocities[e].value;
  const PirateStats *st = pirate_stats(world->pirates[e].archetype);

  world->pirates[e].hp -= damage;
  if (world->pirates[e].hp > 0) {
    world->wireframes[e].flash = 0.07f;
    debris_burst(world, impact, velocity, 2 + world_rand(world, 2),
                 PIRATE_CHIP_COLOR, 1.0f);
    events_emit(EV_PIRATE_CHIPPED, impact);
    return;
  }

  Vec2f position = world->transforms[e].position;
  debris_burst(world, position, velocity,
               (int)st->size - 5 + world_rand(world, 5),
               PIRATE_CHIP_COLOR, 1.5f);
  // Hoarded loot comes back out with the base payout
  scrap_scatter(world, position, velocity,
                st->scrap_base + world_rand(world, 2) + world->pirates[e].loot);
  events_emit(EV_PIRATE_DESTROYED, position);
  entity_destroy(world, e);
}

void pirate_detonate(World *world, Entity e) {
  Vec2f position = world->transforms[e].position;
  debris_burst(world, position, world->velocities[e].value,
               14 + world_rand(world, 6), PIRATE_CHIP_COLOR, 1.8f);
  events_emit(EV_PIRATE_DESTROYED, position);
  entity_destroy(world, e);
}
