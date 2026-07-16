// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include "save.h"
#include "asteroid.h"
#include "config.h"
#include "pirate.h"
#include "scrap.h"
#include "ship.h"
#include "signal.h"
#include "station.h"

static char path[512];

static void ensure_path(void) {
  if (path[0]) return;

#ifdef __EMSCRIPTEN__
  SDL_strlcpy(path, "/prefs/save.txt", sizeof(path));  // IDBFS, see config.c
#else
  char *pref = SDL_GetPrefPath("justinhamilton", "SpaceDregs");
  if (pref) {
    SDL_snprintf(path, sizeof(path), "%ssave.txt", pref);
    SDL_free(pref);
  }
#endif
}

bool save_exists(void) {
  ensure_path();
  return path[0] && SDL_GetPathInfo(path, NULL);
}

bool save_write(App *app) {
  ensure_path();
  if (!path[0]) return false;
  if (!entity_has(&app->world, app->player, C_PLAYER)) return false;

  SDL_IOStream *io = SDL_IOFromFile(path, "w");
  if (!io) return false;

  const World *w = &app->world;
  const Player *p = &w->players[app->player];
  const Transform *tf = &w->transforms[app->player];
  const Velocity *vel = &w->velocities[app->player];

  SDL_IOprintf(io, "seed %u %u\n", (Uint32)(app->chunks.seed >> 32),
               (Uint32)app->chunks.seed);
  SDL_IOprintf(io, "rng %u %u\n", (Uint32)(app->world.rng >> 32),
               (Uint32)app->world.rng);
  SDL_IOprintf(io, "home %f %f\n", app->chunks.home.x, app->chunks.home.y);
  SDL_IOprintf(io, "camera %f %f\n", app->camera.x, app->camera.y);
  SDL_IOprintf(io, "time %f\n", app->time);
  SDL_IOprintf(io, "pos %f %f %f\n", tf->position.x, tf->position.y, tf->angle);
  SDL_IOprintf(io, "vel %f %f\n", vel->value.x, vel->value.y);
  SDL_IOprintf(io, "hull %d %d\n", p->hp, p->max_hp);
  SDL_IOprintf(io, "shield %d %d\n", p->shield, p->shield_max);
  SDL_IOprintf(io, "mineammo %d %d\n", p->mines, p->mines_max);
  SDL_IOprintf(io, "missileammo %d %d\n", p->missiles, p->missiles_max);
  SDL_IOprintf(io, "fuel %f %f\n", p->fuel, p->fuel_max);
  SDL_IOprintf(io, "money %d\n", p->money);
  SDL_IOprintf(io, "scrap %d\n", p->scrap);
  SDL_IOprintf(io, "debt %d\n", p->debt);
  SDL_IOprintf(io, "bounties %d\n", p->bounties_done);
  SDL_IOprintf(io, "stats %f %f %f\n", p->thrust_force, p->fire_interval,
               p->magnet_radius);
  for (int i = 0; i < UPGRADE_COUNT; i++) {
    SDL_IOprintf(io, "up %d %d\n", i, p->upgrades[i]);
  }
  SDL_IOprintf(io, "fac %d %d %f\n",
               app->world.factions.standing[FACTION_GUILD],
               app->world.factions.standing[FACTION_CLANS],
               app->world.factions.heat);

  if (app->quest.type != QUEST_NONE) {
    SDL_IOprintf(io, "quest %d %d %f %f %d\n", (int)app->quest.type,
                 app->quest.reward, app->quest.target_pos.x,
                 app->quest.target_pos.y, app->quest.carrying ? 1 : 0);
  }

  // Generated cells + every live field entity, so continuing restores
  // the world as-is instead of re-rolling it from the seed.
  for (int i = 0; i < CHUNK_TABLE_CAP; i++) {
    if (!app->chunks.used[i]) continue;
    SDL_IOprintf(io, "cell %d %d\n",
                 (int)(Uint32)(app->chunks.keys[i] >> 32),
                 (int)(Uint32)app->chunks.keys[i]);
  }
  for (Entity e = 0; e < w->high_water; e++) {
    if (!entity_has(w, e, C_TRANSFORM)) continue;
    if (entity_has(w, e, C_QUEST_TARGET)) continue;  // quest_restore respawns it

    const Transform *t = &w->transforms[e];
    const Velocity *v = &w->velocities[e];

    if (entity_has(w, e, C_ASTEROID)) {
      const Asteroid *ast = &w->asteroids[e];
      SDL_IOprintf(io, "ast %f %f %f %f %f %f %f %d %d %d\n",
                   t->position.x, t->position.y, t->angle,
                   v->value.x, v->value.y, v->spin,
                   ast->radius, ast->generation, ast->hp, ast->kind);
    } else if (entity_has(w, e, C_PIRATE)) {
      const Pirate *pir = &w->pirates[e];
      SDL_IOprintf(io, "pir %f %f %f %f %f %d %d %d\n",
                   t->position.x, t->position.y, t->angle,
                   v->value.x, v->value.y, pir->hp, pir->loot,
                   pir->archetype);
    } else if (entity_has(w, e, C_SCRAP)) {
      SDL_IOprintf(io, "scr %f %f %f %f\n", t->position.x, t->position.y,
                   v->value.x, v->value.y);
    } else if (entity_has(w, e, C_SIGNAL)) {
      SDL_IOprintf(io, "sig %f %f %d\n", t->position.x, t->position.y,
                   w->signals[e].contract ? 1 : 0);
    } else if (entity_has(w, e, C_STATION)) {
      // The home station is respawned by save_read; only wild ones persist
      if (vec2f_length(vec2f_sub(t->position, app->chunks.home)) < 1.0f)
        continue;
      const Wireframe *wf = &w->wireframes[e];
      SDL_IOprintf(io, "sta %f %f %d %d %d\n", t->position.x, t->position.y,
                   wf->color.r, wf->color.g, wf->color.b);
    }
  }

  SDL_CloseIO(io);
  config_flush_storage();
  return true;
}

bool save_read(App *app) {
  ensure_path();

  size_t len = 0;
  char *data = SDL_LoadFile(path, &len);
  if (!data) return false;

  // Parse into locals first; the world is only rebuilt on success
  Uint32 seed_hi = 0, seed_lo = 0;
  Uint32 rng_hi = 0, rng_lo = 0;
  Vec2f home = vec2f_new(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);
  Vec2f camera = home, pos = home, vel = { 0 };
  float angle = 0.0f, time = 0.0f;
  float fuel = SHIP_FUEL_MAX, fuel_max = SHIP_FUEL_MAX;
  float thrust = SHIP_THRUST_FORCE, fire = 0.0f, magnet = 0.0f;
  int hp = SHIP_MAX_HP, max_hp = SHIP_MAX_HP, money = 0, scrap = 0;
  int shield = 0, shield_max = 0;
  int mines = 0, mines_max = 0;
  int missiles = 0, missiles_max = 0;
  int debt = SHIP_START_DEBT;
  int bounties = 0;
  int ups[UPGRADE_COUNT] = { 0 };
  int q_type = 0, q_reward = 0, q_carrying = 0;
  Vec2f q_pos = { 0 };

  char *line = data;
  while (line && *line) {
    char *next = SDL_strchr(line, '\n');
    if (next) *next++ = '\0';

    int a = 0, b = 0;
    if (SDL_sscanf(line, "seed %u %u", &seed_hi, &seed_lo) == 2) {}
    else if (SDL_sscanf(line, "rng %u %u", &rng_hi, &rng_lo) == 2) {}
    else if (SDL_sscanf(line, "home %f %f", &home.x, &home.y) == 2) {}
    else if (SDL_sscanf(line, "camera %f %f", &camera.x, &camera.y) == 2) {}
    else if (SDL_sscanf(line, "time %f", &time) == 1) {}
    else if (SDL_sscanf(line, "pos %f %f %f", &pos.x, &pos.y, &angle) == 3) {}
    else if (SDL_sscanf(line, "vel %f %f", &vel.x, &vel.y) == 2) {}
    else if (SDL_sscanf(line, "hull %d %d", &hp, &max_hp) == 2) {}
    else if (SDL_sscanf(line, "shield %d %d", &shield, &shield_max) == 2) {}
    else if (SDL_sscanf(line, "mineammo %d %d", &mines, &mines_max) == 2) {}
    else if (SDL_sscanf(line, "missileammo %d %d", &missiles, &missiles_max) == 2) {}
    else if (SDL_sscanf(line, "fuel %f %f", &fuel, &fuel_max) == 2) {}
    else if (SDL_sscanf(line, "gas %f %f", &fuel, &fuel_max) == 2) {}  // pre-rename saves
    else if (SDL_sscanf(line, "money %d", &money) == 1) {}
    else if (SDL_sscanf(line, "scrap %d", &scrap) == 1) {}
    else if (SDL_sscanf(line, "debt %d", &debt) == 1) {}
    else if (SDL_sscanf(line, "bounties %d", &bounties) == 1) {}
    else if (SDL_sscanf(line, "stats %f %f %f", &thrust, &fire, &magnet) == 3) {}
    else if (SDL_sscanf(line, "up %d %d", &a, &b) == 2) {
      if (a >= 0 && a < UPGRADE_COUNT) ups[a] = b;
    }
    else if (SDL_sscanf(line, "quest %d %d %f %f %d", &q_type, &q_reward,
                        &q_pos.x, &q_pos.y, &q_carrying) == 5) {}

    line = next;
  }

  Uint64 seed = ((Uint64)seed_hi << 32) | seed_lo;
  world_init(&app->world);
  app->world.rng = ((Uint64)rng_hi << 32) | rng_lo;
  if (app->world.rng == 0) app->world.rng = (seed ^ 0x9E3779B97F4A7C15ULL) | 1;
  chunks_init(&app->chunks, seed, home);
  station_spawn(&app->world, home, STATION_HOME_COLOR);
  station_reset();
  app->player = ship_spawn(&app->world, pos);

  World *w = &app->world;
  Player *p = &w->players[app->player];
  w->transforms[app->player].angle = angle;
  w->velocities[app->player].value = vel;
  p->hp = hp;
  p->max_hp = max_hp;
  p->shield = shield;
  p->shield_max = shield_max;
  p->mines = mines;
  p->mines_max = mines_max;
  p->missiles = missiles;
  p->missiles_max = missiles_max;
  p->fuel = fuel;
  p->fuel_max = fuel_max;
  p->money = money;
  p->scrap = scrap;
  p->debt = debt;
  p->bounties_done = bounties;
  p->thrust_force = thrust;
  if (fire > 0.0f) p->fire_interval = fire;
  if (magnet > 0.0f) p->magnet_radius = magnet;
  for (int i = 0; i < UPGRADE_COUNT; i++) p->upgrades[i] = ups[i];

  // Second pass over the (now NUL-separated) lines: restore the field.
  // Cells are marked as generated so chunks_update won't re-roll them;
  // entities respawn via their factories, then get their saved state.
  // Old saves without these lines just regenerate from the seed.
  for (char *l = data; l < data + len; l += SDL_strlen(l) + 1) {
    int ca = 0, cb = 0, gen = 0, ihp = 0, loot = 0, flag = 0, kind = 0;
    int cr = 0, cg = 0, cb2 = 0;
    Vec2f ep = { 0 }, ev = { 0 };
    float ang = 0.0f, spin = 0.0f, radius = 0.0f;

    float heat = 0.0f;
    if (SDL_sscanf(l, "cell %d %d", &ca, &cb) == 2) {
      chunks_restore_cell(&app->chunks, ca, cb);
    } else if (SDL_sscanf(l, "fac %d %d %f", &ca, &cb, &heat) == 3) {
      app->world.factions.standing[FACTION_GUILD] = ca;
      app->world.factions.standing[FACTION_CLANS] = cb;
      app->world.factions.heat = heat;
    } else if (SDL_sscanf(l, "ast %f %f %f %f %f %f %f %d %d %d", &ep.x, &ep.y,
                          &ang, &ev.x, &ev.y, &spin, &radius, &gen,
                          &ihp, &kind) >= 9) {
      // kind stays 0 (plain) in pre-rich saves with 9 fields
      Entity e = asteroid_spawn(w, ep, radius);
      if (kind == ASTEROID_RICH) asteroid_make_rich(w, e);
      w->transforms[e].angle = ang;
      w->velocities[e].value = ev;
      w->velocities[e].spin = spin;
      w->asteroids[e].generation = gen;
      w->asteroids[e].hp = ihp;  // after make_rich so the bonus doesn't stack
    } else if (SDL_sscanf(l, "pir %f %f %f %f %f %d %d %d", &ep.x, &ep.y,
                          &ang, &ev.x, &ev.y, &ihp, &loot, &kind) >= 7) {
      // kind stays 0 (dart) in pre-archetype saves with 7 fields
      Entity e = pirate_spawn(w, ep, (PirateArchetype)kind);
      w->transforms[e].angle = ang;
      w->velocities[e].value = ev;
      w->pirates[e].hp = ihp;
      w->pirates[e].loot = loot;
    } else if (SDL_sscanf(l, "scr %f %f %f %f", &ep.x, &ep.y, &ev.x,
                          &ev.y) == 4) {
      scrap_spawn(w, ep, ev);
    } else if (SDL_sscanf(l, "sig %f %f %d", &ep.x, &ep.y, &flag) == 3) {
      signal_spawn(w, ep, flag != 0);
    } else if (SDL_sscanf(l, "sta %f %f %d %d %d", &ep.x, &ep.y, &cr, &cg,
                          &cb2) == 5) {
      station_spawn(w, ep,
                    (SDL_Color){ (Uint8)cr, (Uint8)cg, (Uint8)cb2, 255 });
    }
  }
  SDL_free(data);

  quest_reset(&app->quest);
  if (q_type > QUEST_NONE && q_type <= QUEST_TUTORIAL) {
    app->quest.type = (QuestType)q_type;
    app->quest.reward = q_reward;
    app->quest.target_pos = q_pos;
    app->quest.carrying = q_carrying != 0;
    quest_restore(&app->quest, &app->world);
  }
  distress_reset(&app->distress);  // calls are transient; not saved

  app->camera = camera;
  app->time = time;
  chunks_update(&app->chunks, &app->world, camera);
  app->state = STATE_PLAYING;

  return true;
}

void save_delete(void) {
  ensure_path();
  if (!path[0]) return;
  SDL_RemovePath(path);
  config_flush_storage();
}
