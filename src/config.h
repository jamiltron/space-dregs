// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Settings persistence: key bindings, volumes and fullscreen, stored
 *  as a small key-value text file in the OS app-storage location
 *  (SDL_GetPrefPath). On the web build the file lives in an IDBFS
 *  mount backed by IndexedDB; loading there is asynchronous, so
 *  settings apply a moment after boot.
 */

#ifndef _SD_CONFIG_H
#define _SD_CONFIG_H
#include <SDL3/SDL.h>

/** Mount/locate storage, then load (async on web). */
void config_init(SDL_Window *window);

/** Write current settings; call whenever an option changes. */
void config_save(void);

/** Push pending writes to IndexedDB on web; no-op natively. */
void config_flush_storage(void);

#endif
