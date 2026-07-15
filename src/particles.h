// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Short-lived line-fragment particles (debris, sparks, exhaust).
 */

#ifndef _SD_PARTICLES_H
#define _SD_PARTICLES_H
#include "ecs.h"

/** Scatter count fading motes from position; speed_scale stretches
 *  bigger blasts. */
void debris_burst(World *world, Vec2f position, Vec2f base_velocity,
                  int count, SDL_Color color, float speed_scale);

/** One directed mote for the thruster plume. */
void exhaust_particle(World *world, Vec2f position, Vec2f velocity,
                      SDL_Color color);

#endif
