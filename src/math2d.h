// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  2D float vector math and angle helpers.
 *
 *  Angle convention everywhere: degrees on entities, 0 = up,
 *  clockwise positive (screen Y increases downward).
 */

#ifndef _SD_MATH2D_H
#define _SD_MATH2D_H
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0f * (float)M_PI)
#define DEG_TO_RAD(x) ((x) * (float)(M_PI / 180.0))
#define RAD_TO_DEG(x) ((x) * (float)(180.0 / M_PI))

typedef struct Vec2f {
  float x, y;
} Vec2f;

static inline Vec2f vec2f_new(float x, float y) { return (Vec2f){.x = x, .y = y}; }

static inline Vec2f vec2f_add(Vec2f a, Vec2f b) {
  return vec2f_new(a.x + b.x, a.y + b.y);
}

static inline Vec2f vec2f_sub(Vec2f a, Vec2f b) {
  return vec2f_new(a.x - b.x, a.y - b.y);
}

static inline float vec2f_dot(Vec2f a, Vec2f b) {
  return a.x * b.x + a.y * b.y;
}

static inline Vec2f vec2f_mul(Vec2f v, float s) { return vec2f_new(v.x * s, v.y * s); }

/** Rotate v by radians (clockwise on screen). */
static inline Vec2f vec2f_rotate(Vec2f v, float radians) {
  float c = cosf(radians);
  float s = sinf(radians);

  return vec2f_new(v.x * c - v.y * s, v.x * s + v.y * c);
}

static inline float vec2f_length(Vec2f v) {
  return sqrtf(v.x * v.x + v.y * v.y);
}

/** Unit vector for a heading in radians. */
static inline Vec2f vec2f_dir(float radians) {
  return vec2f_new(sinf(radians), -cosf(radians));
}

/** Heading of a vector in degrees, matching Transform.angle. */
static inline float vec2f_heading(Vec2f v) {
  return RAD_TO_DEG(atan2f(v.x, -v.y));
}

#endif
