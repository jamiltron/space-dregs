// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Faction standings and clan heat.
 *
 *  Two run-scoped meters on World.factions: GUILD standing moves with
 *  contracts, rescues, debt payments and hauler kills; CLANS standing
 *  drops per pirate killed and rises with piracy. Sustained pirate
 *  killing also builds heat, which sends hunter packs. All mutations
 *  happen inside sim steps so replays and saves stay deterministic.
 *  Future factions: the Lender (debt holder, endgame), independent
 *  haulers (escort work), per-palette station houses.
 */

#ifndef _SD_FACTION_H
#define _SD_FACTION_H
#include <stdbool.h>
#include "ecs.h"

#define FACTION_STANDING_MAX 100
#define FACTION_KILL_STANDING 1  /**< Clan standing lost per pirate kill. */
#define FACTION_KILL_HEAT 1.0f   /**< Heat per pirate kill. */
#define FACTION_CAPITAL_HEAT 3.0f /**< Heat per battleship/mothership kill. */
#define FACTION_HEAT_DECAY 0.1f  /**< Heat lost per second. */
#define FACTION_HEAT_HUNT 8.0f   /**< Heat where hunter packs start. */
#define FACTION_HEAT_TIER2 16.0f /**< Packs add a brute. */
#define FACTION_HEAT_TIER3 24.0f /**< Packs add a battleship. */
#define FACTION_HUNTER_DELAY_MIN 45.0f  /**< Seconds between packs... */
#define FACTION_HUNTER_DELAY_SPREAD 25.0f /**< ...plus up to this much. */
#define FACTION_HUNTER_DIST_MIN 1300.0f /**< Pack spawn distance... */
#define FACTION_HUNTER_DIST_SPREAD 400.0f /**< ...plus up to this much. */
#define FACTION_HUNTER_LEASH 3000.0f /**< Hunters this far behind revert to wild. */

#define FACTION_TIER 20           /**< Standing where perks/penalties start. */
#define FACTION_TIER_STRONG 50    /**< Standing where they deepen. */
#define FACTION_DELTA_SECS 3.0f   /**< HUD display time for a standing change. */

/** Adjust a standing, clamped to +/-FACTION_STANDING_MAX; emits
 *  threshold-crossing events for dialogue. */
void faction_add(World *world, FactionId id, int delta);

/** Station price multiplier from guild standing (0.8x to 2.0x). */
float faction_price_scale(const World *world);

/** False while the guild refuses to deal contracts (standing <= -20). */
bool faction_board_open(const World *world);

/** Scale on wild pirates' sense radius toward the player: 0.5 at
 *  clan standing >= 20, 0 at >= 40. Provoked pirates and tagged
 *  raiders/hunters ignore this. */
float faction_sense_scale(const World *world);

/** Decay heat, manage the hunter leash, roll hunter-pack spawns. */
void faction_update(World *world, Entity player, float dt);

/** A player pirate kill: clan standing drops; heat rises unless the
 *  victim was a hunter. */
void faction_on_pirate_kill(World *world, int archetype, bool was_hunter);

#endif
