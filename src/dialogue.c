// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dialogue.h"
#include "app.h"
#include "font.h"
#include "input.h"
#include "quest.h"

#define DIALOGUE_MAX 96
#define DIALOGUE_CPS 32.0f      // typewriter characters per second
#define DIALOGUE_LINGER 5.0f    // seconds after fully revealed
#define POOL_MAX 32

/** One section of assets/dialogue.txt. */
typedef struct LinePool {
  char lines[POOL_MAX][DIALOGUE_MAX];
  int count;
  int last;  // last index spoken, to avoid immediate repeats
} LinePool;

static LinePool attendant_pool = { .last = -1 };
static LinePool signal_pool = { .last = -1 };
static LinePool start_pool = { .last = -1 };
static LinePool tutorial_tip_pool = { .last = -1 };
static LinePool tutorial_done_pool = { .last = -1 };
static LinePool distress_call_pool = { .last = -1 };
static LinePool distress_arrived_pool = { .last = -1 };
static LinePool distress_saved_pool = { .last = -1 };
static LinePool distress_lost_pool = { .last = -1 };
static LinePool distress_ambush_pool = { .last = -1 };
static LinePool distress_cleared_pool = { .last = -1 };
static LinePool freighter_killed_pool = { .last = -1 };
static LinePool distress_abandoned_pool = { .last = -1 };
static LinePool guild_respected_pool = { .last = -1 };
static LinePool guild_outcast_pool = { .last = -1 };
static LinePool clans_respected_pool = { .last = -1 };
static LinePool hunters_pool = { .last = -1 };

/** Maps a dialogue.txt [section] tag to its pool; the fallback keeps
 *  that voice alive if the file is missing or the section is empty. */
typedef struct SectionMap {
  const char *tag;
  LinePool *pool;
  const char *fallback;
} SectionMap;

static const SectionMap SECTIONS[] = {
  { "[attendant", &attendant_pool, "WELCOME BACK DREG" },
  { "[signal", &signal_pool, "AUTOMATED BEACON - NO REPLY" },
  { "[start", &start_pool, "{DEBT} IN THE HOLE - GET MINING" },
  { "[tutorial-tip", &tutorial_tip_pool,
    "PRESS Q AT THE DOCK - ANY CONTRACT PAYS {BONUS} CR EXTRA" },
  { "[tutorial-done", &tutorial_done_pool,
    "SIGNING BONUS WIRED - {BONUS} CR - GOOD HUNTING" },
  { "[distress-call", &distress_call_pool,
    "MAYDAY - FREIGHTER UNDER ATTACK - AMBER MARKER ON NAV" },
  { "[distress-arrived", &distress_arrived_pool,
    "GET THEM OFF ME DREG - HOLD THEM OFF" },
  { "[distress-saved", &distress_saved_pool,
    "HAUL INTACT - SALVAGE JETTISONED, CREDITS WIRED" },
  { "[distress-lost", &distress_lost_pool,
    "SIGNAL LOST - ONLY DUST ON THE SCAN" },
  { "[distress-ambush", &distress_ambush_pool,
    "NO FREIGHTER ON SCAN - IT'S BAIT" },
  { "[distress-cleared", &distress_cleared_pool,
    "DECOY CLEARED - SALVAGE BOUNTY WIRED" },
  { "[freighter-killed", &freighter_killed_pool,
    "THAT WAS A CIVILIAN HAULER - THE GUILD WON'T FORGET" },
  { "[distress-abandoned", &distress_abandoned_pool,
    "RESCUE OFF THE SCOPE - THE HAULER'S ON ITS OWN" },
  { "[guild-respected", &guild_respected_pool,
    "THE GUILD KNOWS YOUR NAME NOW - PRICES REFLECT IT" },
  { "[guild-outcast", &guild_outcast_pool,
    "GUILD NOTICE - NO CONTRACTS, AND YOU PAY OUTSIDER RATES" },
  { "[clans-respected", &clans_respected_pool,
    "CLAN CHANNELS CALL YOU FRIEND - THE PACKS WILL PASS YOU BY" },
  { "[hunters-inbound", &hunters_pool,
    "CLAN CHATTER SPIKES - HUNTERS ARE COMING FOR YOU" },
};
static const int SECTION_COUNT = (int)(sizeof(SECTIONS) / sizeof(SECTIONS[0]));

static char current[DIALOGUE_MAX];
static SDL_Color color;
static bool hold;        // instruction/milestone line: chatter can't replace it
static float revealed;   // characters shown so far
static float linger;     // countdown after full reveal

static char queued[DIALOGUE_MAX];  // follow-up line, spoken after current
static SDL_Color queued_color;

static void pool_add(LinePool *p, const char *s) {
  if (p->count >= POOL_MAX) return;
  SDL_strlcpy(p->lines[p->count++], s, DIALOGUE_MAX);
}

void dialogue_init(void) {
  const char *base = SDL_GetBasePath();
  char path[512];
  SDL_snprintf(path, sizeof(path), "%sassets/dialogue.txt", base ? base : "");

  size_t len = 0;
  char *data = SDL_LoadFile(path, &len);
  if (data) {
    LinePool *section = NULL;
    char *line = data;
    while (line && *line) {
      char *next = SDL_strchr(line, '\n');
      if (next) *next++ = '\0';
      size_t n = SDL_strlen(line);
      if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';

      if (line[0] == '[') {
        section = NULL;
        for (int i = 0; i < SECTION_COUNT; i++) {
          if (SDL_strncmp(line, SECTIONS[i].tag,
                          SDL_strlen(SECTIONS[i].tag)) == 0) {
            section = SECTIONS[i].pool;
            break;
          }
        }
      } else if (line[0] != '#' && line[0] != '\0' && section) {
        pool_add(section, line);
      }
      line = next;
    }
    SDL_free(data);
  } else {
    SDL_Log("dialogue: %s missing, using fallback lines", path);
  }

  // The game keeps talking even if the file is lost
  for (int i = 0; i < SECTION_COUNT; i++) {
    if (SECTIONS[i].pool->count == 0) {
      pool_add(SECTIONS[i].pool, SECTIONS[i].fallback);
    }
  }
}

static void say(const char *text, SDL_Color c) {
  SDL_strlcpy(current, text, sizeof(current));
  color = c;
  hold = true;
  queued[0] = '\0';  // a milestone supersedes any pending follow-up
  revealed = 0.0f;
  linger = DIALOGUE_LINGER;
}

/** Ambient pool lines: dropped rather than stomping an instruction. */
static void chatter(const char *text, SDL_Color c) {
  if ((current[0] && hold) || queued[0]) return;
  SDL_strlcpy(current, text, sizeof(current));
  color = c;
  hold = false;
  revealed = 0.0f;
  linger = DIALOGUE_LINGER;
}

/** Queue a line to speak once the current one expires. */
static void enqueue(const char *text, SDL_Color c) {
  SDL_strlcpy(queued, text, sizeof(queued));
  queued_color = c;
}

/** Pick from a pool without repeating the previous choice. */
static const char *pick(LinePool *p) {
  int i;
  do { i = (int)SDL_rand(p->count); } while (p->count > 1 && i == p->last);
  p->last = i;
  return p->lines[i];
}

/** Copy tmpl into out with the first ph replaced by value. */
static void expand_int(char *out, size_t n, const char *tmpl, const char *ph,
                       int value) {
  const char *hit = SDL_strstr(tmpl, ph);
  if (hit) {
    SDL_snprintf(out, n, "%.*s%d%s", (int)(hit - tmpl), tmpl, value,
                 hit + SDL_strlen(ph));
  } else {
    SDL_strlcpy(out, tmpl, n);
  }
}

void dialogue_run_start(int debt) {
  static int idx = 0;
  const char *tmpl = start_pool.lines[idx % start_pool.count];
  idx++;

  char buf[DIALOGUE_MAX];
  expand_int(buf, sizeof(buf), tmpl, "{DEBT}", debt);
  say(buf, (SDL_Color){ 230, 150, 90, 255 });  // debt orange

  // Opening lesson follows the debt line (quest_grant_tutorial pays it)
  char tip[DIALOGUE_MAX];
  expand_int(tip, sizeof(tip), pick(&tutorial_tip_pool), "{BONUS}",
             QUEST_TUTORIAL_REWARD);
  enqueue(tip, (SDL_Color){ 230, 120, 255, 255 });
}

/** "PRESS <key> TO ..." with the action's live binding filled in, so
 *  rebinds keep the tip truthful. Key names use font-safe glyphs. */
static void say_key_tip(const char *fmt, InputAction act, SDL_Color c) {
  const char *key = SDL_GetScancodeName(input_get(act));
  if (!key || !key[0]) key = "THE BOUND KEY";

  char buf[DIALOGUE_MAX];
  SDL_snprintf(buf, sizeof(buf), fmt, key);
  say(buf, c);
}

void dialogue_on_event(EventType type, Vec2f pos) {
  (void)pos;
  switch (type) {
  case EV_DOCKED:
    chatter(pick(&attendant_pool), (SDL_Color){ 140, 225, 235, 255 });
    break;
  case EV_SIGNAL:
    chatter(pick(&signal_pool), (SDL_Color){ 120, 230, 255, 255 });
    break;
  case EV_SIGNAL_CONTRACT:
    say("INTERCEPTED POSTING - A MARK IS ON YOUR NAV",
        (SDL_Color){ 230, 120, 255, 255 });
    break;
  case EV_DEBT_PAID:
    say("DEBT CLEAR - THE SHIP IS FINALLY YOURS",
        (SDL_Color){ 255, 200, 80, 255 });
    break;
  case EV_QUEST_COMPLETE:
    say("CONTRACT SETTLED - PAYMENT WIRED",
        (SDL_Color){ 230, 120, 255, 255 });
    break;
  case EV_MINE_RACK_BOUGHT:
    say_key_tip("MINE RACK FITTED - PRESS %s TO DROP ONE ASTERN",
                ACT_MINE, (SDL_Color){ 255, 110, 95, 255 });
    break;
  case EV_MISSILE_POD_BOUGHT:
    say_key_tip("MISSILE POD FITTED - PRESS %s TO FIRE, IT SEEKS",
                ACT_MISSILE, (SDL_Color){ 255, 200, 120, 255 });
    break;
  case EV_DISTRESS_CALL:
    say(pick(&distress_call_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_ARRIVED:
    say(pick(&distress_arrived_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_SAVED:
    say(pick(&distress_saved_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_LOST:
    say(pick(&distress_lost_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_AMBUSH:
    say(pick(&distress_ambush_pool), (SDL_Color){ 255, 90, 80, 255 });
    break;
  case EV_DISTRESS_CLEARED:
    say(pick(&distress_cleared_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_FREIGHTER_KILLED:
    say(pick(&freighter_killed_pool), (SDL_Color){ 255, 90, 80, 255 });
    break;
  case EV_DISTRESS_ABANDONED:
    say(pick(&distress_abandoned_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_GUILD_RESPECTED:
    say(pick(&guild_respected_pool), (SDL_Color){ 80, 220, 230, 255 });
    break;
  case EV_GUILD_OUTCAST:
    say(pick(&guild_outcast_pool), (SDL_Color){ 255, 90, 80, 255 });
    break;
  case EV_CLANS_RESPECTED:
    say(pick(&clans_respected_pool), (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_HUNTERS_INBOUND:
    say(pick(&hunters_pool), (SDL_Color){ 255, 90, 80, 255 });
    break;
  case EV_TUTORIAL_DONE: {
    char buf[DIALOGUE_MAX];
    expand_int(buf, sizeof(buf), pick(&tutorial_done_pool), "{BONUS}",
               QUEST_TUTORIAL_REWARD);
    say(buf, (SDL_Color){ 255, 200, 80, 255 });
    break;
  }
  default:
    break;
  }
}

void dialogue_update(float dt) {
  if (!current[0]) return;

  float len = (float)SDL_strlen(current);
  if (revealed < len) {
    revealed += DIALOGUE_CPS * dt;
    if (revealed > len) revealed = len;
  } else {
    linger -= dt;
    if (linger <= 0.0f) {
      current[0] = '\0';
      if (queued[0]) {
        SDL_strlcpy(current, queued, sizeof(current));
        color = queued_color;
        hold = true;
        queued[0] = '\0';
        revealed = 0.0f;
        linger = DIALOGUE_LINGER;
      }
    }
  }
}

void dialogue_render(SDL_Renderer *renderer) {
  if (!current[0]) return;

  char shown[DIALOGUE_MAX];
  int n = (int)revealed;
  SDL_strlcpy(shown, current, sizeof(shown));
  if (n < (int)SDL_strlen(shown)) shown[n] = '\0';

  // Centered on the FULL line's width so text doesn't slide as it types
  float w = font_text_width(current, 12.0f);
  font_draw_text(renderer, shown, (WINDOW_WIDTH - w) / 2.0f,
                 WINDOW_HEIGHT - 44.0f, 12.0f, color);
}
