// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Droppable mines: timed charges from the rear rack. Kamikaze drones
 *  (and later, missiles) prefer a closer mine over the player.
 */

#ifndef _SD_MINE_H
#define _SD_MINE_H
#include "ecs.h"

#define MINE_SIZE 8.0f
#define MINE_FUSE 3.0f            /**< Seconds from drop to blast. */
#define MINE_BLAST_RADIUS 90.0f
#define MINE_DAMAGE_SHIPS 5       /**< To pirates/asteroids (small-int hp scale). */
#define MINE_DAMAGE_PLAYER 25     /**< To the player's hull scale. */
#define MINE_AMMO_MAX 3           /**< Rack capacity. */
#define MINE_DROP_OFFSET 30.0f    /**< Behind the ship's center. */
#define MINE_DAMPING 1.8f         /**< Trails the drop, then parks. */

/** Drop a live mine; it inherits the dropping ship's velocity. */
Entity mine_spawn(World *world, Vec2f position, Vec2f velocity);

/** Detonate now: area damage to pirates, asteroids and the player,
 *  chaining into other mines in the blast. */
void mine_explode(World *world, Entity e);

#endif
