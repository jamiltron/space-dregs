// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Procedural chunked world generation.
 *
 *  Space is a grid of CHUNK_SIZE cells. As the camera nears a cell for
 *  the first time it is populated from a per-run seed hashed with the
 *  cell coords — deterministic layout, denser rocks and more pirates
 *  (and rare outpost stations) farther from home. Cells populate
 *  exactly once; their contents then live as normal entities.
 *  Asteroids landing inside the current viewport are dropped so rocks
 *  never pop into view (the camera is sim-derived, so replays hold).
 */

#ifndef _SD_CHUNKS_H
#define _SD_CHUNKS_H
#include "ecs.h"

#define CHUNK_SIZE 1200.0f
#define CHUNK_TABLE_CAP 4096            /**< Power of two; visited-cell hash set. */
#define CHUNK_SAFE_RADIUS 420.0f        /**< No spawns this close to home. */
#define CHUNK_ROCK_RATE 1.5f            /**< Asteroid count multiplier per cell. */
#define CHUNK_VIEW_MARGIN 80.0f         /**< Rocks spawn at least this far past the screen edge. */
#define CHUNK_PIRATE_MIN_DIST 800.0f
#define CHUNK_STATION_MIN_DIST 2500.0f  /**< Wild stations only spawn past this. */
#define CHUNK_STATION_CHANCE 0.05f      /**< Per chunk, out in the deep field. */
#define CHUNK_ENTITY_HEADROOM 128       /**< Stop generating when this near full. */

/** Visited-cell set plus the run's generation seed. */
typedef struct Chunks {
  Uint64 keys[CHUNK_TABLE_CAP];
  bool used[CHUNK_TABLE_CAP];
  int count;
  Uint64 seed;
  Vec2f home;  /**< Station position; distance from here scales danger. */
} Chunks;

/** Reset the visited set and store the run's seed and home. */
void chunks_init(Chunks *ck, Uint64 seed, Vec2f home);

/** Populate any unvisited cells in the 3x3 around the camera. */
void chunks_update(Chunks *ck, World *world, Vec2f camera);

/** Mark a cell as already generated without populating it (save load:
 *  its entities are restored from the save instead of re-rolled). */
void chunks_restore_cell(Chunks *ck, int cx, int cy);

#endif
