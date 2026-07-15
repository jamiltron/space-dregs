// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Deterministic input recording and playback.
 *
 *  A recording starts a fresh seeded run and captures, per fixed sim
 *  step, the held-action bitmask plus any discrete actions (upgrade
 *  buys, quest choices). Playback resets to the same seed and feeds
 *  the recorded input back through sim_input(), reproducing the run.
 *  Dev feature: F9 record/stop, F10 replay/stop; file lives beside
 *  the save. Same-machine determinism only (floats).
 */

#ifndef _SD_REPLAY_H
#define _SD_REPLAY_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "input.h"

typedef enum ReplayMode {
  REPLAY_OFF,
  REPLAY_RECORD,
  REPLAY_PLAYBACK,
} ReplayMode;

/** Discrete recorded actions (arg = upgrade index / offer index). */
enum {
  RA_BUY_UPGRADE,
  RA_QUEST_ACCEPT,
  RA_QUEST_ABANDON,
  RA_BUY_SHIELD,
  RA_BUY_MINE,     /**< Buys the rack first, then restocks one mine. */
  RA_BUY_MISSILE,  /**< Buys the pod first, then restocks one missile. */
};

ReplayMode replay_mode(void);

/** Start recording a fresh run. @return the seed to game_reset with. */
Uint64 replay_begin_record(void);

/** Load the replay file for playback. @return false if none/invalid;
 *  on success the caller must game_reset with *seed_out. */
bool replay_begin_playback(Uint64 *seed_out);

/** Stop either mode; a recording is written to disk. */
void replay_stop(void);

/** Call at the top of every sim step: captures or injects the held
 *  mask for this step. Playback auto-stops when the tape ends. */
void replay_step(void);

/** Call at the end of every sim step. */
void replay_step_done(void);

/** Held input as the simulation must see it (live, or from tape). */
bool sim_input(InputAction a);

/** Record a discrete action performed live this frame. */
void replay_record_action(int code, int arg);

/** Playback: pop the next discrete action due this step.
 *  @return false when none remain for the current step. */
bool replay_next_action(int *code, int *arg);

#endif
