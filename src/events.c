// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "events.h"
#include "audio.h"
#include "dialogue.h"
#include "fx.h"

#define EVENT_MAX 128

typedef struct GameEvent {
  EventType type;
  Vec2f pos;
} GameEvent;

static GameEvent queue[EVENT_MAX];
static int queue_count = 0;

/** The one place events become sound and shake. Event positions are
 *  carried for future use (distance attenuation, music cues). */
typedef struct EventResponse {
  SfxId sfx;
  bool has_sfx;
  float shake;  // px of camera kick; 0 = none
} EventResponse;

static const EventResponse RESPONSES[EVENT_TYPE_COUNT] = {
  [EV_PLAYER_FIRED]      = { SFX_SHOOT, true, 0.0f },
  [EV_PIRATE_FIRED]      = { SFX_SHOOT_PIRATE, true, 0.0f },
  [EV_ASTEROID_CHIPPED]  = { SFX_CHIP, true, 2.0f },
  [EV_ASTEROID_BROKE]    = { SFX_BREAK, true, 3.5f },
  [EV_ASTEROID_DESTROYED] = { SFX_EXPLOSION, true, 5.0f },
  [EV_PIRATE_CHIPPED]    = { SFX_CHIP, true, 2.0f },
  [EV_PIRATE_DESTROYED]  = { SFX_EXPLOSION, true, 6.0f },
  [EV_PLAYER_HURT]       = { SFX_HURT, true, 9.0f },
  [EV_PLAYER_DIED]       = { SFX_EXPLOSION, true, 22.0f },
  [EV_SCRAP_PICKUP]      = { SFX_PICKUP, true, 0.0f },
  [EV_SCRAP_SOLD]        = { SFX_COIN, true, 0.0f },
  [EV_UPGRADE_BOUGHT]    = { SFX_UPGRADE, true, 0.0f },
  [EV_DEBT_PAYMENT]      = { SFX_COIN, true, 0.0f },
  [EV_DEBT_PAID]         = { SFX_UPGRADE, true, 0.0f },
  [EV_DOCKED]            = { SFX_SHOOT, false, 0.0f },
  [EV_SIGNAL]            = { SFX_PICKUP, true, 0.0f },
  [EV_SIGNAL_CONTRACT]   = { SFX_UPGRADE, true, 0.0f },
  [EV_QUEST_ACCEPTED]    = { SFX_PICKUP, true, 0.0f },
  [EV_QUEST_ITEM]        = { SFX_PICKUP, true, 0.0f },
  [EV_QUEST_COMPLETE]    = { SFX_UPGRADE, true, 0.0f },
  [EV_MINE_DROPPED]      = { SFX_CHIP, true, 0.0f },
  [EV_MINE_EXPLODED]     = { SFX_EXPLOSION, true, 8.0f },
  [EV_MISSILE_FIRED]     = { SFX_SHOOT_PIRATE, true, 0.0f },
  [EV_REFUEL_TICK]       = { SFX_REFUEL, true, 0.0f },
  [EV_REPAIR_TICK]       = { SFX_REPAIR, true, 0.0f },
  [EV_MINE_RACK_BOUGHT]  = { SFX_UPGRADE, true, 0.0f },
  [EV_MISSILE_POD_BOUGHT] = { SFX_UPGRADE, true, 0.0f },
  [EV_TUTORIAL_DONE]     = { SFX_COIN, true, 0.0f },
};

void events_emit(EventType type, Vec2f pos) {
  if (queue_count >= EVENT_MAX) return;
  queue[queue_count++] = (GameEvent){ .type = type, .pos = pos };
}

void events_drain(void) {
  for (int i = 0; i < queue_count; i++) {
    const EventResponse *r = &RESPONSES[queue[i].type];
    if (r->has_sfx) audio_play(r->sfx);
    if (r->shake > 0.0f) fx_shake(r->shake);
    dialogue_on_event(queue[i].type, queue[i].pos);
  }
  queue_count = 0;
}
