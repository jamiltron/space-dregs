// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Asteroid factory and hit/split response.
 */

#ifndef _SD_ASTEROID_H
#define _SD_ASTEROID_H
#include "ecs.h"

#define ASTEROID_MIN_POINTS 8
#define ASTEROID_MAX_POINTS 12
#define ASTEROID_MIN_SPEED 10.0f
#define ASTEROID_MAX_SPEED 40.0f
#define ASTEROID_MAX_SPIN 25.0f  /**< Deg/s tumble. */
#define ASTEROID_MIN_RADIUS 20.0f
#define ASTEROID_MAX_RADIUS 50.0f
#define ASTEROID_GENERATIONS 3   /**< Splits twice, third break bursts. */

#define ASTEROID_RICH_MIN_DIST 2500.0f /**< Rich rocks only roll past this from home. */
#define ASTEROID_RICH_CHANCE 0.10f     /**< Per rock, out in the deep field. */
#define ASTEROID_RICH_HP_BONUS 2       /**< Denser ore cracks slower. */
#define ASTEROID_RICH_SCRAP_BASE 5     /**< Final-burst payout (plain pays 2-3)... */
#define ASTEROID_RICH_SCRAP_SPREAD 3   /**< ...plus up to this much. */

/** Rock varieties; Asteroid.kind. Children inherit the parent's kind. */
typedef enum AsteroidKind {
  ASTEROID_PLAIN,
  ASTEROID_RICH,  /**< Gold, glowing, tougher; bursts into a scrap jackpot. */
} AsteroidKind;

/** Spawn a lumpy drifting rock; radius sets its HP and child sizes. */
Entity asteroid_spawn(World *world, Vec2f position, float radius);

/** Turn a fresh plain rock rich: gold glow, +hp, jackpot payout. */
void asteroid_make_rich(World *world, Entity e);

/** Apply damage at an impact point: chip, split into 1d3+1 children,
 *  or burst with a scrap payout on the last generation. */
void asteroid_hit(World *world, Entity e, int damage, Vec2f impact);

#endif
