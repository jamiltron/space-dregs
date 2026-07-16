// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Component bitmask and component structs for the ECS.
 */

#ifndef _SD_COMPONENTS_H
#define _SD_COMPONENTS_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "math2d.h"

/** Per-entity bitmask; a set bit means the matching component array
 *  slot is valid. Systems select with (mask & wanted) == wanted. */
typedef Uint32 ComponentMask;

enum {
  C_NONE      = 0,
  C_TRANSFORM = 1 << 0,
  C_VELOCITY  = 1 << 1,
  C_PLAYER    = 1 << 2,
  C_WIREFRAME = 1 << 3,
  C_GLOW      = 1 << 4,  /**< Wireframe feeds the bloom pass. */
  C_COLLIDER  = 1 << 5,  /**< Circle collision body. */
  C_BULLET    = 1 << 6,  /**< Damages on contact, never bounces. */
  C_ASTEROID  = 1 << 7,  /**< Splits when destroyed. */
  C_LIFETIME  = 1 << 8,  /**< Despawns on expiry, fades out. */
  C_SCRAP     = 1 << 9,  /**< Magnets to the player, collectible. */
  C_STATION   = 1 << 10, /**< Dock anchor for refuel/sell/upgrades. */
  C_PIRATE    = 1 << 11, /**< Hostile AI ship. */
  C_FROZEN    = 1 << 12, /**< Far off-screen: skipped by systems, catches up on wake. */
  C_QUEST_TARGET = 1 << 13,  /**< The active quest's objective entity. */
  C_SIGNAL    = 1 << 14, /**< Broadcast beacon; speaks once when approached. */
  C_MINE      = 1 << 15, /**< Dropped charge: timed fuse, area blast. */
  C_MISSILE   = 1 << 16, /**< Seeking warhead; guided by system_missiles. */
  C_DISTRESS  = 1 << 17, /**< Distress-scene member (freighter or raider). */
  C_FREIGHTER = 1 << 18, /**< Neutral hauler: real hull, bleeds cargo when broken. */
};

/** Station upgrades, bought with credits; levels live on the Player. */
typedef enum UpgradeId {
  UP_HULL,      /**< +max hull, full repair. */
  UP_TANK,      /**< +fuel capacity. */
  UP_FIRERATE,  /**< Faster shooting. */
  UP_THRUST,    /**< Stronger engine. */
  UP_MAGNET,    /**< Wider scrap magnet. */
  UPGRADE_COUNT
} UpgradeId;

/** World-space position and heading. */
typedef struct Transform {
  Vec2f position;
  float angle;  /**< Degrees, clockwise (screen Y increases downward). */
} Transform;

/** Linear and angular motion, integrated by system_movement. */
typedef struct Velocity {
  Vec2f value;
  float damping;    /**< Exponential velocity decay per second; 0 = coasts forever. */
  float max_speed;  /**< <= 0 means unclamped. */
  float spin;       /**< Deg/s applied to Transform.angle. */
} Velocity;

/** The player's ship: control tuning, resources and progression. */
typedef struct Player {
  float rot_speed;       /**< Degrees per second. */
  float thrust_force;
  float exhaust_offset;  /**< Center to exhaust nozzle, along -facing. */
  float flame_length;
  float muzzle_offset;   /**< Center to bullet spawn, along facing. */
  float fire_interval;   /**< Seconds between shots (upgradeable). */
  float fire_cooldown;   /**< Seconds until the next shot is allowed. */
  float damage_timer;    /**< I-frames remaining; > 0 also drives the flicker. */
  float fuel;            /**< Thrust burns it; an empty tank means drifting. */
  float fuel_max;
  float exhaust_timer;   /**< Paces thrust particle emission. */
  float magnet_radius;   /**< Scrap pull range (upgradeable). */
  float shield_regen;    /**< Seconds accumulated toward the next shield point. */
  float fuel_warn_next;  /**< Fuel level at/below which the low-fuel chip fires. */
  int hull_warn_next;    /**< Hull level at/below which the low-hull siren fires. */
  int shield;            /**< Rechargeable layer absorbed before hull. */
  int shield_max;        /**< 0 until the energy shield is bought. */
  int mines;             /**< Mine ammo aboard; restocked at stations. */
  int mines_max;         /**< 0 until the mine rack is bought. */
  int missiles;          /**< Missile ammo aboard; restocked at stations. */
  int missiles_max;      /**< 0 until the missile pod is bought. */
  bool mine_latch;       /**< Previous drop-key state; drops fire on press. */
  bool missile_latch;    /**< Previous launch-key state; one shot per press. */
  int hp;
  int max_hp;
  int scrap;             /**< Collected scrap, cargo until sold. */
  int money;             /**< Credits from sold scrap. */
  int debt;              /**< The ship's loan; paying it off is the goal. */
  int bounties_done;     /**< Completed bounty contracts; raises shield stock odds. */
  int upgrades[UPGRADE_COUNT];  /**< Purchase levels. */
  bool thrusting;
} Player;

/** Circle collision body. */
typedef struct Collider {
  float radius;  /**< Mass derives from radius^2 in collision response. */
} Collider;

/** Bullet allegiance and punch. */
typedef struct Bullet {
  bool hostile;  /**< Pirate-fired: hurts the player, not pirates. */
  int damage;    /**< Dealt to pirates/asteroids on contact (missiles hit harder). */
} Bullet;

/** Pirate AI state. */
typedef struct Pirate {
  float fire_cooldown;
  float missile_cooldown;  /**< Only ticks for archetypes with a missile_interval. */
  float deploy_cooldown;   /**< Only ticks for archetypes with a deploy_interval. */
  float wander_timer;  /**< Time until the next idle heading change. */
  float wander_dir;    /**< Idle heading, degrees. */
  int hp;
  int loot;            /**< Scrap collected; drops on death. */
  float leak_timer;    /**< Counts up while loot is held; leaks 1 per interval. */
  int archetype;       /**< PirateArchetype; indexes the stats table. */
} Pirate;

/** Bookkeeping for off-screen simulation freezing. */
typedef struct Frozen {
  float since;  /**< Game time when frozen; drives catch-up extrapolation. */
} Frozen;

/** A space beacon's broadcast payload. */
typedef struct Signal {
  bool contract;  /**< Grants a bounty contract instead of just talking. */
} Signal;

/** A dropped mine counting down to its blast. */
typedef struct Mine {
  float fuse;  /**< Seconds until detonation. */
} Mine;

/** Minable rock. */
/** Neutral hauler. */
typedef struct Freighter {
  float hp;             /**< Small-int damage scale. */
  float player_damage;  /**< Player's share; drives blame on destruction. */
} Freighter;

typedef struct Asteroid {
  int generation;  /**< 0 = full size; splits until the last generation. */
  int hp;          /**< Hits left, scales with radius. */
  float radius;    /**< Base shape radius, children scale down from it. */
  int kind;        /**< AsteroidKind; rich rocks glow gold and pay big. */
} Asteroid;

/** Countdown to despawn; render fades alpha by remaining/initial. */
typedef struct Lifetime {
  float remaining;  /**< Seconds left. */
  float initial;    /**< Starting value, drives the fade-out. */
} Lifetime;

#define WIREFRAME_MAX_POINTS 16

/** Renderable line shape. */
typedef struct Wireframe {
  Vec2f points[WIREFRAME_MAX_POINTS];  /**< Local space, drawn as a closed loop. */
  int point_count;
  float flash;           /**< Seconds of white hit-flash remaining. */
  SDL_Color color;       /**< Core line color. */
  SDL_Color glow_color;  /**< Bloom tint when the entity has C_GLOW. */
} Wireframe;

#endif
