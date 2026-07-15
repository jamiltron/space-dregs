// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Signal beacons: small glowing antennas scattered through the belt.
 *  Flying within range triggers a one-time broadcast — flavor chatter,
 *  or an intercepted bounty posting that lands on your nav.
 */

#ifndef _SD_SIGNAL_H
#define _SD_SIGNAL_H
#include "ecs.h"

#define SIGNAL_RANGE 200.0f  /**< Broadcast triggers inside this. */

/** Spawn a beacon; contract ones grant a bounty when received. */
Entity signal_spawn(World *world, Vec2f position, bool contract);

#endif
