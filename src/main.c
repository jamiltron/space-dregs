// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // platform entry point (WinMain on Windows)
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "app.h"
#include "audio.h"
#include "config.h"
#include "dialogue.h"
#include "events.h"
#include "faction.h"
#include "font.h"
#include "fx.h"
#include "hud.h"
#include "input.h"
#include "menu.h"
#include "music.h"
#include "quest.h"
#include "replay.h"
#include "save.h"
#include "ship.h"
#include "station.h"
#include "systems.h"

static App app = { 0 };
static bool muted = false;

/* Fixed-timestep simulation: real frame time accumulates and the world
 * steps in SIM_DT slices, so physics behaves identically at any fps. */
#define SIM_DT (1.0f / 120.0f)
static float sim_accumulator = 0.0f;

/* The scene renders supersampled at SCENE_SS x logical resolution, then
 * downsamples to the real output — integer-scaled primitives keep the
 * hairline vector font intact, and the resample antialiases every line. */
#define SCENE_SS 2

static Uint64 random_seed(void) {
  return ((Uint64)SDL_rand_bits() << 32) | SDL_rand_bits();
}

/** Start a fresh run: world, home station, ship, first chunks.
 *  Everything random derives from seed, so a run is reproducible. */
static void game_reset(Uint64 seed) {
  Vec2f spawn = vec2f_new(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);

  world_init(&app.world);
  app.world.rng = (seed ^ 0x9E3779B97F4A7C15ULL) | 1;
  station_spawn(&app.world, spawn, STATION_HOME_COLOR);
  station_reset();
  app.player = ship_spawn(&app.world, spawn);

  chunks_init(&app.chunks, seed, spawn);
  chunks_update(&app.chunks, &app.world, spawn);
  quest_reset(&app.quest);
  quest_grant_tutorial(&app.quest, spawn);
  distress_reset(&app.distress);
  app.quest_board = false;

  app.camera = spawn;
  app.time = 0.0f;
  app.state = STATE_PLAYING;
}

/** One fixed simulation step: everything that advances the world by dt. */
static void simulate(float dt) {
  // Snapshot transforms so rendering can interpolate between steps
  SDL_memcpy(app.world.prev_transforms, app.world.transforms,
             sizeof(app.world.transforms));

  // Capture or inject this step's input, then any recorded discrete
  // actions that are due (no-ops when replay is off)
  replay_step();
  int code, arg;
  while (replay_next_action(&code, &arg)) {
    switch (code) {
    case RA_BUY_UPGRADE:
      station_try_buy(&app.world, app.player, (UpgradeId)arg);
      break;
    case RA_QUEST_ACCEPT:
      quest_try_accept(&app.quest, &app.world, app.player, arg);
      break;
    case RA_QUEST_ABANDON:
      quest_abandon(&app.quest, &app.world);
      break;
    case RA_BUY_SHIELD:
      station_try_buy_shield(&app.world, app.player);
      break;
    case RA_BUY_MINE:
      station_try_buy_mine(&app.world, app.player);
      break;
    case RA_BUY_MISSILE:
      station_try_buy_missile(&app.world, app.player);
      break;
    default:
      break;
    }
  }

  app.time += dt;

  chunks_update(&app.chunks, &app.world, app.camera);
  system_freeze(&app.world, app.camera, app.time);

  system_player(&app.world, dt);
  system_pirate(&app.world, dt);
  system_movement(&app.world, dt);
  system_missiles(&app.world, dt);
  system_freighters(&app.world, app.camera, dt);
  system_collision(&app.world);
  system_scrap(&app.world, dt);
  system_mines(&app.world, dt);
  system_lifetime(&app.world, dt);
  system_signals(&app.world, &app.quest);
  station_update(&app.world, app.player, dt);
  quest_update(&app.quest, &app.world, app.player, dt);
  distress_update(&app.distress, &app.world, app.player, dt);
  faction_update(&app.world, app.player, dt);

  // The board closes itself when you drift off the station
  if (app.quest_board && !station_docked(&app.world, app.player)) {
    app.quest_board = false;
  }

  if (entity_has(&app.world, app.player, C_PLAYER)) {
    app.camera = app.world.transforms[app.player].position;
    app.death_timer = SHIP_DEATH_DELAY;
  } else if (app.state == STATE_PLAYING) {
    // Let the explosion settle before calling it
    app.death_timer -= dt;
    if (app.death_timer <= 0.0f) {
      app.state = STATE_GAME_OVER;
      save_delete();  // the run is over; no continuing out of death
    }
  }

  replay_step_done();
}

/** Pause overlay: purchased upgrades and the resume/menu/quit hints. */
static void pause_render(SDL_Renderer *renderer) {
  const char *title = "PAUSED";
  float size = 40.0f;
  float y = WINDOW_HEIGHT * 0.25f;
  font_draw_text(renderer, title,
                 (WINDOW_WIDTH - font_text_width(title, size)) / 2.0f,
                 y, size, (SDL_Color){ 180, 200, 255, 255 });
  y += 80.0f;

  font_draw_text(renderer, "UPGRADES",
                 (WINDOW_WIDTH - font_text_width("UPGRADES", 16.0f)) / 2.0f,
                 y, 16.0f, (SDL_Color){ 80, 220, 230, 255 });
  y += 34.0f;

  int shown = 0;
  if (entity_has(&app.world, app.player, C_PLAYER)) {
    const Player *p = &app.world.players[app.player];
    char buf[48];

    for (int i = 0; i < UPGRADE_COUNT; i++) {
      if (p->upgrades[i] == 0) continue;
      SDL_snprintf(buf, sizeof(buf), "%s LVL %d",
                   STATION_UPGRADES[i].name, p->upgrades[i]);
      font_draw_text(renderer, buf,
                     (WINDOW_WIDTH - font_text_width(buf, 14.0f)) / 2.0f,
                     y, 14.0f, (SDL_Color){ 255, 200, 80, 255 });
      y += 26.0f;
      shown++;
    }
  }
  if (shown == 0) {
    font_draw_text(renderer, "NONE",
                   (WINDOW_WIDTH - font_text_width("NONE", 14.0f)) / 2.0f,
                   y, 14.0f, (SDL_Color){ 110, 120, 140, 255 });
    y += 26.0f;
  }

  char standings[48];
  SDL_snprintf(standings, sizeof(standings), "GUILD %+d - CLANS %+d",
               app.world.factions.standing[FACTION_GUILD],
               app.world.factions.standing[FACTION_CLANS]);
  font_draw_text(renderer, standings,
                 (WINDOW_WIDTH - font_text_width(standings, 14.0f)) / 2.0f,
                 y + 8.0f, 14.0f, (SDL_Color){ 230, 150, 90, 255 });
  y += 34.0f;

#ifdef __EMSCRIPTEN__
  const char *hint = "ESC RESUME - M MENU";
#else
  const char *hint = "ESC RESUME - M MENU - Q QUIT";
#endif
  font_draw_text(renderer, hint,
                 (WINDOW_WIDTH - font_text_width(hint, 12.0f)) / 2.0f,
                 y + 30.0f, 12.0f, (SDL_Color){ 110, 120, 140, 255 });
}

static void game_over_render(SDL_Renderer *renderer) {
  const char *title = "GAME OVER";
  float size = 48.0f;
  font_draw_text(renderer, title,
                 (WINDOW_WIDTH - font_text_width(title, size)) / 2.0f,
                 WINDOW_HEIGHT * 0.35f, size,
                 (SDL_Color){ 255, 90, 80, 255 });

  // Blinking prompt
  if (fmodf(SDL_GetTicks() / 1000.0f, 1.2f) < 0.8f) {
    const char *prompt = "PRESS ANY KEY TO RESTART";
    size = 12.0f;
    font_draw_text(renderer, prompt,
                   (WINDOW_WIDTH - font_text_width(prompt, size)) / 2.0f,
                   WINDOW_HEIGHT * 0.35f + 80.0f, size,
                   (SDL_Color){ 180, 200, 255, 255 });
  }
}

/** One rendered frame: events, fixed-step simulation, then drawing. */
static void main_loop(void *arg) {
  (void)arg;
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      app.running = false;
      break;
    case SDL_EVENT_KEY_DOWN:
      // Discrete actions only: auto-repeat spams menus and purchases
      // (held-key gameplay polls key state and is unaffected)
      if (event.key.repeat) break;
      // Dev: F9 records a fresh run, F10 replays the last recording
      if (event.key.scancode == SDL_SCANCODE_F9) {
        if (replay_mode() == REPLAY_RECORD) {
          replay_stop();
        } else {
          replay_stop();
          game_reset(replay_begin_record());
          dialogue_run_start(SHIP_START_DEBT);
        }
        break;
      }
      if (event.key.scancode == SDL_SCANCODE_F10) {
        if (replay_mode() == REPLAY_PLAYBACK) {
          replay_stop();
        } else {
          replay_stop();
          Uint64 replay_seed;
          if (replay_begin_playback(&replay_seed)) game_reset(replay_seed);
        }
        break;
      }

      switch (app.state) {
      case STATE_MENU: {
        MenuResult result = menu_handle_key(event.key.scancode);
        if (result == MENU_START) {
          replay_stop();
          game_reset(random_seed());
          dialogue_run_start(SHIP_START_DEBT);
        } else if (result == MENU_CONTINUE) {
          replay_stop();
          if (!save_read(&app)) game_reset(random_seed());
        } else if (result == MENU_QUIT) app.running = false;
        break;
      }
      case STATE_PLAYING:
        if (event.key.scancode == SDL_SCANCODE_ESCAPE && !app.quest_board) {
          app.state = STATE_PAUSED;
          if (replay_mode() != REPLAY_PLAYBACK) {
            save_write(&app);  // pausing is a checkpoint
          }
          break;
        }
        if (event.key.scancode == SDL_SCANCODE_M) {
          muted = !muted;
          audio_set_muted(muted);
          music_set_muted(muted);
          break;
        }
        if (replay_mode() == REPLAY_PLAYBACK) break;  // tape drives the run

        if (app.quest_board) {
          // Quest board mode: numbers pick, X abandons, B/Esc closes
          if (event.key.scancode == SDL_SCANCODE_ESCAPE ||
              event.key.scancode == SDL_SCANCODE_B) {
            app.quest_board = false;
          } else if (event.key.scancode == SDL_SCANCODE_X) {
            quest_abandon(&app.quest, &app.world);
            replay_record_action(RA_QUEST_ABANDON, 0);
          } else if (event.key.scancode >= SDL_SCANCODE_1 &&
                     event.key.scancode <= SDL_SCANCODE_9) {
            int idx = event.key.scancode - SDL_SCANCODE_1;
            replay_record_action(RA_QUEST_ACCEPT, idx);
            if (quest_try_accept(&app.quest, &app.world, app.player, idx)) {
              app.quest_board = false;
            }
          }
        } else if (event.key.scancode >= SDL_SCANCODE_1 &&
                   event.key.scancode <= SDL_SCANCODE_5) {
          int idx = event.key.scancode - SDL_SCANCODE_1;
          replay_record_action(RA_BUY_UPGRADE, idx);
          station_try_buy(&app.world, app.player, (UpgradeId)idx);
        } else if (event.key.scancode == SDL_SCANCODE_6) {
          replay_record_action(RA_BUY_SHIELD, 0);
          station_try_buy_shield(&app.world, app.player);
        } else if (event.key.scancode == SDL_SCANCODE_7) {
          replay_record_action(RA_BUY_MINE, 0);
          station_try_buy_mine(&app.world, app.player);
        } else if (event.key.scancode == SDL_SCANCODE_8) {
          replay_record_action(RA_BUY_MISSILE, 0);
          station_try_buy_missile(&app.world, app.player);
        } else if (event.key.scancode == SDL_SCANCODE_Q) {
          if (station_docked(&app.world, app.player)) {
            app.quest_board = true;
          }
        }
        break;
      case STATE_PAUSED:
        if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
          app.state = STATE_PLAYING;
        } else if (event.key.scancode == SDL_SCANCODE_M) {
          menu_open();
          app.state = STATE_MENU;
        } else if (event.key.scancode == SDL_SCANCODE_Q) {
#ifdef __EMSCRIPTEN__
          menu_open();  // can't quit a browser tab; the menu is "out"
          app.state = STATE_MENU;
#else
          app.running = false;
#endif
        }
        break;
      case STATE_GAME_OVER:
        if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
          menu_open();
          app.state = STATE_MENU;
        } else {
          replay_stop();
          game_reset(random_seed());  // any other key restarts
          dialogue_run_start(SHIP_START_DEBT);
        }
        break;
      }
      break;
    default:
      break;
    }
  }

  static Uint64 last_ticks = 0;
  if (last_ticks == 0) last_ticks = SDL_GetTicks();
  Uint64 now = SDL_GetTicks();
  float dt = (now - last_ticks) / 1000.0f;
  last_ticks = now;

  // Clamp dt to prevent tunneling on slow frames
  if (dt > 0.05f) dt = 0.05f;

  if (app.state == STATE_PLAYING || app.state == STATE_GAME_OVER) {
    sim_accumulator += dt;
    while (sim_accumulator >= SIM_DT) {
      sim_accumulator -= SIM_DT;
      simulate(SIM_DT);
    }

    events_drain();
  }

  // Render interpolation: fraction of a sim step since the last one
  float alpha = sim_accumulator / SIM_DT;

  // Screen shake rides on top of the camera (frozen while paused/menu)
  Vec2f shake = vec2f_new(0.0f, 0.0f);
  if (app.state == STATE_PLAYING || app.state == STATE_GAME_OVER) {
    shake = fx_shake_offset(dt);
  }

  // The camera follows the *interpolated* player so the world glides
  Vec2f cam_base = app.camera;
  if (entity_has(&app.world, app.player, C_PLAYER)) {
    cam_base = render_transform(&app.world, app.player, alpha).position;
  }
  Vec2f cam = vec2f_add(cam_base, shake);

  Vec2f view_offset = vec2f_new(
                                WINDOW_WIDTH / 2.0f - cam.x,
                                WINDOW_HEIGHT / 2.0f - cam.y
                                );

  bool alive = entity_has(&app.world, app.player, C_PLAYER);
  bool thrusting = app.state == STATE_PLAYING && alive &&
                   app.world.players[app.player].thrusting;
  float pitch = 1.0f;
  if (alive) {
    float speed = vec2f_length(app.world.velocities[app.player].value);
    float frac = speed / SHIP_MAX_SPEED;
    if (frac > 1.0f) frac = 1.0f;
    pitch = 1.0f + 0.35f * frac;
  }
  audio_thrust(thrusting, pitch);
  audio_update();
  music_update();
  dialogue_update(dt);

  SDL_SetRenderTarget(app.renderer, app.scene);
  SDL_SetRenderScale(app.renderer, SCENE_SS, SCENE_SS);
  SDL_SetRenderDrawColor(app.renderer, 8, 8, 16, 255); // Near-black with blue tint
  SDL_RenderClear(app.renderer);

  starfield_render(&app.starfield, app.renderer, cam);

  // Bloom: glow sources render at logical scale, come back blurred + additive
  SDL_SetRenderScale(app.renderer, 1.0f, 1.0f);
  glow_begin(&app.glow, app.renderer);
  system_render_glow(&app.world, app.renderer, view_offset, alpha);
  glow_end(&app.glow, app.renderer, app.scene);
  SDL_SetRenderScale(app.renderer, SCENE_SS, SCENE_SS);

  system_render(&app.world, app.renderer, view_offset, alpha);
  if (app.state != STATE_MENU) {
    hud_render(&app.world, app.player, &app.quest, &app.distress,
               app.quest_board, app.renderer);
    dialogue_render(app.renderer);
  }

  if (app.state == STATE_MENU) {
    menu_render(app.renderer);
  } else if (app.state == STATE_PAUSED) {
    pause_render(app.renderer);
  } else if (app.state == STATE_GAME_OVER) {
    game_over_render(app.renderer);
  }

  if (muted) {
    font_draw_text(app.renderer, "MUTED", WINDOW_WIDTH - 70.0f,
                   WINDOW_HEIGHT - 24.0f, 10.0f,
                   (SDL_Color){ 110, 120, 140, 255 });
  }
  if (replay_mode() == REPLAY_RECORD) {
    font_draw_text(app.renderer, "REC", WINDOW_WIDTH / 2.0f - 14.0f, 12.0f,
                   12.0f, (SDL_Color){ 255, 90, 80, 255 });
  } else if (replay_mode() == REPLAY_PLAYBACK) {
    font_draw_text(app.renderer, "REPLAY", WINDOW_WIDTH / 2.0f - 27.0f, 12.0f,
                   12.0f, (SDL_Color){ 230, 120, 255, 255 });
  }

  SDL_SetRenderScale(app.renderer, 1.0f, 1.0f);
  SDL_SetRenderTarget(app.renderer, NULL);
  SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
  SDL_RenderClear(app.renderer);
  SDL_RenderTexture(app.renderer, app.scene, NULL, NULL);

  SDL_RenderPresent(app.renderer);

#ifdef __EMSCRIPTEN__
  if (!app.running) {
    emscripten_cancel_main_loop();
  }
#endif
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("Failed to initialize SDL3: %s", SDL_GetError());
    return 1;
  }

  if (!audio_init()) {
    SDL_Log("Audio unavailable, running silent: %s", SDL_GetError());
  }
  music_init();

  // The window opens at the display size the game actually occupies —
  // on web, the canvas's CSS size — and HIGH_PIXEL_DENSITY renders at
  // native resolution. Logical presentation keeps game coordinates at
  // 1280x720 regardless, so lines stay crisp at any scale.
  int win_w = WINDOW_WIDTH, win_h = WINDOW_HEIGHT;
#ifdef __EMSCRIPTEN__
  double css_w = 0.0, css_h = 0.0;
  emscripten_get_element_css_size("#canvas", &css_w, &css_h);
  if (css_w > 0.0 && css_h > 0.0) {
    win_w = (int)css_w;
    win_h = (int)css_h;
  }
#endif

  app.window = SDL_CreateWindow(
                                "Space Dregs",
                                win_w,
                                win_h,
                                SDL_WINDOW_HIGH_PIXEL_DENSITY
                                );

  if (!app.window) {
    SDL_Log("Failed to create window: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  app.renderer = SDL_CreateRenderer(app.window, NULL);
  if (!app.renderer) {
    SDL_Log("Failed to create renderer: %s", SDL_GetError());
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 1;
  }

  // Glow passes rely on alpha blending
  SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

  // Fixed logical resolution: fullscreen scales and letterboxes cleanly
  SDL_SetRenderLogicalPresentation(app.renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  app.scene = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_TARGET,
                                WINDOW_WIDTH * SCENE_SS,
                                WINDOW_HEIGHT * SCENE_SS);
  if (!app.scene) {
    SDL_Log("Failed to create scene target: %s", SDL_GetError());
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 1;
  }
  SDL_SetTextureScaleMode(app.scene, SDL_SCALEMODE_LINEAR);

  if (!glow_init(&app.glow, app.renderer, WINDOW_WIDTH, WINDOW_HEIGHT)) {
    SDL_Log("Failed to create glow targets: %s", SDL_GetError());
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 1;
  }

  input_init();
  dialogue_init();
  config_init(app.window);  // after input/audio/music: load applies to them
  menu_init(app.window);
  starfield_init(&app.starfield, WINDOW_WIDTH, WINDOW_HEIGHT);
  game_reset(random_seed());
  menu_open();
  app.state = STATE_MENU;  // boot to the title over the drifting world
  app.running = true;

#ifdef __EMSCRIPTEN__
  // Emscripten requires callback-based loop (browser controls the frame rate)
  emscripten_set_main_loop_arg(main_loop, NULL, 0, 1);
#else
  while (app.running) {
    main_loop(NULL);
  }
#endif

  SDL_DestroyTexture(app.scene);
  bool was_playback = replay_mode() == REPLAY_PLAYBACK;
  replay_stop();  // an in-progress recording is written out
  if ((app.state == STATE_PLAYING || app.state == STATE_PAUSED) &&
      !was_playback) {
    save_write(&app);  // quitting mid-run keeps the run
  }
  config_save();
  music_quit();
  audio_quit();
  glow_destroy(&app.glow);
  SDL_DestroyRenderer(app.renderer);
  SDL_DestroyWindow(app.window);
  SDL_Quit();

  return 0;
}
