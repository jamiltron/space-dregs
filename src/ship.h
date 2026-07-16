// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Player ship factory, tuning constants and death.
 */

#ifndef _SD_SHIP_H
#define _SD_SHIP_H
#include "ecs.h"

#define SHIP_SIZE 18.0f
#define SHIP_ROT_SPEED 240.0f
#define SHIP_THRUST_FORCE 320.0f
#define SHIP_MAX_SPEED 400.0f
#define SHIP_DAMPING 1.2f           /**< Velocity decay per second; also caps
                                         coasting speed at thrust/damping. */
#define SHIP_MAX_HP 100
#define SHIP_START_DEBT 5000        /**< Owed at run start; zero = endgame hook. */
#define SHIP_SHIELD_MAX 10          /**< Capacity of the purchasable shield. */
#define SHIP_SHIELD_REGEN_SECS 10.0f /**< Seconds per shield point regained. */
#define SHIP_FUEL_MAX 100.0f
#define SHIP_LOW_HULL 30            /**< Red zone: HUD color + warning siren. */
#define SHIP_LOW_FUEL_FRAC 0.2f     /**< Red zone as a fraction of the tank. */
#define SHIP_WARN_STEP 10.0f        /**< Warnings re-fire every this much lower. */
#define SHIP_FUEL_BURN 1.25f        /**< Per second of thrust. */
#define SHIP_DAMAGE_COOLDOWN 1.5f   /**< I-frames after taking a hit. */
#define SHIP_SPAWN_PROTECTION 2.0f  /**< I-frames on (re)spawn. */
#define SHIP_DEATH_DELAY 0.9f       /**< Explosion settle before game over. */

/** Spawn the player ship at position with full hull and fuel. */
Entity ship_spawn(World *world, Vec2f position);

/** Blow the ship apart (hull edges + sparks) and destroy the entity. */
void ship_explode(World *world, Entity e);

#endif
