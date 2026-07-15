// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Run persistence: chunk seed, camera/time, full player state and
 *  the active quest. The world regenerates deterministically from the
 *  seed, so destroyed rocks return on load — the run's progress is
 *  what persists. Stored next to config.txt (pref path / IDBFS).
 */

#ifndef _SD_SAVE_H
#define _SD_SAVE_H
#include <stdbool.h>
#include "app.h"

/** True when a saved run exists (drives the CONTINUE menu item). */
bool save_exists(void);

/** Checkpoint the run. @return false unless the player is alive. */
bool save_write(App *app);

/** Rebuild the world from the save. @return false if none/invalid. */
bool save_read(App *app);

/** Delete the save — the run is over. */
void save_delete(void);

#endif
