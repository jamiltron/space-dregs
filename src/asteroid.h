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

/** Spawn a lumpy drifting rock; radius sets its HP and child sizes. */
Entity asteroid_spawn(World *world, Vec2f position, float radius);

/** Apply damage at an impact point: chip, split into 1d3+1 children,
 *  or burst with a scrap payout on the last generation. */
void asteroid_hit(World *world, Entity e, int damage, Vec2f impact);

#endif
