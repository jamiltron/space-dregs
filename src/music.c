// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <SDL3/SDL.h>
#include "music.h"
#include "audio.h"
#include "math2d.h"

#define MUSIC_RATE 44100
#define MUSIC_CHUNK 2048          // samples generated per fill (~46 ms)
#define MUSIC_BUFFER_CHUNKS 4     // keep this much queued ahead
#define MUSIC_PADS 5
#define MUSIC_GAIN 0.5f           // overall level; keep it under the SFX

#define DELAY_SAMPLES 19845       // 0.45 s — the "space"
#define DELAY_FEEDBACK 0.45f
#define DELAY_MIX 0.4f

/** Chord palette: semitone offsets from A2 (110 Hz), one note per pad.
 *  All modal-ambient neighbors of A minor, so any order sounds fine. */
static const int CHORDS[][MUSIC_PADS] = {
  {  0,  7, 12, 14, 19 },  // Am add9
  { -4,  3,  8, 15, 19 },  // Fmaj7
  { -9, -2,  3, 10, 14 },  // Cmaj7
  { -2,  5, 10, 14, 17 },  // G6
  { -7,  0,  5, 12, 17 },  // Dm7
  { -5,  2,  7, 11, 14 },  // Em7
};
#define CHORD_COUNT (int)(sizeof(CHORDS) / sizeof(CHORDS[0]))

/** Pentatonic offsets for the lead, relative to the current chord root. */
static const int LEAD_SCALE[] = { 0, 3, 5, 7, 10, 12, 15, 19, 24 };
#define LEAD_SCALE_COUNT (int)(sizeof(LEAD_SCALE) / sizeof(LEAD_SCALE[0]))

typedef struct Pad {
  float freq, target_freq;    // portamento glides freq toward target
  float level, target_level;
  float phase_a, phase_b;     // detuned oscillator pair
  float lfo_phase, lfo_rate;  // slow per-pad shimmer
} Pad;

static bool enabled = false;
static SDL_AudioStream *stream;
static Pad pads[MUSIC_PADS];
static float chunk_buf[MUSIC_CHUNK];
static float delay_buf[DELAY_SAMPLES];
static int delay_idx;
static float lp_state, lp_lfo_phase;
static float chord_timer, lead_timer;
static int chord_root;
static bool lead_on;
static float lead_freq, lead_phase, lead_age;

static float triangle(float phase) {
  return 4.0f * fabsf(fmodf(phase, 1.0f) - 0.5f) - 1.0f;
}

static float note_freq(int semis) {
  return 110.0f * powf(2.0f, (float)semis / 12.0f);
}

static void pick_chord(bool first) {
  static int last = -1;
  int c;
  do { c = SDL_rand(CHORD_COUNT); } while (c == last);
  last = c;
  chord_root = CHORDS[c][0];

  for (int v = 0; v < MUSIC_PADS; v++) {
    pads[v].target_freq = note_freq(CHORDS[c][v]);
    pads[v].target_level = 0.028f + SDL_randf() * 0.012f;
    if (first) pads[v].freq = pads[v].target_freq;  // no sweep-in from zero
  }

  // Sometimes rest one voice — thins the texture, keeps it breathing
  if (SDL_randf() < 0.35f) pads[SDL_rand(MUSIC_PADS)].target_level = 0.0f;

  chord_timer = 8.0f + SDL_randf() * 5.0f;
}

/** Synthesize one buffer of the endless arrangement. */
static void gen_chunk(void) {
  const float dt = 1.0f / MUSIC_RATE;

  for (int i = 0; i < MUSIC_CHUNK; i++) {
    chord_timer -= dt;
    if (chord_timer <= 0.0f) pick_chord(false);

    lead_timer -= dt;
    if (lead_timer <= 0.0f && !lead_on) {
      lead_freq = note_freq(chord_root + 12 +
                            LEAD_SCALE[SDL_rand(LEAD_SCALE_COUNT)]);
      lead_age = 0.0f;
      lead_on = true;
      lead_timer = 5.0f + SDL_randf() * 7.0f;
    }

    float dry = 0.0f;
    for (int v = 0; v < MUSIC_PADS; v++) {
      Pad *p = &pads[v];
      p->freq += (p->target_freq - p->freq) * (0.8f * dt);
      p->level += (p->target_level - p->level) * (1.2f * dt);
      p->phase_a += p->freq * 0.998f * dt;
      p->phase_b += p->freq * 1.002f * dt;
      p->lfo_phase += p->lfo_rate * dt;
      if (p->phase_a >= 1.0f) p->phase_a -= 1.0f;
      if (p->phase_b >= 1.0f) p->phase_b -= 1.0f;
      if (p->lfo_phase >= TWO_PI) p->lfo_phase -= TWO_PI;

      float shimmer = 0.85f + 0.15f * sinf(p->lfo_phase);
      dry += (triangle(p->phase_a) + triangle(p->phase_b)) * 0.5f *
             p->level * shimmer;
    }

    if (lead_on) {
      lead_age += dt;
      float env = lead_age < 0.9f ? lead_age / 0.9f
                                  : expf(-(lead_age - 0.9f) / 2.2f);
      lead_phase += lead_freq * dt;
      if (lead_phase >= 1.0f) lead_phase -= 1.0f;
      dry += triangle(lead_phase) * 0.045f * env;
      if (lead_age > 1.0f && env < 0.004f) lead_on = false;
    }

    lp_lfo_phase += 0.05f * TWO_PI * dt;
    if (lp_lfo_phase >= TWO_PI) lp_lfo_phase -= TWO_PI;
    float cut = 0.06f + 0.04f * (0.5f + 0.5f * sinf(lp_lfo_phase));
    lp_state += cut * (dry - lp_state);

    float echo = delay_buf[delay_idx];
    delay_buf[delay_idx] = lp_state + echo * DELAY_FEEDBACK;
    delay_idx = (delay_idx + 1) % DELAY_SAMPLES;

    chunk_buf[i] = (lp_state + echo * DELAY_MIX) * MUSIC_GAIN;
  }
}

bool music_init(void) {
  SDL_AudioDeviceID device = audio_device();
  if (!device) return false;

  SDL_AudioSpec spec = {
    .format = SDL_AUDIO_F32,
    .channels = 1,
    .freq = MUSIC_RATE,
  };
  stream = SDL_CreateAudioStream(&spec, &spec);
  if (!stream || !SDL_BindAudioStream(device, stream)) return false;

  for (int v = 0; v < MUSIC_PADS; v++) {
    pads[v].lfo_phase = SDL_randf() * TWO_PI;
    pads[v].lfo_rate = (0.07f + SDL_randf() * 0.12f) * TWO_PI;
  }
  pick_chord(true);
  lead_timer = 3.0f + SDL_randf() * 4.0f;

  enabled = true;
  return true;
}

static float music_volume = 1.0f;
static bool music_muted = false;

static void apply_music_gain(void) {
  if (enabled) {
    SDL_SetAudioStreamGain(stream, music_muted ? 0.0f : music_volume);
  }
}

void music_set_volume(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  music_volume = v;
  apply_music_gain();
}

float music_get_volume(void) { return music_volume; }

void music_set_muted(bool m) {
  music_muted = m;
  apply_music_gain();
}

void music_update(void) {
  if (!enabled) return;

  int chunk_bytes = MUSIC_CHUNK * (int)sizeof(float);
  while (SDL_GetAudioStreamQueued(stream) < MUSIC_BUFFER_CHUNKS * chunk_bytes) {
    gen_chunk();
    SDL_PutAudioStreamData(stream, chunk_buf, chunk_bytes);
  }
}

void music_quit(void) {
  if (!enabled) return;
  SDL_DestroyAudioStream(stream);
  enabled = false;
}
