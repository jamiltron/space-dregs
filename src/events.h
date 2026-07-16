// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Game event queue: simulation emits, presentation consumes.
 *
 *  Systems and factories report what happened (events_emit); once per
 *  frame events_drain maps each event to its presentation response
 *  (sound, screen shake) from a single table. Simulation code never
 *  touches audio or fx directly, so it stays headless-testable and
 *  new juice lands in one place.
 */

#ifndef _SD_EVENTS_H
#define _SD_EVENTS_H
#include "math2d.h"

typedef enum EventType {
  EV_PLAYER_FIRED,
  EV_PIRATE_FIRED,
  EV_ASTEROID_CHIPPED,
  EV_ASTEROID_BROKE,      /**< Split into children. */
  EV_ASTEROID_DESTROYED,  /**< Final-generation burst. */
  EV_PIRATE_CHIPPED,
  EV_PIRATE_DESTROYED,
  EV_PLAYER_HURT,
  EV_PLAYER_DIED,
  EV_SCRAP_PICKUP,
  EV_SCRAP_SOLD,
  EV_UPGRADE_BOUGHT,
  EV_DEBT_PAYMENT,        /**< A payment tick streamed at the station. */
  EV_DEBT_PAID,           /**< The whole loan cleared — endgame hook. */
  EV_DOCKED,              /**< Arrived at a station (attendant speaks). */
  EV_SIGNAL,              /**< A beacon broadcast received. */
  EV_SIGNAL_CONTRACT,     /**< Beacon carried a bounty posting. */
  EV_QUEST_ACCEPTED,
  EV_QUEST_ITEM,          /**< Beacon scooped up. */
  EV_QUEST_COMPLETE,
  EV_MINE_DROPPED,
  EV_MINE_EXPLODED,
  EV_MISSILE_FIRED,
  EV_REFUEL_TICK,         /**< One credit of fuel bought. */
  EV_REPAIR_TICK,         /**< One credit of hull bought. */
  EV_MINE_RACK_BOUGHT,    /**< Dialogue announces the drop key. */
  EV_MISSILE_POD_BOUGHT,  /**< Dialogue announces the launch key. */
  EV_TUTORIAL_DONE,       /**< First contract taken; starter bonus paid. */
  EV_DRONE_DEPLOYED,      /**< A mothership bay spat out a kamikaze drone. */
  EV_DISTRESS_CALL,       /**< Mayday received; amber marker on nav. */
  EV_DISTRESS_ARRIVED,    /**< On scene with the freighter still alive. */
  EV_DISTRESS_SAVED,      /**< Raiders cleared; rescue paid out. */
  EV_DISTRESS_LOST,       /**< The clock ran out; only a wreck left. */
  EV_DISTRESS_AMBUSH,     /**< The call was bait and it just sprang. */
  EV_DISTRESS_CLEARED,    /**< Bait pack destroyed; decoy salvage paid. */
  EV_FUEL_LOW,            /**< Fuel crossed into the red, or 10 lower. */
  EV_HULL_LOW,            /**< Hull crossed into the red, or 10 lower. */
  EV_FREIGHTER_KILLED,    /**< The player broke a hauler — future faction hook. */
  EV_DISTRESS_ABANDONED,  /**< Answered the call, then flew off past the leash. */
  EVENT_TYPE_COUNT
} EventType;

/** Queue an event; pos is where it happened (drops silently when full). */
void events_emit(EventType type, Vec2f pos);

/** Consume and clear the queue; call once per frame after the systems. */
void events_drain(void);

#endif
