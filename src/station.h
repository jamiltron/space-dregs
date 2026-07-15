// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Stations: docking, paid services (fuel/repair), scrap selling,
 *  upgrades, and pirate sanctuary zones.
 */

#ifndef _SD_STATION_H
#define _SD_STATION_H
#include <stdbool.h>
#include "ecs.h"

#define STATION_DOCK_RADIUS 120.0f
#define STATION_SAFE_ZONE 520.0f     /**< Pirates won't enter or fight inside. */
#define STATION_REFUEL_RATE 40.0f    /**< Fuel per second while holding refuel. */
#define STATION_REPAIR_RATE 12.0f    /**< Hull per second while holding repair. */
#define STATION_FUEL_PER_CREDIT 5.0f
#define STATION_HULL_PER_CREDIT 2
#define STATION_EMERGENCY_FUEL 20.0f /**< Free trickle up to here — never stranded. */
#define STATION_EMERGENCY_RATE 8.0f
#define STATION_SELL_INTERVAL 0.08f
#define SCRAP_VALUE 5                /**< Credits per scrap sold. */
#define STATION_SHIELD_PRICE 150         /**< Energy shield price. */
#define STATION_MINE_RACK_PRICE 140      /**< Mine rack, arrives loaded. */
#define STATION_MINE_PRICE 20            /**< One replacement mine. */
#define STATION_MISSILE_POD_PRICE 160    /**< Missile pod, arrives loaded. */
#define STATION_MISSILE_PRICE 25         /**< One replacement missile. */
#define STATION_SHIELD_BASE_CHANCE 0.15f /**< Stock odds per dock... */
#define STATION_SHIELD_BOUNTY_BONUS 0.20f/**< ...plus this per bounty done. */
#define STATION_SHIELD_MAX_CHANCE 0.90f
#define STATION_DEBT_RATE 150.0f     /**< Credits per second while holding pay. */
#define STATION_DEBT_QUANTUM 5       /**< Credits per payment tick. */

/** Home is classic teal; wild outposts pick from a pastel-neon palette. */
#define STATION_HOME_COLOR (SDL_Color){ 80, 200, 220, 255 }
#define STATION_PALETTE_COUNT 6

/** One upgrade's display name and pricing base. */
typedef struct UpgradeDef {
  const char *name;
  int base_cost;  /**< Actual cost = base_cost * (level + 1). */
} UpgradeDef;

extern const UpgradeDef STATION_UPGRADES[UPGRADE_COUNT];

/** Outpost color by index (wraps). */
SDL_Color station_palette(int index);

/** Spawn a station (two counter-rotating rings) at position. */
Entity station_spawn(World *world, Vec2f position, SDL_Color color);

/** Reset per-dock state (stock roll, service accumulators) for a new run. */
void station_reset(void);

/** True while the docked station has the energy shield in stock. */
bool station_shield_in_stock(void);

/** True while the docked station has the mine rack in stock. */
bool station_mine_in_stock(void);

/** True while the docked station has the missile pod in stock. */
bool station_missile_in_stock(void);

/** Buy the mine rack if in stock (comes loaded); once owned, buys one
 *  replacement mine per call instead. @return true on any purchase. */
bool station_try_buy_mine(World *world, Entity player);

/** Buy the missile pod if in stock (comes loaded); once owned, buys
 *  one replacement missile per call instead. @return true on purchase. */
bool station_try_buy_missile(World *world, Entity player);

/** Buy the energy shield if docked, in stock, and affordable.
 *  @return true on purchase. */
bool station_try_buy_shield(World *world, Entity player);

/** Closest station to pos, or MAX_ENTITIES if none exist. */
Entity station_nearest(World *world, Vec2f pos);

/** True while the player is within dock range of any station. */
bool station_docked(World *world, Entity player);

/** Docked services: emergency fuel, hold-to-buy fuel/repair, auto-sell. */
void station_update(World *world, Entity player, float dt);

/** Price of the next level of an upgrade for this player. */
int station_upgrade_cost(const World *world, Entity player, UpgradeId id);

/** Buy an upgrade if docked and affordable.
 *  @return true on purchase. */
bool station_try_buy(World *world, Entity player, UpgradeId id);

#endif
