// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "station.h"
#include "events.h"
#include "faction.h"
#include "mine.h"
#include "missile.h"
#include "replay.h"
#include "ship.h"

const UpgradeDef STATION_UPGRADES[UPGRADE_COUNT] = {
  [UP_HULL]     = { "HULL",      40 },
  [UP_TANK]     = { "FUEL TANK", 30 },
  [UP_FIRERATE] = { "FIRE RATE", 50 },
  [UP_THRUST]   = { "THRUST",    45 },
  [UP_MAGNET]   = { "MAGNET",    25 },
};

/** Pastel-neon outpost identities, so far-flung stations are
 *  distinguishable landmarks. */
static const SDL_Color PALETTE[STATION_PALETTE_COUNT] = {
  { 150, 255, 200, 255 },  // mint
  { 255, 160, 220, 255 },  // pink
  { 190, 160, 255, 255 },  // lavender
  { 255, 190, 140, 255 },  // peach
  { 150, 200, 255, 255 },  // sky
  { 240, 255, 150, 255 },  // lemon
};

SDL_Color station_palette(int index) {
  return PALETTE[index % STATION_PALETTE_COUNT];
}

static SDL_Color brighten(SDL_Color c) {
  c.r = (Uint8)(c.r + (255 - c.r) * 2 / 5);
  c.g = (Uint8)(c.g + (255 - c.g) * 2 / 5);
  c.b = (Uint8)(c.b + (255 - c.b) * 2 / 5);
  return c;
}

/** One rotating polygon ring of a station body. */
static Entity spawn_ring(World *world, Vec2f position, int points,
                         float radius, float spin, SDL_Color color) {
  Entity e = entity_create(world);
  world->masks[e] = C_TRANSFORM | C_VELOCITY | C_WIREFRAME | C_GLOW;

  world->transforms[e] = (Transform){ .position = position, .angle = 0.0f };
  world->velocities[e] = (Velocity){ .damping = 0.0f, .spin = spin };

  Wireframe *wf = &world->wireframes[e];
  for (int i = 0; i < points; i++) {
    float ang = (float)i / (float)points * TWO_PI;
    wf->points[i] = vec2f_mul(vec2f_dir(ang), radius);
  }
  wf->point_count = points;
  wf->color      = color;
  wf->glow_color = color;

  return e;
}

/** A counter-rotating ring pair: colored octagon outside (the dock
 *  anchor), brighter square inside. No collider — the dock radius is
 *  the interaction, not the geometry. */
Entity station_spawn(World *world, Vec2f position, SDL_Color color) {
  Entity outer = spawn_ring(world, position, 8, 90.0f, 4.0f, color);
  world->masks[outer] |= C_STATION;

  spawn_ring(world, position, 4, 45.0f, -10.0f, brighten(color));

  return outer;
}

// Per-dock/session state; station_reset clears it for a new run
static bool shield_stocked = false;
static bool mine_stocked = false;
static bool missile_stocked = false;
static bool was_docked = false;
static float refuel_acc = 0.0f;
static float repair_acc = 0.0f;
static float debt_acc = 0.0f;
static float sell_timer = 0.0f;

void station_reset(void) {
  shield_stocked = false;
  mine_stocked = false;
  missile_stocked = false;
  was_docked = false;
  refuel_acc = repair_acc = debt_acc = sell_timer = 0.0f;
}

bool station_shield_in_stock(void) { return shield_stocked; }

bool station_mine_in_stock(void) { return mine_stocked; }

bool station_try_buy_mine(World *world, Entity player) {
  if (!station_docked(world, player)) return false;

  Player *p = &world->players[player];
  if (p->mines_max == 0) {
    // First purchase: the rack itself, in stock or not at all
    if (!mine_stocked) return false;
    int cost = station_price(world, STATION_MINE_RACK_PRICE);
    if (p->money < cost) return false;

    p->money -= cost;
    p->mines_max = MINE_AMMO_MAX;
    p->mines = MINE_AMMO_MAX;
    mine_stocked = false;  // sold
    events_emit(EV_MINE_RACK_BOUGHT, world->transforms[player].position);
    return true;
  }

  // Owned: any station restocks, one mine per press
  if (p->mines >= p->mines_max) return false;
  int cost = station_price(world, STATION_MINE_PRICE);
  if (p->money < cost) return false;

  p->money -= cost;
  p->mines++;
  events_emit(EV_SCRAP_SOLD, world->transforms[player].position);
  return true;
}

bool station_missile_in_stock(void) { return missile_stocked; }

bool station_try_buy_missile(World *world, Entity player) {
  if (!station_docked(world, player)) return false;

  Player *p = &world->players[player];
  if (p->missiles_max == 0) {
    // First purchase: the pod itself, in stock or not at all
    if (!missile_stocked) return false;
    int cost = station_price(world, STATION_MISSILE_POD_PRICE);
    if (p->money < cost) return false;

    p->money -= cost;
    p->missiles_max = MISSILE_AMMO_MAX;
    p->missiles = MISSILE_AMMO_MAX;
    missile_stocked = false;  // sold
    events_emit(EV_MISSILE_POD_BOUGHT, world->transforms[player].position);
    return true;
  }

  // Owned: any station restocks, one missile per press
  if (p->missiles >= p->missiles_max) return false;
  int cost = station_price(world, STATION_MISSILE_PRICE);
  if (p->money < cost) return false;

  p->money -= cost;
  p->missiles++;
  events_emit(EV_SCRAP_SOLD, world->transforms[player].position);
  return true;
}

bool station_try_buy_shield(World *world, Entity player) {
  if (!station_docked(world, player) || !shield_stocked) return false;

  Player *p = &world->players[player];
  if (p->shield_max > 0) return false;  // already installed
  int cost = station_price(world, STATION_SHIELD_PRICE);
  if (p->money < cost) return false;

  p->money -= cost;
  p->shield_max = SHIP_SHIELD_MAX;
  p->shield = SHIP_SHIELD_MAX;
  shield_stocked = false;  // sold
  events_emit(EV_UPGRADE_BOUGHT, world->transforms[player].position);
  return true;
}

Entity station_nearest(World *world, Vec2f pos) {
  Entity best = MAX_ENTITIES;
  float best_d = 1e30f;

  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_STATION | C_TRANSFORM)) continue;

    float d = vec2f_length(vec2f_sub(world->transforms[e].position, pos));
    if (d < best_d) {
      best_d = d;
      best = e;
    }
  }
  return best;
}

bool station_docked(World *world, Entity player) {
  if (!entity_has(world, player, C_PLAYER | C_TRANSFORM)) return false;

  Vec2f player_pos = world->transforms[player].position;
  Entity station = station_nearest(world, player_pos);
  if (station == MAX_ENTITIES) return false;

  Vec2f delta = vec2f_sub(world->transforms[station].position, player_pos);
  return vec2f_length(delta) < STATION_DOCK_RADIUS;
}

/** Selling is automatic; fuel and repair are bought by holding their
 *  keys, one credit-quantum at a time so the HUD counters visibly
 *  trade against each other. */
void station_update(World *world, Entity player, float dt) {
  bool docked = station_docked(world, player);
  Player *p = &world->players[player];

  // On dock entry, roll what special stock this stop carries —
  // positive guild standing raises the odds
  if (docked && !was_docked && entity_has(world, player, C_PLAYER)) {
    int goodwill = world->factions.standing[FACTION_GUILD];
    float chance = STATION_SHIELD_BASE_CHANCE +
                   STATION_SHIELD_STANDING_BONUS *
                       (float)(goodwill > 0 ? goodwill : 0);
    if (chance > STATION_SHIELD_MAX_CHANCE) chance = STATION_SHIELD_MAX_CHANCE;
    shield_stocked = world_randf(world) < chance;
    mine_stocked = world_randf(world) < chance;     // independent rolls
    missile_stocked = world_randf(world) < chance;

    events_emit(EV_DOCKED, world->transforms[player].position);
  }
  was_docked = docked;
  if (!docked) return;

  if (p->fuel < STATION_EMERGENCY_FUEL) {
    p->fuel += STATION_EMERGENCY_RATE * dt;
    if (p->fuel > STATION_EMERGENCY_FUEL) p->fuel = STATION_EMERGENCY_FUEL;
  }

  if (sim_input(ACT_REFUEL) && p->money > 0 && p->fuel < p->fuel_max) {
    refuel_acc += STATION_REFUEL_RATE * dt;
    while (refuel_acc >= STATION_FUEL_PER_CREDIT) {
      refuel_acc -= STATION_FUEL_PER_CREDIT;
      if (p->money <= 0 || p->fuel >= p->fuel_max) break;
      p->money--;
      p->fuel += STATION_FUEL_PER_CREDIT / faction_price_scale(world);
      if (p->fuel > p->fuel_max) p->fuel = p->fuel_max;
      events_emit(EV_REFUEL_TICK, world->transforms[player].position);
    }
  } else {
    refuel_acc = 0.0f;
  }

  if (sim_input(ACT_REPAIR) && p->money > 0 && p->hp < p->max_hp) {
    repair_acc += STATION_REPAIR_RATE * dt;
    while (repair_acc >= (float)STATION_HULL_PER_CREDIT) {
      repair_acc -= (float)STATION_HULL_PER_CREDIT;
      if (p->money <= 0 || p->hp >= p->max_hp) break;
      p->money--;
      int hull = (int)SDL_roundf((float)STATION_HULL_PER_CREDIT /
                                 faction_price_scale(world));
      p->hp += hull > 0 ? hull : 1;
      if (p->hp > p->max_hp) p->hp = p->max_hp;
      events_emit(EV_REPAIR_TICK, world->transforms[player].position);
    }
  } else {
    repair_acc = 0.0f;
  }

  if (sim_input(ACT_PAY_DEBT) && p->money > 0 && p->debt > 0) {
    bool paid = false;
    debt_acc += STATION_DEBT_RATE * dt;
    while (debt_acc >= (float)STATION_DEBT_QUANTUM) {
      debt_acc -= (float)STATION_DEBT_QUANTUM;
      if (p->money <= 0 || p->debt <= 0) break;

      int pay = STATION_DEBT_QUANTUM;
      if (pay > p->money) pay = p->money;
      if (pay > p->debt) pay = p->debt;
      p->money -= pay;
      p->debt -= pay;
      paid = true;

      if (p->debt == 0) {
        faction_add(world, FACTION_GUILD, 10);
        events_emit(EV_DEBT_PAID, world->transforms[player].position);
      }
    }
    if (paid && p->debt > 0) {
      events_emit(EV_DEBT_PAYMENT, world->transforms[player].position);
    }
  } else {
    debt_acc = 0.0f;
  }

  bool sold = false;
  sell_timer -= dt;
  while (sell_timer <= 0.0f && p->scrap > 0) {
    p->scrap--;
    p->money += SCRAP_VALUE;
    sell_timer += STATION_SELL_INTERVAL;
    sold = true;
  }
  if (p->scrap == 0) sell_timer = 0.0f;
  if (sold) events_emit(EV_SCRAP_SOLD, world->transforms[player].position);
}

int station_price(const World *world, int base) {
  int cost = (int)SDL_ceilf((float)base * faction_price_scale(world));
  return cost > 0 ? cost : 1;
}

int station_upgrade_cost(const World *world, Entity player, UpgradeId id) {
  int level = world->players[player].upgrades[id];
  return station_price(world, STATION_UPGRADES[id].base_cost * (level + 1));
}

bool station_try_buy(World *world, Entity player, UpgradeId id) {
  if (id >= UPGRADE_COUNT) return false;
  if (!station_docked(world, player)) return false;

  Player *p = &world->players[player];
  int cost = station_upgrade_cost(world, player, id);
  if (p->money < cost) return false;

  p->money -= cost;
  p->upgrades[id]++;

  switch (id) {
  case UP_HULL:
    p->max_hp += 25;
    p->hp = p->max_hp;
    break;
  case UP_TANK:
    p->fuel_max += 30.0f;
    break;
  case UP_FIRERATE:
    p->fire_interval *= 0.925f;
    break;
  case UP_THRUST:
    p->thrust_force *= 1.15f;
    break;
  case UP_MAGNET:
    p->magnet_radius += 50.0f;
    break;
  default:
    break;
  }

  events_emit(EV_UPGRADE_BOUGHT, world->transforms[player].position);
  return true;
}
