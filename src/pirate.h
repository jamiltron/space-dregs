// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Pirate factory, AI tuning and hit response.
 */

#ifndef _SD_PIRATE_H
#define _SD_PIRATE_H
#include "ecs.h"

#define PIRATE_SIZE 15.0f
#define PIRATE_HP 3
#define PIRATE_ROT_SPEED 180.0f      /**< Deg/s, slower than the player. */
#define PIRATE_THRUST 230.0f
#define PIRATE_MAX_SPEED 260.0f
#define PIRATE_DAMPING 1.0f          /**< Velocity decay per second. */
#define PIRATE_SENSE_RADIUS 450.0f   /**< Notices the player inside this. */
#define PIRATE_KEEP_DIST 180.0f      /**< Stops closing in past this. */
#define PIRATE_FIRE_RANGE 400.0f
#define PIRATE_FIRE_INTERVAL 1.4f    /**< Slower trigger than the player. */
#define PIRATE_BULLET_SPEED_SCALE 0.75f
#define PIRATE_BULLET_DAMAGE 12
#define PIRATE_RAM_DAMAGE 15
#define PIRATE_SCRAP_RADIUS 350.0f   /**< Loots scrap it notices while idle. */
#define PIRATE_SCRAP_PICKUP 22.0f
#define PIRATE_GLOW_COLOR (SDL_Color){ 255, 90, 60, 255 }  /**< Hostile red halo. */

#define PIRATE_BRUTE_MIN_DIST 2000.0f  /**< Brutes only spawn past this. */
#define PIRATE_BRUTE_CHANCE 0.25f      /**< Per pirate, in the deep field. */
#define PIRATE_DRONE_MIN_DIST 1200.0f  /**< Drones only spawn past this. */
#define PIRATE_DRONE_CHANCE 0.20f      /**< Per pirate. */
#define PIRATE_BATTLESHIP_MIN_DIST 3500.0f  /**< Battleships haunt the far deep. */
#define PIRATE_BATTLESHIP_CHANCE 0.06f      /**< Per pirate; much rarer than the rest. */
#define PIRATE_MOTHERSHIP_MIN_DIST 4500.0f  /**< Motherships rule the frontier. */
#define PIRATE_MOTHERSHIP_CHANCE 0.03f      /**< Per pirate; the rarest sight. */

#define PIRATE_MAX_GUNS 3
#define PIRATE_DEPLOY_CAP 5        /**< No new drones with this many already close. */
#define PIRATE_DEPLOY_RADIUS 800.0f /**< Radius of that drone head-count. */

/** Pirate archetypes; per-type tuning lives in the table in pirate.c. */
typedef enum PirateArchetype {
  PIRATE_DART,        /**< Baseline chevron: quick and fragile. */
  PIRATE_BRUTE,       /**< Heavy gunship: bigger, slower, much tougher. */
  PIRATE_DRONE,       /**< Kamikaze mine: sits dormant, then rams and explodes. */
  PIRATE_BATTLESHIP,  /**< Long capital hull: heavy hp, fast in a line,
                           slow to turn, lobs seeking missiles. */
  PIRATE_MOTHERSHIP,  /**< Carrier: barely moves, laser mounts on three
                           bearings, spits kamikaze drones while engaged. */
  PIRATE_ARCHETYPE_COUNT
} PirateArchetype;

/** Per-archetype tuning. */
typedef struct PirateStats {
  float size;           /**< Hull scale; collider and muzzle follow it. */
  float rot_speed;      /**< Deg/s. */
  float thrust;
  float max_speed;
  float fire_interval;  /**< Seconds between shots (a volley if multi-gun). */
  float missile_interval; /**< Seconds between seeking missiles; 0 = no launcher. */
  float deploy_interval;  /**< Seconds between drone launches; 0 = no bay. */
  int   gun_count;      /**< Fixed mounts; 0 reads as one nose gun. */
  float gun_bearings[PIRATE_MAX_GUNS]; /**< Mount angles off the nose, degrees. */
  bool  laser;          /**< Shots render as green laser bolts. */
  float sense_radius;   /**< Notices the player inside this. */
  float damping;        /**< Velocity decay per second. */
  int   hp;
  int   ram_damage;     /**< Dealt to the player on contact. */
  int   scrap_base;     /**< Minimum death payout, before loot. */
  bool  kamikaze;       /**< Dormant until sensed, no gun or looting,
                             detonates on player contact. */
} PirateStats;

/** Tuning row for an archetype (out-of-range types read as dart). */
const PirateStats *pirate_stats(int archetype);

/** Spawn a pirate of the given archetype; behavior runs in system_pirate. */
Entity pirate_spawn(World *world, Vec2f position, PirateArchetype type);

/** Apply damage: spark, or burst with a scrap+loot payout on death. */
void pirate_hit(World *world, Entity e, int damage, Vec2f impact);

/** Kamikaze detonation: burst and die, no payout (shooting one down
 *  through pirate_hit still pays). */
void pirate_detonate(World *world, Entity e);

#endif
