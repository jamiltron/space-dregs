// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "font.h"
#include "draw.h"

/** Segment layout on a 2-wide x 4-tall cell (y down):
 *  @verbatim
 *   0,0  A1  1,0  A2  2,0
 *    F   H   I   J   B
 *   0,2  G1  1,2  G2  2,2
 *    E   K   L   M   C
 *   0,4  D1* 1,4  D2* 2,4      (*D1 right half, D2 left half)
 *  @endverbatim */
#define A1 (1 << 0)
#define A2 (1 << 1)
#define B  (1 << 2)
#define C  (1 << 3)
#define D1 (1 << 4)
#define D2 (1 << 5)
#define E  (1 << 6)
#define F  (1 << 7)
#define G1 (1 << 8)
#define G2 (1 << 9)
#define H  (1 << 10)
#define I  (1 << 11)
#define J  (1 << 12)
#define K  (1 << 13)
#define L  (1 << 14)
#define M  (1 << 15)
// Extras beyond the classic 16: full diagonals to bottom-center (a real
// symmetric V), and small marks for punctuation.
#define VL (1 << 16)
#define VR (1 << 17)
#define PDOT (1 << 18)   // period: baseline tick
#define CTAIL (1 << 19)  // comma: descender tail
#define CUP (1 << 20)    // colon: upper tick
#define CLO (1 << 21)    // colon: lower tick
#define APOS (1 << 22)   // apostrophe: top tick

#define SEGMENT_COUNT 23

static const float SEGMENTS[SEGMENT_COUNT][4] = {
  { 0, 0, 1, 0 },  // A1
  { 1, 0, 2, 0 },  // A2
  { 2, 0, 2, 2 },  // B
  { 2, 2, 2, 4 },  // C
  { 1, 4, 2, 4 },  // D1
  { 0, 4, 1, 4 },  // D2
  { 0, 2, 0, 4 },  // E
  { 0, 0, 0, 2 },  // F
  { 0, 2, 1, 2 },  // G1
  { 1, 2, 2, 2 },  // G2
  { 0, 0, 1, 2 },  // H
  { 1, 0, 1, 2 },  // I
  { 2, 0, 1, 2 },  // J
  { 0, 4, 1, 2 },  // K
  { 1, 2, 1, 4 },  // L
  { 2, 4, 1, 2 },  // M
  { 0, 0, 1, 4 },  // VL: top-left to bottom-center
  { 2, 0, 1, 4 },  // VR: top-right to bottom-center
  { 1, 3.5f, 1, 4 },        // PDOT
  { 1.15f, 3.5f, 0.75f, 4.7f },  // CTAIL
  { 1, 0.9f, 1, 1.5f },     // CUP
  { 1, 2.5f, 1, 3.1f },     // CLO
  { 1.15f, 0, 0.85f, 0.9f },     // APOS
};

static Uint32 glyph_mask(char c) {
  switch (c) {
  case '0': return A1 | A2 | B | C | D1 | D2 | E | F;
  case '1': return B | C;
  case '2': return A1 | A2 | B | G1 | G2 | E | D1 | D2;
  case '3': return A1 | A2 | B | C | D1 | D2 | G2;
  case '4': return F | G1 | G2 | B | C;
  case '5': return A1 | A2 | F | G1 | G2 | C | D1 | D2;
  case '6': return A1 | A2 | F | E | D1 | D2 | C | G1 | G2;
  case '7': return A1 | A2 | B | C;
  case '8': return A1 | A2 | B | C | D1 | D2 | E | F | G1 | G2;
  case '9': return A1 | A2 | B | C | D1 | D2 | F | G1 | G2;
  case 'A': return A1 | A2 | B | C | E | F | G1 | G2;
  case 'B': return A1 | A2 | B | C | D1 | D2 | G2 | I | L;
  case 'C': return A1 | A2 | F | E | D1 | D2;
  case 'D': return A1 | A2 | B | C | D1 | D2 | I | L;
  case 'E': return A1 | A2 | F | E | D1 | D2 | G1;
  case 'F': return A1 | A2 | F | E | G1;
  case 'G': return A1 | A2 | F | E | D1 | D2 | C | G2;
  case 'H': return F | E | B | C | G1 | G2;
  case 'I': return A1 | A2 | D1 | D2 | I | L;
  case 'J': return B | C | D1 | D2 | E;
  case 'K': return F | E | G1 | J | M;
  case 'L': return F | E | D1 | D2;
  case 'M': return F | E | B | C | H | J;
  case 'N': return F | E | B | C | H | M;
  case 'O': return A1 | A2 | B | C | D1 | D2 | E | F;
  case 'P': return A1 | A2 | B | F | E | G1 | G2;
  case 'Q': return A1 | A2 | B | C | D1 | D2 | E | F | M;
  case 'R': return A1 | A2 | B | F | E | G1 | G2 | M;
  case 'S': return A1 | A2 | F | G1 | G2 | C | D1 | D2;
  case 'T': return A1 | A2 | I | L;
  case 'U': return F | E | D1 | D2 | C | B;
  case 'V': return VL | VR;
  case 'W': return F | E | B | C | K | M;
  case 'X': return H | J | K | M;
  case 'Y': return H | J | L;
  case 'Z': return A1 | A2 | J | K | D1 | D2;
  case '-': return G1 | G2;
  case '+': return G1 | G2 | I | L;
  case '/': return J | K;  // TR to center to BL
  case '.': return PDOT;
  case ',': return CTAIL;
  case ':': return CUP | CLO;
  case '\'': return APOS;
  default:  return 0;  // space and anything unknown
  }
}

void font_draw_text(SDL_Renderer *renderer, const char *text,
                    float x, float y, float size, SDL_Color color) {
  float scale = size / 4.0f;

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

  for (const char *c = text; *c; c++) {
    char up = (*c >= 'a' && *c <= 'z') ? (char)(*c - 'a' + 'A') : *c;
    Uint32 mask = glyph_mask(up);

    for (int s = 0; s < SEGMENT_COUNT; s++) {
      if (!(mask & (1u << s))) continue;
      draw_line(renderer,
                x + SEGMENTS[s][0] * scale, y + SEGMENTS[s][1] * scale,
                x + SEGMENTS[s][2] * scale, y + SEGMENTS[s][3] * scale);
    }

    x += 3.0f * scale;  // 2 wide + 1 gap
  }
}

float font_text_width(const char *text, float size) {
  int n = 0;
  while (text[n]) n++;
  if (n == 0) return 0.0f;

  return (float)(n * 3 - 1) * (size / 4.0f);
}
