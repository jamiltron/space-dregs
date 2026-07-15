// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menu.h"
#include "app.h"
#include "audio.h"
#include "config.h"
#include "font.h"
#include "input.h"
#include "music.h"
#include "save.h"
#include "version.h"

typedef enum MenuScreen {
  SCREEN_MAIN,
  SCREEN_OPTIONS,
} MenuScreen;

// Options rows: one per action, then volumes, fullscreen, back
#define ROW_SFX (ACT_COUNT)
#define ROW_MUSIC (ACT_COUNT + 1)
#define ROW_FULLSCREEN (ACT_COUNT + 2)
#define ROW_BACK (ACT_COUNT + 3)
#define OPTION_ROWS (ACT_COUNT + 4)

// Main items are dynamic: CONTINUE appears only with a saved run
// (which can pop in late on web, after the async IDBFS load)
#define MAIN_MAX_ITEMS 4

static int main_items(const char *items[MAIN_MAX_ITEMS],
                      MenuResult results[MAIN_MAX_ITEMS]) {
  int n = 0;
  if (save_exists()) {
    items[n] = "CONTINUE";
    results[n++] = MENU_CONTINUE;
  }
  items[n] = "NEW GAME";
  results[n++] = MENU_START;
  items[n] = "OPTIONS";
  results[n++] = MENU_NONE;  // handled specially: opens options
#ifndef __EMSCRIPTEN__
  items[n] = "QUIT";        // meaningless in a browser tab
  results[n++] = MENU_QUIT;
#endif
  return n;
}

static SDL_Window *window;
static MenuScreen screen = SCREEN_MAIN;
static int cursor = 0;
static int rebinding = -1;  // action index waiting for a key, or -1

static const SDL_Color COLOR_TITLE    = { 180, 200, 255, 255 };
static const SDL_Color COLOR_SELECTED = { 255, 200, 80, 255 };
static const SDL_Color COLOR_ITEM     = { 110, 130, 170, 255 };
static const SDL_Color COLOR_HINT     = { 90, 100, 125, 255 };

void menu_init(SDL_Window *w) { window = w; }

void menu_open(void) {
  screen = SCREEN_MAIN;
  cursor = 0;
  rebinding = -1;
}

static bool fullscreen_on(void) {
  return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

static void adjust_row(int row, float dir) {
  if (row == ROW_SFX) {
    audio_set_sfx_volume(audio_get_sfx_volume() + dir * 0.1f);
  } else if (row == ROW_MUSIC) {
    music_set_volume(music_get_volume() + dir * 0.1f);
  } else if (row == ROW_FULLSCREEN) {
    SDL_SetWindowFullscreen(window, !fullscreen_on());
  } else {
    return;
  }
  config_save();
}

MenuResult menu_handle_key(SDL_Scancode key) {
  if (rebinding >= 0) {
    if (key != SDL_SCANCODE_ESCAPE) {
      input_set((InputAction)rebinding, key);
      config_save();
    }
    rebinding = -1;
    return MENU_NONE;
  }

  if (screen == SCREEN_MAIN) {
    const char *items[MAIN_MAX_ITEMS];
    MenuResult results[MAIN_MAX_ITEMS];
    int count = main_items(items, results);
    if (cursor >= count) cursor = count - 1;

    switch (key) {
    case SDL_SCANCODE_UP:
      cursor = (cursor + count - 1) % count;
      break;
    case SDL_SCANCODE_DOWN:
      cursor = (cursor + 1) % count;
      break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_SPACE:
      if (SDL_strcmp(items[cursor], "OPTIONS") == 0) {
        screen = SCREEN_OPTIONS;
        cursor = 0;
        return MENU_NONE;
      }
      return results[cursor];
    default:
      break;
    }
    return MENU_NONE;
  }

  switch (key) {
  case SDL_SCANCODE_ESCAPE:
    screen = SCREEN_MAIN;
    cursor = 1;
    break;
  case SDL_SCANCODE_UP:
    cursor = (cursor + OPTION_ROWS - 1) % OPTION_ROWS;
    break;
  case SDL_SCANCODE_DOWN:
    cursor = (cursor + 1) % OPTION_ROWS;
    break;
  case SDL_SCANCODE_LEFT:
    adjust_row(cursor, -1.0f);
    break;
  case SDL_SCANCODE_RIGHT:
    adjust_row(cursor, 1.0f);
    break;
  case SDL_SCANCODE_RETURN:
  case SDL_SCANCODE_SPACE:
    if (cursor < ACT_COUNT) {
      rebinding = cursor;
    } else if (cursor == ROW_FULLSCREEN) {
      adjust_row(cursor, 1.0f);
    } else if (cursor == ROW_BACK) {
      screen = SCREEN_MAIN;
      cursor = 1;
    } else {
      adjust_row(cursor, 1.0f);  // volumes: enter nudges up
    }
    break;
  default:
    break;
  }
  return MENU_NONE;
}

/** Draw text horizontally centered at height y. */
static void draw_centered(SDL_Renderer *r, const char *text, float y,
                          float size, SDL_Color color) {
  font_draw_text(r, text, (WINDOW_WIDTH - font_text_width(text, size)) / 2.0f,
                 y, size, color);
}

static void render_main(SDL_Renderer *renderer) {
  draw_centered(renderer, "SPACE DREGS", WINDOW_HEIGHT * 0.22f, 56.0f,
                COLOR_TITLE);

  const char *items[MAIN_MAX_ITEMS];
  MenuResult results[MAIN_MAX_ITEMS];
  int count = main_items(items, results);
  if (cursor >= count) cursor = count - 1;

  float y = WINDOW_HEIGHT * 0.5f;
  for (int i = 0; i < count; i++) {
    draw_centered(renderer, items[i], y, 20.0f,
                  i == cursor ? COLOR_SELECTED : COLOR_ITEM);
    y += 42.0f;
  }

  draw_centered(renderer, "ARROWS TO MOVE - ENTER TO SELECT",
                WINDOW_HEIGHT - 60.0f, 10.0f, COLOR_HINT);

  font_draw_text(renderer, GAME_VERSION,
                 WINDOW_WIDTH - font_text_width(GAME_VERSION, 10.0f) - 12.0f,
                 WINDOW_HEIGHT - 22.0f, 10.0f, COLOR_HINT);
}

static void render_options(SDL_Renderer *renderer) {
  draw_centered(renderer, "OPTIONS", WINDOW_HEIGHT * 0.12f, 32.0f, COLOR_TITLE);

  float label_x = WINDOW_WIDTH / 2.0f - 260.0f;
  float value_x = WINDOW_WIDTH / 2.0f + 90.0f;
  float y = WINDOW_HEIGHT * 0.24f;
  char buf[32];

  for (int row = 0; row < OPTION_ROWS; row++) {
    SDL_Color color = row == cursor ? COLOR_SELECTED : COLOR_ITEM;
    const char *label = NULL;
    const char *value = NULL;

    if (row < ACT_COUNT) {
      label = input_label((InputAction)row);
      value = rebinding == row ? "PRESS KEY"
                               : SDL_GetScancodeName(input_get((InputAction)row));
    } else if (row == ROW_SFX) {
      label = "SFX VOLUME";
      SDL_snprintf(buf, sizeof(buf), "%d", (int)(audio_get_sfx_volume() * 100.0f + 0.5f));
      value = buf;
    } else if (row == ROW_MUSIC) {
      label = "MUSIC VOLUME";
      SDL_snprintf(buf, sizeof(buf), "%d", (int)(music_get_volume() * 100.0f + 0.5f));
      value = buf;
    } else if (row == ROW_FULLSCREEN) {
      label = "FULLSCREEN";
      value = fullscreen_on() ? "ON" : "OFF";
    } else {
      label = "BACK";
    }

    font_draw_text(renderer, label, label_x, y, 14.0f, color);
    if (value) font_draw_text(renderer, value, value_x, y, 14.0f, color);
    y += 32.0f;
  }

  draw_centered(renderer, "ENTER REBIND - LEFT RIGHT ADJUST - ESC BACK",
                WINDOW_HEIGHT - 60.0f, 10.0f, COLOR_HINT);
}

void menu_render(SDL_Renderer *renderer) {
  // Dim the drifting world behind the menu
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
  SDL_RenderFillRect(renderer, NULL);

  if (screen == SCREEN_MAIN) render_main(renderer);
  else render_options(renderer);
}
