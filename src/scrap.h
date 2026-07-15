// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Scrap pickups: golden diamonds that magnet to the player.
 */

#ifndef _SD_SCRAP_H
#define _SD_SCRAP_H
#include "ecs.h"

#define SCRAP_SIZE 5.0f
#define SCRAP_PICKUP_DIST 20.0f    /**< Collected inside this range. */
#define SCRAP_MAGNET_RADIUS 160.0f /**< Base pull range (upgradeable). */
#define SCRAP_MAGNET_PULL 1100.0f  /**< Px/s^2 at the rim, stronger closer. */

/** Spawn one piece of scrap. */
Entity scrap_spawn(World *world, Vec2f position, Vec2f velocity);

/** Scatter count pieces outward from position (asteroid/pirate drops). */
void scrap_scatter(World *world, Vec2f position, Vec2f base_velocity, int count);

#endif
