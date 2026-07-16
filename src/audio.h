// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Sound effects: all synthesized at init (squares, noise, sweeps) —
 *  no asset files. One-shots play through a small stream pool; the
 *  thruster is a gated live-synthesized loop. Everything no-ops if the
 *  audio device fails, so the game runs fine silent.
 */

#ifndef _SD_AUDIO_H
#define _SD_AUDIO_H
#include <stdbool.h>
#include <SDL3/SDL.h>

typedef enum SfxId {
  SFX_SHOOT,         /**< Player pew. */
  SFX_SHOOT_PIRATE,  /**< Lower, lazier pew. */
  SFX_CHIP,          /**< Bullet lands, target survives. */
  SFX_BREAK,         /**< Asteroid cracks apart. */
  SFX_EXPLOSION,     /**< Final burst / ship / pirate death. */
  SFX_PICKUP,        /**< Scrap or quest item collected. */
  SFX_COIN,          /**< Scrap sold at the station. */
  SFX_HURT,          /**< Player takes damage. */
  SFX_UPGRADE,       /**< Purchase or quest complete. */
  SFX_REFUEL,        /**< One credit of fuel: low liquid glug. */
  SFX_REPAIR,        /**< One credit of hull: ratchet clank. */
  SFX_MAYDAY,        /**< Two-tone distress siren. */
  SFX_FUEL_LOW,      /**< Soft single chip: fuel in the red. */
  SFX_HULL_LOW,      /**< Descending two-tone: hull in the red. */
  SFX_WARHORN,       /**< Low two-note horn: hunters inbound. */
  SFX_COUNT
} SfxId;

/** Open the device, build all clips. @return false to run silent. */
bool audio_init(void);

/** The opened device, or 0 when audio is unavailable. */
SDL_AudioDeviceID audio_device(void);

/** Master SFX gain, 0..1 (applied to all voices and the thruster). */
void audio_set_sfx_volume(float v);
float audio_get_sfx_volume(void);

/** Mute without touching the saved volume (session-only). */
void audio_set_muted(bool m);

/** Play a one-shot on a free voice. */
void audio_play(SfxId id);

/** Gate the engine rumble; pitch ~1.0 at rest, rises with speed. */
void audio_thrust(bool on, float pitch);

/** Keep the rumble stream fed; call once per frame. */
void audio_update(void);

void audio_quit(void);

#endif
