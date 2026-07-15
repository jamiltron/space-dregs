// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Endless generative ambient music.
 *
 *  Detuned triangle pads drift between chords from a small modal
 *  palette with slow portamento, a sparse pentatonic lead floats on
 *  top, and a feedback delay supplies the space. Samples are
 *  synthesized continuously into a stream — nothing is prerecorded
 *  and it never loops. Requires audio_init() to have succeeded.
 */

#ifndef _SD_MUSIC_H
#define _SD_MUSIC_H
#include <stdbool.h>

/** Bind the music stream and pick the first chord. */
bool music_init(void);

/** Keep the stream fed; call once per frame. */
void music_update(void);

/** Music gain, 0..1. */
void music_set_volume(float v);
float music_get_volume(void);

/** Mute without touching the saved volume (session-only). */
void music_set_muted(bool m);

void music_quit(void);

#endif
