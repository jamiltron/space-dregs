// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "audio.h"
#include "input.h"
#include "music.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static SDL_Window *window;
static char path[512];

/** Parse the config file and apply each recognized setting. */
static void config_load(void) {
  size_t len = 0;
  char *data = SDL_LoadFile(path, &len);
  if (!data) return;  // first run: nothing saved yet

  char *line = data;
  while (line && *line) {
    char *next = SDL_strchr(line, '\n');
    if (next) *next++ = '\0';

    float fval = 0.0f;
    int a = 0, b = 0;
    if (SDL_sscanf(line, "sfx %f", &fval) == 1) {
      audio_set_sfx_volume(fval);
    } else if (SDL_sscanf(line, "music %f", &fval) == 1) {
      music_set_volume(fval);
    } else if (SDL_sscanf(line, "fullscreen %d", &a) == 1) {
      SDL_SetWindowFullscreen(window, a != 0);
    } else if (SDL_sscanf(line, "bind %d %d", &a, &b) == 2) {
      if (a >= 0 && a < ACT_COUNT && b > SDL_SCANCODE_UNKNOWN &&
          b < SDL_SCANCODE_COUNT) {
        input_set((InputAction)a, (SDL_Scancode)b);
      }
    }

    line = next;
  }

  SDL_free(data);
}

#ifdef __EMSCRIPTEN__

// IDBFS load completes asynchronously; this lands back in C when the
// IndexedDB contents have been pulled into the in-memory filesystem.
EMSCRIPTEN_KEEPALIVE void config_web_synced(void) { config_load(); }

// EM_JS rather than EM_ASM: the JS body is compiled in with a real C
// prototype and, unlike EM_ASM, needs no GNU statement expressions,
// so it works in this project's strict C99 build.
EM_JS(void, web_storage_mount, (void), {
  try { FS.mkdir('/prefs'); } catch (e) {}
  FS.mount(IDBFS, {}, '/prefs');
  FS.syncfs(true, function(err) {
    if (!err) Module._config_web_synced();
  });
});

EM_JS(void, web_storage_flush, (void), {
  FS.syncfs(false, function(err) {});
});

static void storage_init(void) {
  SDL_strlcpy(path, "/prefs/config.txt", sizeof(path));
  web_storage_mount();
}

static void storage_flush(void) {
  web_storage_flush();
}

#else

static void storage_init(void) {
  char *pref = SDL_GetPrefPath("justinhamilton", "SpaceDregs");
  if (pref) {
    SDL_snprintf(path, sizeof(path), "%sconfig.txt", pref);
    SDL_free(pref);
  }
  config_load();
}

static void storage_flush(void) {}

#endif

void config_init(SDL_Window *w) {
  window = w;
  storage_init();
}

void config_flush_storage(void) { storage_flush(); }

void config_save(void) {
  if (!path[0]) return;

  SDL_IOStream *io = SDL_IOFromFile(path, "w");
  if (!io) return;

  SDL_IOprintf(io, "sfx %.2f\n", audio_get_sfx_volume());
  SDL_IOprintf(io, "music %.2f\n", music_get_volume());
  SDL_IOprintf(io, "fullscreen %d\n",
               (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) ? 1 : 0);
  for (int a = 0; a < ACT_COUNT; a++) {
    SDL_IOprintf(io, "bind %d %d\n", a, (int)input_get((InputAction)a));
  }

  SDL_CloseIO(io);
  storage_flush();
}
