// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

/** @file
 *  Station contracts, one active at a time.
 *
 *  The board always offers two contracts, each type independently
 *  random; rerolled per dock and after completion/abandon.
 *  Accepting spawns the objective, marked with C_QUEST_TARGET so
 *  completion tracking survives entity id recycling; the HUD adds a
 *  second compass pointing at it.
 */

#ifndef _SD_QUEST_H
#define _SD_QUEST_H
#include <stdbool.h>
#include "ecs.h"

#define QUEST_MAX_OFFERS 3

typedef enum QuestType {
  QUEST_NONE,
  QUEST_FETCH,    /**< Recover a beacon from deep space, return to any station. */
  QUEST_BOUNTY,   /**< Destroy a marked, tougher pirate. */
  QUEST_DELIVER,  /**< Haul a package to a destination station. */
} QuestType;

/** Active contract plus the docked station's current offers. */
typedef struct Quest {
  QuestType type;        /**< Active quest, QUEST_NONE when idle. */
  int reward;
  Vec2f target_pos;      /**< Kept current from the live target entity. */
  float complete_timer;  /**< "QUEST COMPLETE" banner time remaining. */
  bool carrying;         /**< Fetch beacon / delivery package aboard. */

  QuestType offers[QUEST_MAX_OFFERS];
  int offer_rewards[QUEST_MAX_OFFERS];
  int offer_count;       /**< 0 = board not rolled yet. */
} Quest;

/** Clear everything (new run). */
void quest_reset(Quest *q);

/** Roll offers while docked, track the objective, detect completion. */
void quest_update(Quest *q, World *world, Entity player, float dt);

/** Accept offer index while docked with no active quest.
 *  @return true if the contract was taken. */
bool quest_try_accept(Quest *q, World *world, Entity player, int index);

/** A signal beacon assigns a bounty directly (no-op with a quest active). */
void quest_grant_bounty(Quest *q, World *world, Vec2f from);

/** Drop the active contract; the objective is unmarked or removed. */
void quest_abandon(Quest *q, World *world);

/** Respawn the active quest's objective after loading a save. */
void quest_restore(Quest *q, World *world);

/** Where the quest compass should point.
 *  @return false when there is nothing to point at. */
bool quest_compass_target(const Quest *q, Vec2f *out);

/** Contract name for the board ("BOUNTY CONTRACT", ...). */
const char *quest_offer_label(QuestType t);

/** Progress text for the HUD ("HUNT THE MARK", ...). */
const char *quest_status_label(const Quest *q);

#endif
