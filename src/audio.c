// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <SDL3/SDL.h>
#include "audio.h"

#define SAMPLE_RATE 44100
#define VOICE_COUNT 8
#define THRUST_CHUNK 1024  // rumble synthesis block (~23 ms)

static bool enabled = false;
static SDL_AudioDeviceID device;
static SDL_AudioStream *voices[VOICE_COUNT];
static SDL_AudioStream *thrust_stream;
static int steal_next = 0;

static float *clips[SFX_COUNT];
static int clip_samples[SFX_COUNT];
static bool thrust_on = false;
static float thrust_env = 0.0f;  // smoothed on/off level; kills the clicks
static float thrust_pitch = 1.0f;
static float thrust_phase_a = 0.0f;
static float thrust_phase_b = 0.0f;

static float square(float phase) {
  return fmodf(phase, 1.0f) < 0.5f ? 1.0f : -1.0f;
}

static float triangle(float phase) {
  return 4.0f * fabsf(fmodf(phase, 1.0f) - 0.5f) - 1.0f;
}

static float noise(void) { return SDL_randf() * 2.0f - 1.0f; }

static float *alloc_clip(SfxId id, float seconds) {
  clip_samples[id] = (int)(seconds * SAMPLE_RATE);
  clips[id] = SDL_calloc((size_t)clip_samples[id], sizeof(float));
  return clips[id];
}

/** Square wave sliding f0 -> f1 with exponential decay. */
static void gen_sweep(SfxId id, float seconds, float f0, float f1,
                      float amp, float decay) {
  float *buf = alloc_clip(id, seconds);
  int n = clip_samples[id];
  float phase = 0.0f;

  for (int i = 0; i < n; i++) {
    float t = (float)i / SAMPLE_RATE;
    float f = f0 + (f1 - f0) * ((float)i / (float)n);
    phase += f / SAMPLE_RATE;
    buf[i] = square(phase) * amp * expf(-decay * t);
  }
}

/** Lowpassed noise burst with exponential decay; tone adds a body hum. */
static void gen_burst(SfxId id, float seconds, float amp, float decay,
                      float lowpass, float tone_freq, float tone_amp) {
  float *buf = alloc_clip(id, seconds);
  int n = clip_samples[id];
  float lp = 0.0f;
  float phase = 0.0f;

  for (int i = 0; i < n; i++) {
    float t = (float)i / SAMPLE_RATE;
    lp += lowpass * (noise() - lp);
    phase += tone_freq / SAMPLE_RATE;
    buf[i] = (lp * amp + square(phase) * tone_amp) * expf(-decay * t);
  }
}

/** Sequence of square-wave notes (arpeggio-style chimes). */
static void gen_notes(SfxId id, const float *freqs, int count,
                      float note_seconds, float amp) {
  float *buf = alloc_clip(id, note_seconds * (float)count);
  int per = (int)(note_seconds * SAMPLE_RATE);
  float phase = 0.0f;

  for (int k = 0; k < count; k++) {
    for (int i = 0; i < per; i++) {
      float t = (float)i / SAMPLE_RATE;
      phase += freqs[k] / SAMPLE_RATE;
      buf[k * per + i] = square(phase) * amp * expf(-14.0f * t);
    }
  }
}

static void gen_all_clips(void) {
  gen_sweep(SFX_SHOOT, 0.09f, 880.0f, 440.0f, 0.25f, 22.0f);
  gen_sweep(SFX_SHOOT_PIRATE, 0.12f, 500.0f, 260.0f, 0.22f, 16.0f);
  gen_burst(SFX_CHIP, 0.06f, 0.35f, 45.0f, 0.5f, 0.0f, 0.0f);
  gen_burst(SFX_BREAK, 0.2f, 0.5f, 18.0f, 0.25f, 110.0f, 0.1f);
  gen_burst(SFX_EXPLOSION, 0.55f, 0.8f, 7.0f, 0.12f, 55.0f, 0.12f);
  gen_burst(SFX_HURT, 0.22f, 0.4f, 14.0f, 0.4f, 110.0f, 0.2f);

  const float pickup[] = { 660.0f, 990.0f };
  gen_notes(SFX_PICKUP, pickup, 2, 0.06f, 0.2f);

  const float coin[] = { 1320.0f };
  gen_notes(SFX_COIN, coin, 1, 0.05f, 0.12f);

  const float upgrade[] = { 523.0f, 659.0f, 784.0f, 1047.0f };
  gen_notes(SFX_UPGRADE, upgrade, 4, 0.07f, 0.22f);

  // Station services: held keys tick these several times a second, so
  // they read as a pouring stream and a socket-wrench ratchet
  gen_sweep(SFX_REFUEL, 0.07f, 180.0f, 330.0f, 0.13f, 18.0f);
  gen_burst(SFX_REPAIR, 0.05f, 0.3f, 40.0f, 0.6f, 620.0f, 0.1f);

  const float mayday[] = { 640.0f, 460.0f, 640.0f, 460.0f };
  gen_notes(SFX_MAYDAY, mayday, 4, 0.16f, 0.18f);

  const float fuel_low[] = { 294.0f };
  gen_notes(SFX_FUEL_LOW, fuel_low, 1, 0.1f, 0.13f);

  const float hull_low[] = { 880.0f, 587.0f };
  gen_notes(SFX_HULL_LOW, hull_low, 2, 0.12f, 0.15f);

  const float warhorn[] = { 196.0f, 147.0f };
  gen_notes(SFX_WARHORN, warhorn, 2, 0.22f, 0.2f);
}

bool audio_init(void) {
  SDL_AudioSpec spec = {
    .format = SDL_AUDIO_F32,
    .channels = 1,
    .freq = SAMPLE_RATE,
  };

  device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
  if (!device) return false;

  for (int i = 0; i < VOICE_COUNT; i++) {
    voices[i] = SDL_CreateAudioStream(&spec, &spec);
    if (!voices[i] || !SDL_BindAudioStream(device, voices[i])) return false;
  }

  thrust_stream = SDL_CreateAudioStream(&spec, &spec);
  if (!thrust_stream || !SDL_BindAudioStream(device, thrust_stream)) return false;

  SDL_ResumeAudioDevice(device);
  gen_all_clips();

  enabled = true;
  return true;
}

SDL_AudioDeviceID audio_device(void) {
  return enabled ? device : 0;
}

static float sfx_volume = 1.0f;
static bool sfx_muted = false;

static void apply_sfx_gain(void) {
  if (!enabled) return;
  float g = sfx_muted ? 0.0f : sfx_volume;

  for (int i = 0; i < VOICE_COUNT; i++) SDL_SetAudioStreamGain(voices[i], g);
  SDL_SetAudioStreamGain(thrust_stream, g);
}

void audio_set_sfx_volume(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  sfx_volume = v;
  apply_sfx_gain();
}

float audio_get_sfx_volume(void) { return sfx_volume; }

void audio_set_muted(bool m) {
  sfx_muted = m;
  apply_sfx_gain();
}

void audio_play(SfxId id) {
  if (!enabled) return;

  int bytes = clip_samples[id] * (int)sizeof(float);

  for (int i = 0; i < VOICE_COUNT; i++) {
    if (SDL_GetAudioStreamQueued(voices[i]) == 0) {
      SDL_PutAudioStreamData(voices[i], clips[id], bytes);
      return;
    }
  }

  // All voices busy: steal one round-robin
  SDL_ClearAudioStream(voices[steal_next]);
  SDL_PutAudioStreamData(voices[steal_next], clips[id], bytes);
  steal_next = (steal_next + 1) % VOICE_COUNT;
}

void audio_thrust(bool on, float pitch) {
  thrust_on = on;
  if (pitch < 0.5f) pitch = 0.5f;
  if (pitch > 2.0f) pitch = 2.0f;
  thrust_pitch = pitch;
}

void audio_update(void) {
  if (!enabled) return;
  if (!thrust_on && thrust_env <= 0.0005f) return;

  // Rumble is synthesized live: 50 + 70 Hz triangles under an envelope
  // that eases in (~50 ms) and out (~110 ms) — phase runs continuously,
  // so keying the thrust never lands on a waveform discontinuity.
  static float buf[THRUST_CHUNK];
  int chunk_bytes = THRUST_CHUNK * (int)sizeof(float);
  float target = thrust_on ? 1.0f : 0.0f;
  float k = thrust_on ? 1.0f / (0.05f * SAMPLE_RATE)
                      : 1.0f / (0.11f * SAMPLE_RATE);

  while (SDL_GetAudioStreamQueued(thrust_stream) < 2 * chunk_bytes) {
    for (int i = 0; i < THRUST_CHUNK; i++) {
      thrust_env += (target - thrust_env) * k;
      thrust_phase_a += 50.0f * thrust_pitch / SAMPLE_RATE;
      thrust_phase_b += 70.0f * thrust_pitch / SAMPLE_RATE;
      if (thrust_phase_a >= 1.0f) thrust_phase_a -= 1.0f;
      if (thrust_phase_b >= 1.0f) thrust_phase_b -= 1.0f;

      buf[i] = (triangle(thrust_phase_a) * 0.045f +
                triangle(thrust_phase_b) * 0.025f) * thrust_env;
    }
    SDL_PutAudioStreamData(thrust_stream, buf, chunk_bytes);

    if (!thrust_on && thrust_env <= 0.0005f) break;  // tail finished
  }
}

void audio_quit(void) {
  if (!enabled) return;

  for (int i = 0; i < VOICE_COUNT; i++) SDL_DestroyAudioStream(voices[i]);
  SDL_DestroyAudioStream(thrust_stream);
  SDL_CloseAudioDevice(device);

  for (int i = 0; i < SFX_COUNT; i++) SDL_free(clips[i]);
  enabled = false;
}
