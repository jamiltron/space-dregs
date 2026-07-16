// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Distress calls: timed rescue events alongside the contract system.
 *
 *  On a random interval a mayday arrives — an amber compass arrow and
 *  a HUD countdown point at a freighter under raider attack. Reaching
 *  the scene pauses the clock (the raiders turn on you); clearing them
 *  pays credits and jettisoned scrap. Let the clock run out and only a
 *  wreck remains. Some calls are bait: an ambush pack and no freighter.
 *  Scene members carry C_DISTRESS; survivors are untagged into normal
 *  pirates when the event resolves. State is transient — it is not
 *  saved, and stale tags resolve safely after a load.
 */

#ifndef _SD_DISTRESS_H
#define _SD_DISTRESS_H
#include <stdbool.h>
#include "ecs.h"

#define DISTRESS_FIRST_DELAY 75.0f     /**< Quiet opening stretch of a run. */
#define DISTRESS_INTERVAL_MIN 100.0f   /**< Seconds between calls... */
#define DISTRESS_INTERVAL_SPREAD 60.0f /**< ...plus up to this much. */
#define DISTRESS_TIMER 40.0f           /**< Freighter survival clock. */
#define DISTRESS_DIST_MIN 1400.0f      /**< Scene distance from the player... */
#define DISTRESS_DIST_SPREAD 600.0f    /**< ...plus up to this much. */
#define DISTRESS_SCENE_RADIUS 550.0f   /**< Arriving inside pauses the clock. */
#define DISTRESS_LEASH 2500.0f         /**< Fleeing an ambush this far dissolves it. */
#define DISTRESS_AMBUSH_CHANCE 0.25f
#define DISTRESS_REWARD_BASE 120       /**< Rescue credits... */
#define DISTRESS_REWARD_SPREAD 80      /**< ...plus up to this much. */
#define DISTRESS_AMBUSH_REWARD 60      /**< Decoy salvage for clearing bait. */
#define DISTRESS_RESCUE_SCRAP 8        /**< Jettisoned by a saved freighter. */
#define DISTRESS_WRECK_SCRAP 3         /**< Salvage left by a lost one. */

typedef enum DistressState {
  DISTRESS_IDLE,    /**< No call; roll_timer counts to the next one. */
  DISTRESS_CALLED,  /**< Scene live, clock running, player en route. */
  DISTRESS_FIGHT,   /**< Player on scene; clock held while raiders live. */
} DistressState;

typedef struct Distress {
  DistressState state;
  bool ambush;       /**< This call is bait: no freighter, nastier pack. */
  float timer;       /**< Survival clock while CALLED. */
  float roll_timer;  /**< IDLE: seconds until the next mayday. */
  float spark_timer; /**< Cadence of the under-attack debris sparks. */
  Vec2f pos;         /**< Scene anchor; tracks the freighter while it lives. */
} Distress;

/** Clear state and schedule the first call (new run). */
void distress_reset(Distress *d);

/** Roll for calls, run the scene clock, detect rescue/loss. */
void distress_update(Distress *d, World *world, Entity player, float dt);

/** Where the amber compass arrow should point.
 *  @return false when no call is live. */
bool distress_compass_target(const Distress *d, Vec2f *out);

#endif
