// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include "chunks.h"
#include "app.h"
#include "asteroid.h"
#include "pirate.h"
#include "signal.h"
#include "station.h"

void chunks_init(Chunks *ck, Uint64 seed, Vec2f home) {
  SDL_memset(ck, 0, sizeof(*ck));
  ck->seed = seed;
  ck->home = home;
}

/** Splitmix-style hash of (seed, cx, cy): a cell always rolls the
 *  same layout no matter when it's first visited. */
static Uint64 chunk_rng_state(Uint64 seed, int cx, int cy) {
  Uint64 h = seed ^ ((Uint64)(Uint32)cx * 0x9E3779B97F4A7C15ULL) ^
             ((Uint64)(Uint32)cy * 0xC2B2AE3D27D4EB4FULL);
  h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ULL;
  h ^= h >> 27; h *= 0x94D049BB133111EBULL;
  h ^= h >> 31;
  return h | 1;  // never zero
}

/** Open-addressed insert-only hash set of visited cells.
 *  @return true if the cell was newly marked (needs populating). */
static bool chunks_mark(Chunks *ck, int cx, int cy) {
  // Leave probe slack; past this the frontier just stops growing
  if (ck->count >= CHUNK_TABLE_CAP - CHUNK_TABLE_CAP / 8) return false;

  Uint64 key = ((Uint64)(Uint32)cx << 32) | (Uint64)(Uint32)cy;
  Uint64 idx = (key * 0x9E3779B97F4A7C15ULL) & (CHUNK_TABLE_CAP - 1);

  while (ck->used[idx]) {
    if (ck->keys[idx] == key) return false;  // already generated
    idx = (idx + 1) & (CHUNK_TABLE_CAP - 1);
  }

  ck->used[idx] = true;
  ck->keys[idx] = key;
  ck->count++;
  return true;
}

/** Would a spawn here be inside the camera's view (plus margin)? */
static bool on_screen(Vec2f pos, Vec2f camera) {
  return fabsf(pos.x - camera.x) < WINDOW_WIDTH / 2.0f + CHUNK_VIEW_MARGIN &&
         fabsf(pos.y - camera.y) < WINDOW_HEIGHT / 2.0f + CHUNK_VIEW_MARGIN;
}

/** Fill a fresh cell; density and danger scale with distance from home. */
static void chunk_populate(Chunks *ck, World *world, int cx, int cy,
                           Vec2f camera) {
  Uint64 rng = chunk_rng_state(ck->seed, cx, cy);
  Vec2f base = vec2f_new((float)cx * CHUNK_SIZE, (float)cy * CHUNK_SIZE);
  Vec2f center = vec2f_add(base, vec2f_new(CHUNK_SIZE / 2.0f, CHUNK_SIZE / 2.0f));
  float home_dist = vec2f_length(vec2f_sub(center, ck->home));

  Vec2f outpost = { 0 };
  bool has_outpost = false;
  if (home_dist > CHUNK_STATION_MIN_DIST &&
      SDL_randf_r(&rng) < CHUNK_STATION_CHANCE) {
    // Keep it off the chunk edges so its safe zone stays mostly local
    outpost = vec2f_add(base,
                        vec2f_new((0.25f + SDL_randf_r(&rng) * 0.5f) * CHUNK_SIZE,
                                  (0.25f + SDL_randf_r(&rng) * 0.5f) * CHUNK_SIZE));
    station_spawn(world, outpost,
                  station_palette(SDL_rand_r(&rng, STATION_PALETTE_COUNT)));
    has_outpost = true;
  }

  int rocks = 3 + SDL_rand_r(&rng, 3);
  if (home_dist > 1500.0f) rocks += 1 + SDL_rand_r(&rng, 2);
  if (home_dist > 3000.0f) rocks += 2;
  rocks = (int)((float)rocks * CHUNK_ROCK_RATE);

  for (int i = 0; i < rocks; i++) {
    Vec2f pos = vec2f_add(base, vec2f_new(SDL_randf_r(&rng) * CHUNK_SIZE,
                                          SDL_randf_r(&rng) * CHUNK_SIZE));
    if (on_screen(pos, camera)) continue;  // never pop into the frame
    if (vec2f_length(vec2f_sub(pos, ck->home)) < CHUNK_SAFE_RADIUS) continue;
    if (has_outpost &&
        vec2f_length(vec2f_sub(pos, outpost)) < CHUNK_SAFE_RADIUS) continue;

    float radius = ASTEROID_MIN_RADIUS +
                   SDL_randf_r(&rng) * (ASTEROID_MAX_RADIUS - ASTEROID_MIN_RADIUS);
    asteroid_spawn(world, pos, radius);
  }

  if (SDL_randf_r(&rng) < 0.10f) {
    Vec2f pos = vec2f_add(base, vec2f_new(SDL_randf_r(&rng) * CHUNK_SIZE,
                                          SDL_randf_r(&rng) * CHUNK_SIZE));
    if (vec2f_length(vec2f_sub(pos, ck->home)) > CHUNK_SAFE_RADIUS) {
      signal_spawn(world, pos, SDL_randf_r(&rng) < 0.35f);
    }
  }

  if (home_dist > CHUNK_PIRATE_MIN_DIST) {
    float chance = (home_dist - CHUNK_PIRATE_MIN_DIST) / 2500.0f;
    if (chance > 0.75f) chance = 0.75f;

    if (SDL_randf_r(&rng) < chance) {
      int pirates = 1 + SDL_rand_r(&rng, 2);
      for (int i = 0; i < pirates; i++) {
        Vec2f pos = vec2f_add(base, vec2f_new(SDL_randf_r(&rng) * CHUNK_SIZE,
                                              SDL_randf_r(&rng) * CHUNK_SIZE));
        if (vec2f_length(vec2f_sub(pos, ck->home)) < STATION_SAFE_ZONE) continue;
        if (has_outpost &&
            vec2f_length(vec2f_sub(pos, outpost)) < STATION_SAFE_ZONE) continue;

        // One roll, partitioned into archetype slices, else dart
        PirateArchetype type = PIRATE_DART;
        float roll = SDL_randf_r(&rng);
        if (home_dist > PIRATE_DRONE_MIN_DIST && roll < PIRATE_DRONE_CHANCE) {
          type = PIRATE_DRONE;
        } else if (home_dist > PIRATE_BRUTE_MIN_DIST &&
                   roll < PIRATE_DRONE_CHANCE + PIRATE_BRUTE_CHANCE) {
          type = PIRATE_BRUTE;
        } else if (home_dist > PIRATE_BATTLESHIP_MIN_DIST &&
                   roll < PIRATE_DRONE_CHANCE + PIRATE_BRUTE_CHANCE +
                          PIRATE_BATTLESHIP_CHANCE) {
          type = PIRATE_BATTLESHIP;
        } else if (home_dist > PIRATE_MOTHERSHIP_MIN_DIST &&
                   roll < PIRATE_DRONE_CHANCE + PIRATE_BRUTE_CHANCE +
                          PIRATE_BATTLESHIP_CHANCE + PIRATE_MOTHERSHIP_CHANCE) {
          type = PIRATE_MOTHERSHIP;
        }
        pirate_spawn(world, pos, type);
      }
    }
  }
}

void chunks_restore_cell(Chunks *ck, int cx, int cy) {
  chunks_mark(ck, cx, cy);
}

void chunks_update(Chunks *ck, World *world, Vec2f camera) {
  // Don't generate into a nearly-full entity pool; retry once it drains
  int alive = (int)world->high_water - world->free_count;
  if (MAX_ENTITIES - alive < CHUNK_ENTITY_HEADROOM) return;

  int ccx = (int)floorf(camera.x / CHUNK_SIZE);
  int ccy = (int)floorf(camera.y / CHUNK_SIZE);

  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (chunks_mark(ck, ccx + dx, ccy + dy)) {
        chunk_populate(ck, world, ccx + dx, ccy + dy, camera);
      }
    }
  }
}
