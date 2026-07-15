// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "replay.h"
#include "config.h"

#define REPLAY_MAX_STEPS (1 << 17)  // ~18 minutes at 120 Hz
#define REPLAY_MAX_ACTIONS 1024
#define REPLAY_MAGIC 0x53445250u    // "SDRP"

typedef struct ReplayEvent {
  Uint32 step;
  Uint8 code;
  Uint8 arg;
} ReplayEvent;

static ReplayMode mode = REPLAY_OFF;
static Uint64 seed;
static Uint8 held[REPLAY_MAX_STEPS];
static ReplayEvent actions[REPLAY_MAX_ACTIONS];
static Uint32 step_count, action_count;
static Uint32 cur_step, action_cursor;
static Uint8 cur_mask;

static char path[512];

static void ensure_path(void) {
  if (path[0]) return;
#ifdef __EMSCRIPTEN__
  SDL_strlcpy(path, "/prefs/replay.dat", sizeof(path));
#else
  char *pref = SDL_GetPrefPath("justinhamilton", "SpaceDregs");
  if (pref) {
    SDL_snprintf(path, sizeof(path), "%sreplay.dat", pref);
    SDL_free(pref);
  }
#endif
}

static Uint8 live_mask(void) {
  Uint8 mask = 0;
  for (int a = 0; a < ACT_COUNT; a++) {
    if (input_down((InputAction)a)) mask |= (Uint8)(1 << a);
  }
  return mask;
}

ReplayMode replay_mode(void) { return mode; }

Uint64 replay_begin_record(void) {
  seed = ((Uint64)SDL_rand_bits() << 32) | SDL_rand_bits();
  step_count = action_count = cur_step = action_cursor = 0;
  mode = REPLAY_RECORD;
  SDL_Log("Replay: recording (seed %llx)", (unsigned long long)seed);
  return seed;
}

bool replay_begin_playback(Uint64 *seed_out) {
  ensure_path();

  SDL_IOStream *io = SDL_IOFromFile(path, "r");
  if (!io) return false;

  Uint32 magic = 0;
  bool ok = SDL_ReadIO(io, &magic, sizeof(magic)) == sizeof(magic) &&
            magic == REPLAY_MAGIC &&
            SDL_ReadIO(io, &seed, sizeof(seed)) == sizeof(seed) &&
            SDL_ReadIO(io, &step_count, sizeof(step_count)) == sizeof(step_count) &&
            SDL_ReadIO(io, &action_count, sizeof(action_count)) == sizeof(action_count) &&
            step_count <= REPLAY_MAX_STEPS && action_count <= REPLAY_MAX_ACTIONS &&
            SDL_ReadIO(io, held, step_count) == step_count &&
            SDL_ReadIO(io, actions, action_count * sizeof(ReplayEvent)) ==
                action_count * sizeof(ReplayEvent);
  SDL_CloseIO(io);

  if (!ok) {
    SDL_Log("Replay: no valid recording to play");
    return false;
  }

  cur_step = 0;
  action_cursor = 0;
  mode = REPLAY_PLAYBACK;
  *seed_out = seed;
  SDL_Log("Replay: playing %u steps", step_count);
  return true;
}

static void save_recording(void) {
  ensure_path();

  SDL_IOStream *io = SDL_IOFromFile(path, "w");
  if (!io) return;

  Uint32 magic = REPLAY_MAGIC;
  SDL_WriteIO(io, &magic, sizeof(magic));
  SDL_WriteIO(io, &seed, sizeof(seed));
  SDL_WriteIO(io, &step_count, sizeof(step_count));
  SDL_WriteIO(io, &action_count, sizeof(action_count));
  SDL_WriteIO(io, held, step_count);
  SDL_WriteIO(io, actions, action_count * sizeof(ReplayEvent));
  SDL_CloseIO(io);
  config_flush_storage();

  SDL_Log("Replay: saved %u steps", step_count);
}

void replay_stop(void) {
  if (mode == REPLAY_RECORD) save_recording();
  else if (mode == REPLAY_PLAYBACK) SDL_Log("Replay: stopped");
  mode = REPLAY_OFF;
}

void replay_step(void) {
  switch (mode) {
  case REPLAY_RECORD:
    if (cur_step >= REPLAY_MAX_STEPS) {
      replay_stop();  // tape full
      break;
    }
    cur_mask = live_mask();
    held[cur_step] = cur_mask;
    step_count = cur_step + 1;
    break;
  case REPLAY_PLAYBACK:
    if (cur_step >= step_count) {
      replay_stop();  // tape finished; control returns to the player
      break;
    }
    cur_mask = held[cur_step];
    break;
  default:
    break;
  }
}

void replay_step_done(void) {
  if (mode != REPLAY_OFF) cur_step++;
}

bool sim_input(InputAction a) {
  if (mode == REPLAY_OFF) return input_down(a);
  return (cur_mask >> a) & 1;
}

void replay_record_action(int code, int arg) {
  if (mode != REPLAY_RECORD || action_count >= REPLAY_MAX_ACTIONS) return;
  actions[action_count++] = (ReplayEvent){
    .step = cur_step,
    .code = (Uint8)code,
    .arg  = (Uint8)arg,
  };
}

bool replay_next_action(int *code, int *arg) {
  if (mode != REPLAY_PLAYBACK) return false;
  if (action_cursor >= action_count) return false;
  if (actions[action_cursor].step != cur_step) return false;

  *code = actions[action_cursor].code;
  *arg = actions[action_cursor].arg;
  action_cursor++;
  return true;
}
