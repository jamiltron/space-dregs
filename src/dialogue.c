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
        if (SDL_strncmp(line, "[attendant", 10) == 0) section = &attendant_pool;
        else if (SDL_strncmp(line, "[signal", 7) == 0) section = &signal_pool;
        else if (SDL_strncmp(line, "[start", 6) == 0) section = &start_pool;
        else section = NULL;
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
  if (attendant_pool.count == 0) pool_add(&attendant_pool, "WELCOME BACK DREG");
  if (signal_pool.count == 0) pool_add(&signal_pool, "AUTOMATED BEACON - NO REPLY");
  if (start_pool.count == 0) pool_add(&start_pool, "{DEBT} IN THE HOLE - GET MINING");
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

void dialogue_run_start(int debt) {
  static int idx = 0;
  const char *tmpl = start_pool.lines[idx % start_pool.count];
  idx++;

  char buf[DIALOGUE_MAX];
  const char *ph = SDL_strstr(tmpl, "{DEBT}");
  if (ph) {
    SDL_snprintf(buf, sizeof(buf), "%.*s%d%s", (int)(ph - tmpl), tmpl, debt,
                 ph + 6);
  } else {
    SDL_strlcpy(buf, tmpl, sizeof(buf));
  }

  say(buf, (SDL_Color){ 230, 150, 90, 255 });  // debt orange

  // Opening lesson follows the debt line (quest_grant_tutorial pays it)
  char tip[DIALOGUE_MAX];
  SDL_snprintf(tip, sizeof(tip),
               "PRESS Q AT THE DOCK - ANY CONTRACT PAYS %d CR EXTRA",
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
    say("MAYDAY - FREIGHTER UNDER ATTACK - AMBER MARKER ON NAV",
        (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_ARRIVED:
    say("GET THEM OFF ME DREG - HOLD THEM OFF",
        (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_SAVED:
    say("HAUL INTACT - SALVAGE JETTISONED, CREDITS WIRED",
        (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_LOST:
    say("SIGNAL LOST - ONLY DUST ON THE SCAN",
        (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_DISTRESS_AMBUSH:
    say("NO FREIGHTER ON SCAN - IT'S BAIT",
        (SDL_Color){ 255, 90, 80, 255 });
    break;
  case EV_DISTRESS_CLEARED:
    say("DECOY CLEARED - SALVAGE BOUNTY WIRED",
        (SDL_Color){ 255, 180, 60, 255 });
    break;
  case EV_TUTORIAL_DONE: {
    char buf[DIALOGUE_MAX];
    SDL_snprintf(buf, sizeof(buf), "SIGNING BONUS WIRED - %d CR - GOOD HUNTING",
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
