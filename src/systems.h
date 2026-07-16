// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Per-frame ECS systems, called in order from the main loop.
 */

#ifndef _SD_SYSTEMS_H
#define _SD_SYSTEMS_H
#include <SDL3/SDL.h>
#include "ecs.h"
#include "quest.h"

/** Half-extent of the box around the camera inside which entities simulate. */
#define FREEZE_RADIUS 1100.0f
#define FREEZE_WAKE_MARGIN 80.0f  /**< Wake catch-up may not land within this of the view. */

/** Freeze entities far from the camera; on wake, dead-reckon the missed time.
 *  @param now Game time in seconds (App.time), not wall clock. */
void system_freeze(World *world, Vec2f camera, float now);

/** Player input: rotation, thrust (fuel burn, exhaust), firing. */
void system_player(World *world, float dt);

/** Pirate AI: flee station zones, chase/shoot the player, loot scrap, wander. */
void system_pirate(World *world, float dt);

/** Integrate velocity and spin; apply drag, speed clamp and hit-flash decay. */
void system_movement(World *world, float dt);

/** Circle-vs-circle pass: bounce impulses, bullet damage, impact damage. */
void system_collision(World *world);

/** Pull nearby scrap toward the player and collect it in pickup range. */
void system_scrap(World *world, float dt);

/** Tick mine fuses (blink when nearly spent) and detonate expired ones. */
void system_mines(World *world, float dt);

/** Steer missiles toward the nearest pirate in seeker range. */
void system_missiles(World *world, float dt);

/** Steer departing haulers around rocks; despawn them once off screen. */
void system_freighters(World *world, Vec2f camera, float dt);

/** Damage to the player lands on the shield first, then the hull. */
void player_take_damage(Player *player, int dmg);

/** Trigger beacon broadcasts near the player (one per beacon). */
void system_signals(World *world, Quest *quest);

/** Tick lifetimes and destroy expired entities (runs for frozen ones too). */
void system_lifetime(World *world, float dt);

/** Transform blended between the last two sim steps for smooth rendering;
 *  snaps to current on large jumps (spawn, freeze catch-up).
 *  @param alpha Fraction of a sim step elapsed since the last one, [0,1). */
Transform render_transform(const World *world, Entity e, float alpha);

/** Draw crisp wireframes and the player's thruster flame.
 *  view_offset maps world to screen: screen = world + view_offset;
 *  alpha is the fraction of a sim step elapsed since the last one. */
void system_render(World *world, SDL_Renderer *renderer, Vec2f view_offset,
                   float alpha);

/** Draw C_GLOW wireframes into the bloom buffer (between glow_begin/end). */
void system_render_glow(World *world, SDL_Renderer *renderer, Vec2f view_offset,
                        float alpha);

#endif
