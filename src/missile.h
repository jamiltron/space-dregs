// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Heat-seeking missiles: launched off the nose, steer toward the
 *  nearest target in seeker range, fizzle when the motor burns out.
 *  Player missiles hunt pirates; hostile ones (battleship-fired) hunt
 *  the player but are decoyed by a closer mine, like drones.
 */

#ifndef _SD_MISSILE_H
#define _SD_MISSILE_H
#include "ecs.h"

#define MISSILE_SPEED 430.0f       /**< Slower than a bullet, faster than ships. */
#define MISSILE_TURN_RATE 260.0f   /**< Deg/s of seeker steering. */
#define MISSILE_SEEK_RADIUS 520.0f /**< Acquires pirates inside this. */
#define MISSILE_LIFETIME 4.0f      /**< Motor burn; fizzles after. */
#define MISSILE_DAMAGE 4           /**< On contact (small-int hp scale). */
#define MISSILE_HULL_DAMAGE 18     /**< Hostile hit on the player (hull scale). */
#define MISSILE_AMMO_MAX 4         /**< Pod capacity. */

/** Launch a missile from position along angle (degrees); hostile ones
 *  seek the player instead of pirates. */
Entity missile_spawn(World *world, Vec2f position, float angle, bool hostile);

#endif
