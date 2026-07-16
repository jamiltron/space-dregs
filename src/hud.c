// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include "hud.h"
#include "draw.h"
#include "faction.h"
#include "app.h"
#include "font.h"
#include "ship.h"
#include "station.h"

#define HUD_MARGIN 16.0f
#define HUD_TEXT_SIZE 16.0f
#define HUD_LINE_GAP 26.0f
#define HUD_VALUE_RIGHT 216.0f  // stat values right-align to this column
#define HUD_LOW_HULL SHIP_LOW_HULL       // red zones match the warning beeps
#define HUD_LOW_FUEL SHIP_LOW_FUEL_FRAC

static const SDL_Color COLOR_SHIP   = { 180, 200, 255, 255 };
static const SDL_Color COLOR_DANGER = { 255, 90, 80, 255 };
static const SDL_Color COLOR_GOLD   = { 255, 200, 80, 255 };
static const SDL_Color COLOR_FUEL   = { 150, 220, 150, 255 };
static const SDL_Color COLOR_DOCK   = { 80, 220, 230, 255 };
static const SDL_Color COLOR_DIM    = { 110, 120, 140, 255 };
static const SDL_Color COLOR_QUEST  = { 230, 120, 255, 255 };
static const SDL_Color COLOR_SHIELD = { 130, 200, 255, 255 };
static const SDL_Color COLOR_MINE = { 255, 110, 95, 255 };
static const SDL_Color COLOR_MISSILE = { 255, 200, 120, 255 };
static const SDL_Color COLOR_DISTRESS = { 255, 180, 60, 255 };

#define COMPASS_EDGE_INSET 48.0f
#define COMPASS_TARGET_EXTENT 100.0f  /**< Target size + slack for the hide test. */

/** Arrow at the screen edge pointing along delta, with a rough
 *  distance readout. Hidden while the target is on screen. */
static void compass_arrow(SDL_Renderer *renderer, Vec2f delta,
                          SDL_Color color) {
  float dist = vec2f_length(delta);
  if (dist <= 0.0f) return;

  // The camera is centered on the player, so the target is visible
  // when its center is within half a screen (plus some slack)
  if (fabsf(delta.x) < WINDOW_WIDTH / 2.0f + COMPASS_TARGET_EXTENT &&
      fabsf(delta.y) < WINDOW_HEIGHT / 2.0f + COMPASS_TARGET_EXTENT) {
    return;
  }
  Vec2f center = vec2f_new(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);
  Vec2f dir = vec2f_mul(delta, 1.0f / dist);

  // Slide along dir until hitting the inset screen rectangle
  float half_w = center.x - COMPASS_EDGE_INSET;
  float half_h = center.y - COMPASS_EDGE_INSET;
  float tx = dir.x != 0.0f ? half_w / fabsf(dir.x) : 1e9f;
  float ty = dir.y != 0.0f ? half_h / fabsf(dir.y) : 1e9f;
  Vec2f pos = vec2f_add(center, vec2f_mul(dir, tx < ty ? tx : ty));

  // Arrow triangle aimed along dir
  float rad = atan2f(dir.x, -dir.y);
  static const Vec2f ARROW[3] = { { 0, -12 }, { -7, 7 }, { 7, 7 } };
  SDL_FPoint pts[4];
  for (int i = 0; i < 3; i++) {
    Vec2f p = vec2f_add(pos, vec2f_rotate(ARROW[i], rad));
    pts[i] = (SDL_FPoint){ .x = p.x, .y = p.y };
  }
  pts[3] = pts[0];

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
  draw_lines(renderer, pts, 4);

  // Distance readout, tucked toward screen center from the arrow
  char buf[16];
  SDL_snprintf(buf, sizeof(buf), "%d", (int)(dist / 10.0f));
  Vec2f text_pos = vec2f_add(pos, vec2f_mul(dir, -30.0f));
  font_draw_text(renderer, buf,
                 text_pos.x - font_text_width(buf, 10.0f) / 2.0f,
                 text_pos.y - 5.0f, 10.0f, color);
}

static void compass_render(World *world, Entity player, const Quest *quest,
                           const Distress *distress, SDL_Renderer *renderer) {
  Vec2f player_pos = world->transforms[player].position;

  Entity station = station_nearest(world, player_pos);
  if (station != MAX_ENTITIES) {
    compass_arrow(renderer,
                  vec2f_sub(world->transforms[station].position, player_pos),
                  COLOR_DOCK);
  }

  Vec2f quest_pos;
  if (quest_compass_target(quest, &quest_pos)) {
    compass_arrow(renderer, vec2f_sub(quest_pos, player_pos), COLOR_QUEST);
  }

  Vec2f distress_pos;
  if (distress_compass_target(distress, &distress_pos)) {
    compass_arrow(renderer, vec2f_sub(distress_pos, player_pos),
                  COLOR_DISTRESS);
  }
}

/** Docked quest board: offers to take, or the active contract to abandon. */
static void quest_board_render(World *world, const Quest *quest,
                               SDL_Renderer *renderer) {
  float x = WINDOW_WIDTH - 320.0f;
  float y = HUD_MARGIN;
  char buf[48];

  font_draw_text(renderer, "QUEST BOARD", x, y, HUD_TEXT_SIZE, COLOR_QUEST);
  y += HUD_LINE_GAP * 1.3f;

  if (quest->type != QUEST_NONE && quest->type != QUEST_TUTORIAL) {
    SDL_snprintf(buf, sizeof(buf), "%s +%d CR",
                 quest_status_label(quest), quest->reward);
    font_draw_text(renderer, buf, x, y, 12.0f, COLOR_QUEST);
    y += HUD_LINE_GAP * 0.9f;

    font_draw_text(renderer, "X - ABANDON CONTRACT", x, y, 12.0f, COLOR_DANGER);
    y += HUD_LINE_GAP * 0.9f;
  } else if (quest->offer_count == 0) {
    font_draw_text(renderer,
                   faction_board_open(world) ? "NO CONTRACTS TODAY"
                                             : "NO WORK FOR YOUR KIND",
                   x, y, 12.0f, COLOR_DIM);
    y += HUD_LINE_GAP * 0.9f;
  } else {
    for (int i = 0; i < quest->offer_count; i++) {
      SDL_snprintf(buf, sizeof(buf), "%d - %s +%d CR", i + 1,
                   quest_offer_label(quest->offers[i]),
                   quest->offer_rewards[i]);
      font_draw_text(renderer, buf, x, y, 12.0f, COLOR_QUEST);
      y += HUD_LINE_GAP * 0.9f;
    }
    if (quest->type == QUEST_TUTORIAL) {
      SDL_snprintf(buf, sizeof(buf), "ANY CONTRACT PAYS %d CR EXTRA",
                   QUEST_TUTORIAL_REWARD);
      font_draw_text(renderer, buf, x, y, 12.0f, COLOR_GOLD);
      y += HUD_LINE_GAP * 0.9f;
    }
  }

  font_draw_text(renderer, "B - BACK", x, y, 12.0f, COLOR_DIM);
}

/** Docked services panel: board pointer, paid services, upgrade shop. */
static void dock_panel_render(World *world, Entity player, const Quest *quest,
                              SDL_Renderer *renderer) {
  float x = WINDOW_WIDTH - 320.0f;
  float y = HUD_MARGIN;
  char buf[48];

  font_draw_text(renderer, "DOCKED", x, y, HUD_TEXT_SIZE, COLOR_DOCK);
  y += HUD_LINE_GAP * 1.3f;

  const Player *p = &world->players[player];

  font_draw_text(renderer, "Q - QUEST BOARD", x, y, 12.0f, COLOR_QUEST);
  y += HUD_LINE_GAP * 1.1f;

  // Paid services: lit while useful and affordable; rates move with
  // guild standing
  float scale = faction_price_scale(world);
  bool can_refuel = p->money > 0 && p->fuel < p->fuel_max;
  SDL_snprintf(buf, sizeof(buf), "HOLD F - FUEL %.0f PER CR",
               STATION_FUEL_PER_CREDIT / scale);
  font_draw_text(renderer, buf, x, y, 12.0f,
                 can_refuel ? COLOR_FUEL : COLOR_DIM);
  y += HUD_LINE_GAP * 0.8f;

  int hull_rate = (int)SDL_roundf((float)STATION_HULL_PER_CREDIT / scale);
  bool can_repair = p->money > 0 && p->hp < p->max_hp;
  SDL_snprintf(buf, sizeof(buf), "HOLD R - HULL %d PER CR",
               hull_rate > 0 ? hull_rate : 1);
  font_draw_text(renderer, buf, x, y, 12.0f,
                 can_repair ? COLOR_SHIP : COLOR_DIM);
  y += HUD_LINE_GAP * 0.8f;

  bool can_pay = p->money > 0 && p->debt > 0;
  font_draw_text(renderer, "HOLD P - PAY DEBT", x, y, 12.0f,
                 can_pay ? (SDL_Color){ 230, 150, 90, 255 } : COLOR_DIM);
  y += HUD_LINE_GAP * 0.9f;

  SDL_snprintf(buf, sizeof(buf), "GUILD %+d - CLANS %+d",
               world->factions.standing[FACTION_GUILD],
               world->factions.standing[FACTION_CLANS]);
  font_draw_text(renderer, buf, x, y, 12.0f, COLOR_DIM);
  y += HUD_LINE_GAP * 1.1f;

  // Special equipment appears only when this dock's stock roll hit;
  // bounties completed raise the odds (see station_update)
  if (p->shield_max == 0 && station_shield_in_stock()) {
    int shield_cost = station_price(world, STATION_SHIELD_PRICE);
    SDL_snprintf(buf, sizeof(buf), "6 - ENERGY SHIELD - %d", shield_cost);
    font_draw_text(renderer, buf, x, y, 12.0f,
                   p->money >= shield_cost ? COLOR_SHIELD : COLOR_DIM);
    y += HUD_LINE_GAP * 1.1f;
  }
  if (p->mines_max == 0 && station_mine_in_stock()) {
    int rack_cost = station_price(world, STATION_MINE_RACK_PRICE);
    SDL_snprintf(buf, sizeof(buf), "7 - MINE RACK - %d", rack_cost);
    font_draw_text(renderer, buf, x, y, 12.0f,
                   p->money >= rack_cost ? COLOR_MINE : COLOR_DIM);
    y += HUD_LINE_GAP * 1.1f;
  } else if (p->mines_max > 0 && p->mines < p->mines_max) {
    int mine_cost = station_price(world, STATION_MINE_PRICE);
    SDL_snprintf(buf, sizeof(buf), "7 - MINE - %d", mine_cost);
    font_draw_text(renderer, buf, x, y, 12.0f,
                   p->money >= mine_cost ? COLOR_MINE : COLOR_DIM);
    y += HUD_LINE_GAP * 1.1f;
  }
  if (p->missiles_max == 0 && station_missile_in_stock()) {
    int pod_cost = station_price(world, STATION_MISSILE_POD_PRICE);
    SDL_snprintf(buf, sizeof(buf), "8 - MISSILE POD - %d", pod_cost);
    font_draw_text(renderer, buf, x, y, 12.0f,
                   p->money >= pod_cost ? COLOR_MISSILE : COLOR_DIM);
    y += HUD_LINE_GAP * 1.1f;
  } else if (p->missiles_max > 0 && p->missiles < p->missiles_max) {
    int missile_cost = station_price(world, STATION_MISSILE_PRICE);
    SDL_snprintf(buf, sizeof(buf), "8 - MISSILE - %d", missile_cost);
    font_draw_text(renderer, buf, x, y, 12.0f,
                   p->money >= missile_cost ? COLOR_MISSILE : COLOR_DIM);
    y += HUD_LINE_GAP * 1.1f;
  }
  for (int i = 0; i < UPGRADE_COUNT; i++) {
    int cost = station_upgrade_cost(world, player, (UpgradeId)i);
    SDL_snprintf(buf, sizeof(buf), "%d %s LVL %d - %d",
                 i + 1, STATION_UPGRADES[i].name, p->upgrades[i], cost);
    font_draw_text(renderer, buf, x, y, 12.0f,
                   p->money >= cost ? COLOR_SHIP : COLOR_DIM);
    y += HUD_LINE_GAP * 0.8f;
  }
}

/** Label on the left, value right-aligned to a shared column. */
static void stat_line(SDL_Renderer *renderer, const char *label,
                      const char *value, float y, SDL_Color color) {
  font_draw_text(renderer, label, HUD_MARGIN, y, HUD_TEXT_SIZE, color);
  font_draw_text(renderer, value,
                 HUD_MARGIN + HUD_VALUE_RIGHT -
                     font_text_width(value, HUD_TEXT_SIZE),
                 y, HUD_TEXT_SIZE, color);
}

void hud_render(World *world, Entity player, const Quest *quest,
                const Distress *distress, bool quest_board,
                SDL_Renderer *renderer) {
  if (!entity_has(world, player, C_PLAYER)) return;

  const Player *p = &world->players[player];
  char buf[48];
  float y = HUD_MARGIN;

  SDL_snprintf(buf, sizeof(buf), "%d/%d", p->hp > 0 ? p->hp : 0, p->max_hp);
  stat_line(renderer, "HULL", buf, y,
            p->hp <= HUD_LOW_HULL ? COLOR_DANGER : COLOR_SHIP);
  y += HUD_LINE_GAP;

  if (p->shield_max > 0) {
    SDL_snprintf(buf, sizeof(buf), "%d/%d", p->shield, p->shield_max);
    stat_line(renderer, "SHIELD", buf, y, COLOR_SHIELD);
    y += HUD_LINE_GAP;
  }

  if (p->mines_max > 0) {
    SDL_snprintf(buf, sizeof(buf), "%d/%d", p->mines, p->mines_max);
    stat_line(renderer, "MINES", buf, y, COLOR_MINE);
    y += HUD_LINE_GAP;
  }

  if (p->missiles_max > 0) {
    SDL_snprintf(buf, sizeof(buf), "%d/%d", p->missiles, p->missiles_max);
    stat_line(renderer, "MISSILES", buf, y, COLOR_MISSILE);
    y += HUD_LINE_GAP;
  }

  SDL_snprintf(buf, sizeof(buf), "%d/%d", (int)p->fuel, (int)p->fuel_max);
  stat_line(renderer, "FUEL", buf, y,
            p->fuel <= p->fuel_max * HUD_LOW_FUEL ? COLOR_DANGER : COLOR_FUEL);
  y += HUD_LINE_GAP;

  SDL_snprintf(buf, sizeof(buf), "%d", p->scrap);
  stat_line(renderer, "SCRAP", buf, y, COLOR_GOLD);
  y += HUD_LINE_GAP;

  SDL_snprintf(buf, sizeof(buf), "%d", p->money);
  stat_line(renderer, "CREDITS", buf, y, COLOR_DOCK);
  y += HUD_LINE_GAP;

  if (p->debt > 0) {
    SDL_snprintf(buf, sizeof(buf), "%d", p->debt);
    stat_line(renderer, "DEBT", buf, y, (SDL_Color){ 230, 150, 90, 255 });
  } else {
    stat_line(renderer, "DEBT", "CLEAR", y, COLOR_GOLD);
  }
  y += HUD_LINE_GAP;

  if (world->factions.heat > 0.0f) {
    SDL_snprintf(buf, sizeof(buf), "%d", (int)SDL_ceilf(world->factions.heat));
    stat_line(renderer, "HEAT", buf, y,
              world->factions.heat >= FACTION_HEAT_HUNT ? COLOR_DANGER
                                                        : COLOR_DISTRESS);
    y += HUD_LINE_GAP;
  }

  if (quest->complete_timer > 0.0f) {
    SDL_snprintf(buf, sizeof(buf), "QUEST COMPLETE");
    font_draw_text(renderer, buf, HUD_MARGIN, y, 12.0f, COLOR_QUEST);
    y += HUD_LINE_GAP * 0.8f;
  } else if (quest->type != QUEST_NONE) {
    SDL_snprintf(buf, sizeof(buf), "%s +%d CR",
                 quest_status_label(quest), quest->reward);
    font_draw_text(renderer, buf, HUD_MARGIN, y, 12.0f, COLOR_QUEST);
    y += HUD_LINE_GAP * 0.8f;
  }

  if (distress->state == DISTRESS_CALLED) {
    SDL_snprintf(buf, sizeof(buf), "MAYDAY - HAULER HULL %d",
                 (int)SDL_ceilf(distress->hull));
    font_draw_text(renderer, buf, HUD_MARGIN, y, 12.0f, COLOR_DISTRESS);
    y += HUD_LINE_GAP * 0.8f;
  } else if (distress->state == DISTRESS_FIGHT) {
    SDL_snprintf(buf, sizeof(buf), "MAYDAY - RAIDERS LEFT %d",
                 distress->raiders);
    font_draw_text(renderer, buf, HUD_MARGIN, y, 12.0f, COLOR_DISTRESS);
    y += HUD_LINE_GAP * 0.8f;
  }

  for (int i = 0; i < FACTION_COUNT; i++) {
    if (world->factions.delta_timer[i] <= 0.0f) continue;
    SDL_snprintf(buf, sizeof(buf), "%s %+d",
                 i == FACTION_GUILD ? "GUILD" : "CLANS",
                 world->factions.delta[i]);
    SDL_Color up = i == FACTION_GUILD ? COLOR_DOCK : COLOR_DISTRESS;
    font_draw_text(renderer, buf, HUD_MARGIN, y, 12.0f,
                   world->factions.delta[i] >= 0 ? up : COLOR_DANGER);
    y += HUD_LINE_GAP * 0.8f;
  }

  compass_render(world, player, quest, distress, renderer);

  if (station_docked(world, player)) {
    if (quest_board) quest_board_render(world, quest, renderer);
    else dock_panel_render(world, player, quest, renderer);
  }
}
