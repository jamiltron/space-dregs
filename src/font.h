// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Vector line font: segment glyphs (A-Z, 0-9, dash, slash, period,
 *  comma, colon, apostrophe, space) drawn as lines to match the
 *  wireframe look.
 */

#ifndef _SD_FONT_H
#define _SD_FONT_H
#include <SDL3/SDL.h>

/** Draw text at (x, y). size is the glyph height in pixels; width is
 *  half that. Lowercase maps to uppercase; unknown glyphs are blank. */
void font_draw_text(SDL_Renderer *renderer, const char *text,
                    float x, float y, float size, SDL_Color color);

/** Rendered width of text at the given size (fixed-width glyphs). */
float font_text_width(const char *text, float size);

#endif
