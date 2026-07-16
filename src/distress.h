// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Distress calls: rescue events alongside the contract system.
 *
 *  On a random interval a mayday arrives — an amber compass arrow and
 *  a HUD hull readout point at a freighter under raider attack. The
 *  raiders (amber-glowing, counted on the HUD) chew its hull while
 *  they roam free; being on scene pauses the chew (they turn on you).
 *  Killing every tagged raider — from any range — pays credits and
 *  jettisoned scrap. The freighter has a real hull: raider fire,
 *  player fire, mines, and rams all damage it, and breaking it
 *  yourself scatters its cargo but marks you (EV_FREIGHTER_KILLED,
 *  the faction hook). Some calls are bait: an ambush pack, no
 *  freighter, and a phantom hull readout to keep the lie straight.
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
#define FREIGHTER_HP 12.0f             /**< Hauler hull (small-int damage scale). */
#define DISTRESS_HULL_DRAIN 0.3f       /**< Hull/s chewed while raiders roam free. */
#define FREIGHTER_RAM_DAMAGE 12        /**< Dealt to the player on contact. */
#define FREIGHTER_RAM_HIT 2            /**< Dealt to the hauler by that bump. */
#define FREIGHTER_BLAME_DAMAGE 4.0f    /**< Player damage share that earns the
                                            kill blame when the hauler breaks. */
#define FREIGHTER_FLEE_SPEED 190.0f    /**< Departure cruise after a rescue. */
#define FREIGHTER_TURN 60.0f           /**< Deg/s of departure steering. */
#define FREIGHTER_AVOID_RADIUS 240.0f  /**< Steers away from rocks inside this. */
#define FREIGHTER_DESPAWN_MARGIN 120.0f /**< Past the screen edge by this: gone. */
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
  bool visited;      /**< Player has been on scene; leaving past the leash
                          abandons the event. */
  float hull;        /**< HUD copy of the hauler's hull (phantom on bait). */
  int raiders;       /**< HUD copy of the live tagged-raider count. */
  float roll_timer;  /**< IDLE: seconds until the next mayday. */
  float spark_timer; /**< Cadence of the under-attack debris sparks. */
  Vec2f pos;         /**< Scene anchor; tracks the freighter while it lives. */
} Distress;

/** Clear state and schedule the first call (new run). */
void distress_reset(Distress *d);

/** Roll for calls, chew the hull, detect rescue/loss. */
void distress_update(Distress *d, World *world, Entity player, float dt);

/** Damage a hauler. Player damage accumulates; if the hauler breaks
 *  with the player's share past FREIGHTER_BLAME_DAMAGE the kill is
 *  blamed on him (EV_FREIGHTER_KILLED), else EV_DISTRESS_LOST. */
void freighter_hit(World *world, Entity e, int damage, Vec2f impact,
                   bool by_player);

/** Where the amber compass arrow should point.
 *  @return false when no call is live. */
bool distress_compass_target(const Distress *d, Vec2f *out);

#endif
