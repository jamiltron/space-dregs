// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Bullet factory: short neon tracers with a lifetime.
 */

#ifndef _SD_BULLET_H
#define _SD_BULLET_H
#include "ecs.h"

#define BULLET_SPEED 700.0f
#define BULLET_COOLDOWN 0.44f  /**< Player fire interval before upgrades. */
#define BULLET_LIFETIME 1.1f
#define BULLET_RADIUS 2.5f
#define BULLET_LENGTH 9.0f

/** Spawn a tracer. Hostile bullets are pirate-fired: red and hurt only the
 *  player. angle only orients the visual. */
Entity bullet_spawn(World *world, Vec2f position, Vec2f velocity, float angle,
                    bool hostile);

#endif
