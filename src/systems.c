// Copyright (C) 2026 Justin Hamilton
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <SDL3/SDL.h>
#include "systems.h"
#include "app.h"
#include "draw.h"
#include "asteroid.h"
#include "bullet.h"
#include "distress.h"
#include "events.h"
#include "faction.h"
#include "mine.h"
#include "missile.h"
#include "particles.h"
#include "pirate.h"
#include "replay.h"
#include "scrap.h"
#include "ship.h"
#include "signal.h"
#include "station.h"

/** First (only) player, or MAX_ENTITIES while the ship is gone. */
static Entity find_player(const World *world) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (entity_has(world, e, C_PLAYER | C_TRANSFORM)) return e;
  }
  return MAX_ENTITIES;
}

/** Far entities stop simulating; on wake they extrapolate
 *  position/angle by the frozen duration in one step, so the world
 *  appears to have kept drifting while unwatched. Lifetimes keep
 *  ticking elsewhere so frozen transients still expire. */
void system_freeze(World *world, Vec2f camera, float now) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_TRANSFORM)) continue;
    if (entity_has(world, e, C_PLAYER)) continue;  // never freeze the player
    // Distress scenes keep simulating so the raid is already underway
    // when the player closes in
    if (entity_has(world, e, C_DISTRESS)) continue;

    Transform *tf = &world->transforms[e];
    bool far = fabsf(tf->position.x - camera.x) > FREEZE_RADIUS ||
               fabsf(tf->position.y - camera.y) > FREEZE_RADIUS;
    bool frozen = entity_has(world, e, C_FROZEN);

    if (far && !frozen) {
      world->masks[e] |= C_FROZEN;
      world->frozens[e].since = now;
    } else if (!far && frozen) {
      world->masks[e] &= ~C_FROZEN;

      // Catch up: dead-reckon the missed time (no collisions). Damping
      // must be honored — its closed form coasts at most v/k — or a
      // long-frozen ship teleports by v*t on wake.
      if (entity_has(world, e, C_VELOCITY)) {
        float elapsed = now - world->frozens[e].since;
        Velocity *vel = &world->velocities[e];
        Vec2f target = tf->position;
        if (vel->damping > 0.0f) {
          float decay = expf(-vel->damping * elapsed);
          target = vec2f_add(
              target, vec2f_mul(vel->value, (1.0f - decay) / vel->damping));
          vel->value = vec2f_mul(vel->value, decay);
        } else {
          target = vec2f_add(target, vec2f_mul(vel->value, elapsed));
        }
        // Waking happens off-screen; a catch-up jump that lands in the
        // view reads as a spawn. Hold position and drift in live instead.
        bool in_view =
            fabsf(target.x - camera.x) <
                WINDOW_WIDTH / 2.0f + FREEZE_WAKE_MARGIN &&
            fabsf(target.y - camera.y) <
                WINDOW_HEIGHT / 2.0f + FREEZE_WAKE_MARGIN;
        if (!in_view) tf->position = target;
        tf->angle += vel->spin * elapsed;
      }
    }
  }
}

void system_player(World *world, float dt) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_PLAYER | C_TRANSFORM | C_VELOCITY)) continue;

    Player    *player = &world->players[e];
    Transform *tf     = &world->transforms[e];
    Velocity  *vel    = &world->velocities[e];

    if (sim_input(ACT_LEFT)) {
      tf->angle -= player->rot_speed * dt;
    }
    if (sim_input(ACT_RIGHT)) {
      tf->angle += player->rot_speed * dt;
    }

    Vec2f dir = vec2f_dir(DEG_TO_RAD(tf->angle));

    player->thrusting = sim_input(ACT_THRUST) && player->fuel > 0.0f;

    if (player->thrusting) {
      player->fuel -= SHIP_FUEL_BURN * dt;
      if (player->fuel < 0.0f) player->fuel = 0.0f;

      vel->value = vec2f_add(vel->value, vec2f_mul(dir, player->thrust_force * dt));

      // Upgraded engines burn bigger: more motes, thrown harder
      float fire_scale = sqrtf(player->thrust_force / SHIP_THRUST_FORCE);

      player->exhaust_timer -= dt;
      while (player->exhaust_timer <= 0.0f) {
        player->exhaust_timer += 0.03f / fire_scale;

        Vec2f nozzle = vec2f_add(tf->position,
                                 vec2f_mul(dir, -player->exhaust_offset));
        Vec2f perp = vec2f_new(-dir.y, dir.x);
        Vec2f pvel = vec2f_add(
            vel->value,
            vec2f_mul(dir, -(110.0f + world_randf(world) * 70.0f) * fire_scale));
        pvel = vec2f_add(pvel,
                         vec2f_mul(perp, (world_randf(world) * 2.0f - 1.0f) * 35.0f));

        SDL_Color glow = world_randf(world) < 0.5f
                             ? (SDL_Color){ 255, 170, 60, 255 }
                             : (SDL_Color){ 255, 220, 120, 255 };
        exhaust_particle(world, nozzle, pvel, glow);
      }
    }

    player->damage_timer -= dt;

    if (player->shield_max > 0 && player->shield < player->shield_max) {
      player->shield_regen += dt;
      if (player->shield_regen >= SHIP_SHIELD_REGEN_SECS) {
        player->shield_regen -= SHIP_SHIELD_REGEN_SECS;
        player->shield++;
      }
    } else {
      player->shield_regen = 0.0f;
    }

    // Low-resource warnings: one beep on entering the red, another at
    // each SHIP_WARN_STEP lower. The armed level tracks partial
    // refuel/repair inside the red so a renewed drop beeps again.
    float red_fuel = player->fuel_max * SHIP_LOW_FUEL_FRAC;
    if (player->fuel > red_fuel) {
      player->fuel_warn_next = red_fuel;
    } else {
      if (player->fuel <= player->fuel_warn_next) {
        events_emit(EV_FUEL_LOW, tf->position);
      }
      while (player->fuel_warn_next >= player->fuel) {
        player->fuel_warn_next -= SHIP_WARN_STEP;
      }
      while (player->fuel_warn_next + SHIP_WARN_STEP < player->fuel &&
             player->fuel_warn_next + SHIP_WARN_STEP <= red_fuel) {
        player->fuel_warn_next += SHIP_WARN_STEP;
      }
    }

    if (player->hp > SHIP_LOW_HULL) {
      player->hull_warn_next = SHIP_LOW_HULL;
    } else if (player->hp > 0) {
      if (player->hp <= player->hull_warn_next) {
        events_emit(EV_HULL_LOW, tf->position);
      }
      while (player->hull_warn_next >= player->hp) {
        player->hull_warn_next -= (int)SHIP_WARN_STEP;
      }
      while (player->hull_warn_next + (int)SHIP_WARN_STEP < player->hp &&
             player->hull_warn_next + (int)SHIP_WARN_STEP <= SHIP_LOW_HULL) {
        player->hull_warn_next += (int)SHIP_WARN_STEP;
      }
    }

    player->fire_cooldown -= dt;

    if (sim_input(ACT_FIRE) && player->fire_cooldown <= 0.0f) {
      player->fire_cooldown = player->fire_interval;

      Vec2f muzzle = vec2f_add(tf->position,
                               vec2f_mul(dir, player->muzzle_offset));
      Vec2f bullet_vel = vec2f_add(vel->value, vec2f_mul(dir, BULLET_SPEED));
      bullet_spawn(world, muzzle, bullet_vel, tf->angle, false);
      events_emit(EV_PLAYER_FIRED, muzzle);
    }

    bool mine_key = sim_input(ACT_MINE);
    if (mine_key && !player->mine_latch && player->mines > 0 &&
        !station_docked(world, e)) {
      Vec2f stern = vec2f_sub(tf->position, vec2f_mul(dir, MINE_DROP_OFFSET));
      mine_spawn(world, stern, vel->value);
      player->mines--;
      events_emit(EV_MINE_DROPPED, stern);
    }
    player->mine_latch = mine_key;

    bool missile_key = sim_input(ACT_MISSILE);
    if (missile_key && !player->missile_latch && player->missiles > 0 &&
        !station_docked(world, e)) {
      Vec2f nose = vec2f_add(tf->position,
                             vec2f_mul(dir, player->muzzle_offset));
      missile_spawn(world, nose, tf->angle, false);
      player->missiles--;
      events_emit(EV_MISSILE_FIRED, nose);
    }
    player->missile_latch = missile_key;
  }
}

/** Wander until the player enters the sense radius, then turn in,
 *  close to a standoff distance, and fire when lined up. Station
 *  zones are fled; loose scrap gets looted while idle. */
void system_pirate(World *world, float dt) {
  Entity player = find_player(world);

  // Station zones are sanctuary: pirates flee them and won't engage
  // a player sheltering inside one.
  bool player_in_sanctuary = false;
  if (player != MAX_ENTITIES) {
    Vec2f ppos = world->transforms[player].position;
    Entity near_station = station_nearest(world, ppos);
    if (near_station != MAX_ENTITIES) {
      Vec2f d = vec2f_sub(world->transforms[near_station].position, ppos);
      player_in_sanctuary = vec2f_length(d) < STATION_SAFE_ZONE;
    }
  }

  // The live distress hauler, if any: tagged raiders converge on it
  Entity hauler = MAX_ENTITIES;
  for (Entity e = 0; e < world->high_water; e++) {
    if (entity_has(world, e, C_FREIGHTER | C_DISTRESS | C_TRANSFORM)) {
      hauler = e;
      break;
    }
  }

  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_PIRATE | C_TRANSFORM | C_VELOCITY)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;

    Pirate    *pirate = &world->pirates[e];
    Transform *tf     = &world->transforms[e];
    Velocity  *vel    = &world->velocities[e];
    const PirateStats *st = pirate_stats(pirate->archetype);

    pirate->fire_cooldown -= dt;

    // Hoarded loot drips back out into the field, dropped outside the
    // pirate's own magnet so it doesn't just re-collect it
    if (pirate->loot > 0) {
      pirate->leak_timer += dt;
      if (pirate->leak_timer >= PIRATE_LOOT_LEAK_SECS) {
        pirate->leak_timer = 0.0f;
        pirate->loot--;
        Vec2f back = vec2f_dir(DEG_TO_RAD(tf->angle + 180.0f));
        Vec2f drop = vec2f_add(tf->position,
                               vec2f_mul(back, PIRATE_MAGNET_RADIUS + 24.0f));
        scrap_scatter(world, drop, vec2f_mul(back, 30.0f), 1);
      }
    } else {
      pirate->leak_timer = 0.0f;
    }

    Vec2f to_player = { 0 };
    float dist = 0.0f;
    bool engaged = false;
    if (player != MAX_ENTITIES && !player_in_sanctuary) {
      to_player = vec2f_sub(world->transforms[player].position, tf->position);
      dist = vec2f_length(to_player);

      // Provoked or tagged pirates ignore the clan-standing perk
      float sense = st->sense_radius;
      if (!pirate->provoked && !entity_has(world, e, C_DISTRESS)) {
        sense *= faction_sense_scale(world);
      }
      engaged = dist < sense;
    }

    if (st->kamikaze) {
      for (Entity m = 0; m < world->high_water; m++) {
        if (!entity_has(world, m, C_MINE | C_TRANSFORM)) continue;
        if (entity_has(world, m, C_FROZEN)) continue;

        Vec2f d = vec2f_sub(world->transforms[m].position, tf->position);
        float md = vec2f_length(d);
        if (md < st->sense_radius && (!engaged || md < dist)) {
          to_player = d;
          dist = md;
          engaged = true;
        }
      }
    }

    bool fleeing_station = false;
    Vec2f flee_from = { 0 };
    Entity my_station = station_nearest(world, tf->position);
    if (my_station != MAX_ENTITIES) {
      flee_from = world->transforms[my_station].position;
      if (vec2f_length(vec2f_sub(tf->position, flee_from)) < STATION_SAFE_ZONE) {
        fleeing_station = true;
        engaged = false;
      }
    }

    // Tagged raiders converge on the hauler when one lives; an ambush
    // pack (no hauler) targets the player regardless of sense range
    bool hunting = false;
    Vec2f to_hauler = { 0 };
    float hauler_dist = 0.0f;
    if (!fleeing_station && !engaged && entity_has(world, e, C_DISTRESS)) {
      if (hauler != MAX_ENTITIES && !st->kamikaze) {
        to_hauler = vec2f_sub(world->transforms[hauler].position,
                              tf->position);
        hauler_dist = vec2f_length(to_hauler);
        hunting = hauler_dist > 0.0f;
      } else if (hauler == MAX_ENTITIES && player != MAX_ENTITIES &&
                 !player_in_sanctuary) {
        engaged = true;
      }
    }

    if (!fleeing_station && !engaged && entity_has(world, e, C_HUNTER) &&
        player != MAX_ENTITIES && !player_in_sanctuary) {
      engaged = true;
    }

    Entity prize = MAX_ENTITIES;
    Vec2f to_prize = { 0 };
    if (!fleeing_station && !engaged && !hunting && !st->kamikaze) {
      float best = PIRATE_SCRAP_RADIUS;
      for (Entity s = 0; s < world->high_water; s++) {
        if (!entity_has(world, s, C_SCRAP | C_TRANSFORM)) continue;
        if (entity_has(world, s, C_FROZEN)) continue;

        Vec2f d = vec2f_sub(world->transforms[s].position, tf->position);
        float sd = vec2f_length(d);
        if (sd < best) {
          best = sd;
          prize = s;
          to_prize = d;
        }
      }

      if (prize != MAX_ENTITIES && best < PIRATE_SCRAP_PICKUP) {
        pirate->loot++;
        entity_destroy(world, prize);
        prize = MAX_ENTITIES;
      }
    }

    float target_angle;
    if (fleeing_station) {
      target_angle = vec2f_heading(vec2f_sub(tf->position, flee_from));
    } else if (engaged) {
      target_angle = vec2f_heading(to_player);
    } else if (hunting) {
      target_angle = vec2f_heading(to_hauler);
    } else if (prize != MAX_ENTITIES) {
      target_angle = vec2f_heading(to_prize);
    } else if (st->kamikaze) {
      target_angle = tf->angle;  // dormant: hold position
    } else {
      pirate->wander_timer -= dt;
      if (pirate->wander_timer <= 0.0f) {
        pirate->wander_timer = 2.0f + world_randf(world) * 3.0f;
        pirate->wander_dir = world_randf(world) * 360.0f;
      }
      target_angle = pirate->wander_dir;
    }

    float heading_error =
        fmodf(target_angle - tf->angle + 540.0f, 360.0f) - 180.0f;
    float max_turn = st->rot_speed * dt;
    float turn = heading_error;
    if (turn > max_turn) turn = max_turn;
    if (turn < -max_turn) turn = -max_turn;
    tf->angle += turn;

    Vec2f dir = vec2f_dir(DEG_TO_RAD(tf->angle));

    //     Kamikazes only move on an engaged player, and never stand off ---
    bool facing = fabsf(heading_error) < 60.0f;
    bool wants_thrust;
    if (st->kamikaze) {
      wants_thrust = engaged || fleeing_station;
    } else if (hunting) {
      // Wide standoff: coasting past it must not plow into the hauler
      wants_thrust = hauler_dist > PIRATE_KEEP_DIST * 1.5f;
    } else {
      wants_thrust = !engaged || dist > PIRATE_KEEP_DIST;
    }
    if (facing && wants_thrust) {
      vel->value = vec2f_add(vel->value, vec2f_mul(dir, st->thrust * dt));
    }

    // Single-gun ships hold fire until lined up; a multi-mount volley
    // covers its bearings regardless of aim
    if (!st->kamikaze && engaged && dist < PIRATE_FIRE_RANGE &&
        pirate->fire_cooldown <= 0.0f &&
        (st->gun_count > 1 || fabsf(heading_error) < 12.0f)) {
      pirate->fire_cooldown = st->fire_interval;

      int guns = st->gun_count > 0 ? st->gun_count : 1;
      for (int g = 0; g < guns; g++) {
        float bearing = tf->angle + st->gun_bearings[g];
        Vec2f gdir = vec2f_dir(DEG_TO_RAD(bearing));
        Vec2f muzzle = vec2f_add(tf->position,
                                 vec2f_mul(gdir, st->size + 4.0f));
        Vec2f bullet_vel = vec2f_add(
            vel->value,
            vec2f_mul(gdir, BULLET_SPEED * PIRATE_BULLET_SPEED_SCALE));
        Entity b = bullet_spawn(world, muzzle, bullet_vel, bearing, true);
        if (st->laser) {
          world->wireframes[b].color = (SDL_Color){ 130, 255, 120, 255 };
          world->wireframes[b].glow_color = (SDL_Color){ 90, 255, 90, 255 };
        }
      }
      events_emit(EV_PIRATE_FIRED, tf->position);  // one report per volley
    }

    if (st->missile_interval > 0.0f) {
      pirate->missile_cooldown -= dt;
      // The seeker corrects the aim; only a rough nose-on is needed
      if (engaged && fabsf(heading_error) < 90.0f &&
          pirate->missile_cooldown <= 0.0f) {
        pirate->missile_cooldown = st->missile_interval;

        Vec2f nose = vec2f_add(tf->position, vec2f_mul(dir, st->size + 6.0f));
        missile_spawn(world, nose, tf->angle, true);
        events_emit(EV_MISSILE_FIRED, nose);
      }
    }

    if (st->deploy_interval > 0.0f) {
      pirate->deploy_cooldown -= dt;
      if (engaged && pirate->deploy_cooldown <= 0.0f) {
        pirate->deploy_cooldown = st->deploy_interval;

        // The bay holds launch while enough drones already swarm nearby
        int nearby = 0;
        for (Entity d2 = 0; d2 < world->high_water; d2++) {
          if (!entity_has(world, d2, C_PIRATE | C_TRANSFORM)) continue;
          if (world->pirates[d2].archetype != PIRATE_DRONE) continue;
          Vec2f dd = vec2f_sub(world->transforms[d2].position, tf->position);
          if (vec2f_length(dd) < PIRATE_DEPLOY_RADIUS) nearby++;
        }
        if (nearby < PIRATE_DEPLOY_CAP) {
          Vec2f bay = vec2f_sub(tf->position, vec2f_mul(dir, st->size + 8.0f));
          Entity d = pirate_spawn(world, bay, PIRATE_DRONE);
          world->velocities[d].value =
              vec2f_add(vel->value, vec2f_mul(dir, -120.0f));
          events_emit(EV_DRONE_DEPLOYED, bay);
        }
      }
    }
  }
}

void system_scrap(World *world, float dt) {
  Entity player = find_player(world);
  if (player == MAX_ENTITIES) return;

  Vec2f player_pos = world->transforms[player].position;
  float magnet_radius = world->players[player].magnet_radius;

  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_SCRAP | C_TRANSFORM | C_VELOCITY)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;

    Vec2f delta = vec2f_sub(player_pos, world->transforms[e].position);
    float dist = vec2f_length(delta);

    if (dist < SCRAP_PICKUP_DIST) {
      world->players[player].scrap++;
      events_emit(EV_SCRAP_PICKUP, world->transforms[e].position);
      entity_destroy(world, e);
      continue;
    }

    if (dist < magnet_radius) {
      // Pull ramps up as the scrap gets closer
      Vec2f dir = vec2f_mul(delta, 1.0f / dist);
      float pull = SCRAP_MAGNET_PULL * (1.2f - dist / magnet_radius);
      world->velocities[e].value =
          vec2f_add(world->velocities[e].value, vec2f_mul(dir, pull * dt));
    }

    // Pirates run a weaker magnet of their own (kamikazes don't loot)
    for (Entity p = 0; p < world->high_water; p++) {
      if (!entity_has(world, p, C_PIRATE | C_TRANSFORM)) continue;
      if (entity_has(world, p, C_FROZEN)) continue;
      if (pirate_stats(world->pirates[p].archetype)->kamikaze) continue;

      Vec2f pd = vec2f_sub(world->transforms[p].position,
                           world->transforms[e].position);
      float pdist = vec2f_length(pd);
      if (pdist >= PIRATE_MAGNET_RADIUS) continue;

      if (pdist < PIRATE_SCRAP_PICKUP) {
        world->pirates[p].loot++;
        entity_destroy(world, e);
        break;
      }

      Vec2f pdir = vec2f_mul(pd, 1.0f / pdist);
      float ppull = SCRAP_MAGNET_PULL * 0.6f *
                    (1.2f - pdist / PIRATE_MAGNET_RADIUS);
      world->velocities[e].value =
          vec2f_add(world->velocities[e].value, vec2f_mul(pdir, ppull * dt));
    }
  }
}

void system_mines(World *world, float dt) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_MINE)) continue;

    Mine *mine = &world->mines[e];
    mine->fuse -= dt;

    if (mine->fuse <= 0.0f) {
      mine_explode(world, e);
      continue;
    }

    // Blink faster as the fuse runs out
    if (mine->fuse < 1.2f && fmodf(mine->fuse, 0.3f) < 0.06f) {
      world->wireframes[e].flash = 0.05f;
    }
  }
}

/** Constant speed along the heading; the heading turns, rate-limited,
 *  toward the nearest target in seeker range. Player missiles seek
 *  pirates; hostile ones seek the player, but a closer mine decoys
 *  them (same trick the drones fall for). No target: straight. */
void system_missiles(World *world, float dt) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_MISSILE | C_TRANSFORM | C_VELOCITY)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;

    Transform *tf = &world->transforms[e];
    bool hostile = world->bullets[e].hostile;

    // Re-acquire every step: the nearest valid target wins the seeker
    float best = MISSILE_SEEK_RADIUS;
    Vec2f to_target = { 0 };
    bool locked = false;
    if (hostile) {
      Entity player = find_player(world);
      if (player != MAX_ENTITIES) {
        Vec2f d = vec2f_sub(world->transforms[player].position, tf->position);
        float pd = vec2f_length(d);
        if (pd < best) {
          best = pd;
          to_target = d;
          locked = true;
        }
      }
      for (Entity m = 0; m < world->high_water; m++) {
        if (!entity_has(world, m, C_MINE | C_TRANSFORM)) continue;
        if (entity_has(world, m, C_FROZEN)) continue;

        Vec2f d = vec2f_sub(world->transforms[m].position, tf->position);
        float md = vec2f_length(d);
        if (md < best) {
          best = md;
          to_target = d;
          locked = true;
        }
      }
    } else {
      for (Entity p = 0; p < world->high_water; p++) {
        if (!entity_has(world, p, C_PIRATE | C_TRANSFORM)) continue;
        if (entity_has(world, p, C_FROZEN)) continue;

        Vec2f d = vec2f_sub(world->transforms[p].position, tf->position);
        float pd = vec2f_length(d);
        if (pd < best) {
          best = pd;
          to_target = d;
          locked = true;
        }
      }
    }

    if (locked) {
      float heading_error = fmodf(vec2f_heading(to_target) - tf->angle +
                                      540.0f, 360.0f) - 180.0f;
      float max_turn = MISSILE_TURN_RATE * dt;
      if (heading_error > max_turn) heading_error = max_turn;
      if (heading_error < -max_turn) heading_error = -max_turn;
      tf->angle += heading_error;
    }

    Vec2f dir = vec2f_dir(DEG_TO_RAD(tf->angle));
    world->velocities[e].value = vec2f_mul(dir, MISSILE_SPEED);

    // Motor sparks off the tail
    if (world_randf(world) < 0.5f) {
      Vec2f tail = vec2f_sub(tf->position, vec2f_mul(dir, 7.0f));
      Vec2f pvel = vec2f_mul(dir, -60.0f);
      exhaust_particle(world, tail, pvel,
                       (SDL_Color){ 255, 190, 90, 255 });
    }
  }
}

/** Departing (untagged) haulers cruise away, veering around rocks,
 *  and despawn once safely past the screen edge. */
void system_freighters(World *world, Vec2f camera, float dt) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_FREIGHTER | C_TRANSFORM | C_VELOCITY))
      continue;
    if (entity_has(world, e, C_DISTRESS) || entity_has(world, e, C_FROZEN))
      continue;

    Transform *tf = &world->transforms[e];

    if (fabsf(tf->position.x - camera.x) >
            WINDOW_WIDTH / 2.0f + FREIGHTER_DESPAWN_MARGIN ||
        fabsf(tf->position.y - camera.y) >
            WINDOW_HEIGHT / 2.0f + FREIGHTER_DESPAWN_MARGIN) {
      entity_destroy(world, e);
      continue;
    }

    Vec2f fwd = vec2f_dir(DEG_TO_RAD(tf->angle));
    Vec2f steer = fwd;
    for (Entity r = 0; r < world->high_water; r++) {
      if (!entity_has(world, r, C_ASTEROID | C_TRANSFORM)) continue;

      Vec2f away = vec2f_sub(tf->position, world->transforms[r].position);
      float d = vec2f_length(away);
      if (d <= 0.0f || d >= FREIGHTER_AVOID_RADIUS) continue;
      steer = vec2f_add(steer, vec2f_mul(away, (1.0f - d / FREIGHTER_AVOID_RADIUS) *
                                                   (1.5f / d)));
    }

    float want = vec2f_heading(steer);
    float err = fmodf(want - tf->angle + 540.0f, 360.0f) - 180.0f;
    float max_turn = FREIGHTER_TURN * dt;
    if (err > max_turn) err = max_turn;
    if (err < -max_turn) err = -max_turn;
    tf->angle += err;

    world->velocities[e].value =
        vec2f_mul(vec2f_dir(DEG_TO_RAD(tf->angle)), FREIGHTER_FLEE_SPEED);
  }
}

void system_lifetime(World *world, float dt) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_LIFETIME)) continue;

    world->lifetimes[e].remaining -= dt;
    if (world->lifetimes[e].remaining <= 0.0f) {
      entity_destroy(world, e);
    }
  }
}

void system_movement(World *world, float dt) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_TRANSFORM | C_VELOCITY)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;

    Transform *tf  = &world->transforms[e];
    Velocity  *vel = &world->velocities[e];

    if (vel->damping > 0.0f) {
      vel->value = vec2f_mul(vel->value, expf(-vel->damping * dt));
    }

    float speed = vec2f_length(vel->value);
    if (vel->max_speed > 0.0f && speed > vel->max_speed) {
      vel->value = vec2f_mul(vel->value, vel->max_speed / speed);
    }

    tf->position = vec2f_add(tf->position, vec2f_mul(vel->value, dt));
    tf->angle += vel->spin * dt;

    if (entity_has(world, e, C_WIREFRAME) && world->wireframes[e].flash > 0.0f) {
      world->wireframes[e].flash -= dt;
    }
  }
}

void player_take_damage(Player *player, int dmg) {
  if (player->shield > 0) {
    int absorbed = dmg < player->shield ? dmg : player->shield;
    player->shield -= absorbed;
    dmg -= absorbed;
  }
  player->hp -= dmg;
}

// Collision tuning
#define RESTITUTION 0.85f
#define IMPACT_DAMAGE_MIN 70.0f    /**< Closing speed below this is harmless. */
#define IMPACT_DAMAGE_UNIT 110.0f  /**< Closing speed worth 1 hp at equal size. */

/** Pairwise circle vs circle: overlapping bodies are pushed apart and
 *  exchange an elastic impulse, with mass taken as radius^2 so small
 *  rocks glance off big ones. Bullets damage instead of bouncing.
 *  O(n^2) is fine at current active-entity counts. */
void system_collision(World *world) {
  const ComponentMask wanted = C_TRANSFORM | C_VELOCITY | C_COLLIDER;

  for (Entity a = 0; a < world->high_water; a++) {
    if (!entity_has(world, a, wanted)) continue;
    if (entity_has(world, a, C_FROZEN)) continue;

    for (Entity b = a + 1; b < world->high_water; b++) {
      if (!entity_has(world, a, wanted)) break;  // a may die mid-loop (bullet hit)
      if (!entity_has(world, b, wanted)) continue;
      if (entity_has(world, b, C_FROZEN)) continue;

      Transform *ta = &world->transforms[a];
      Transform *tb = &world->transforms[b];
      float ra = world->colliders[a].radius;
      float rb = world->colliders[b].radius;

      Vec2f delta = vec2f_sub(tb->position, ta->position);
      float dist = vec2f_length(delta);
      float overlap = (ra + rb) - dist;
      if (overlap <= 0.0f) continue;

      bool a_bullet = entity_has(world, a, C_BULLET);
      if (a_bullet || entity_has(world, b, C_BULLET)) {
        Entity bullet = a_bullet ? a : b;
        Entity other  = a_bullet ? b : a;
        if (entity_has(world, other, C_BULLET)) continue;  // tracers pass

        // Bullet position ≈ point of contact
        Vec2f contact = world->transforms[bullet].position;
        bool hostile = world->bullets[bullet].hostile;

        int punch = world->bullets[bullet].damage;
        if (entity_has(world, other, C_ASTEROID)) {
          asteroid_hit(world, other, punch, contact);
          entity_destroy(world, bullet);
        } else if (!hostile && entity_has(world, other, C_PIRATE)) {
          pirate_hit(world, other, punch, contact, true);
          entity_destroy(world, bullet);
        } else if (hostile && entity_has(world, other, C_PLAYER)) {
          Player *player = &world->players[other];
          if (player->damage_timer <= 0.0f) {
            // Missiles hit on the hull scale, harder than a bullet
            player_take_damage(player,
                               entity_has(world, bullet, C_MISSILE)
                                   ? MISSILE_HULL_DAMAGE
                                   : PIRATE_BULLET_DAMAGE);
            player->damage_timer = SHIP_DAMAGE_COOLDOWN;
            world->wireframes[other].flash = 0.1f;
            events_emit(EV_PLAYER_HURT, contact);
            if (player->hp <= 0) ship_explode(world, other);
          }
          entity_destroy(world, bullet);
        } else if (hostile && entity_has(world, other, C_MINE)) {
          mine_explode(world, other);
          entity_destroy(world, bullet);
        } else if (entity_has(world, other, C_FREIGHTER)) {
          // Anyone's fire chews a hauler; only player fire marks a kill
          freighter_hit(world, other, punch, contact, !hostile);
          entity_destroy(world, bullet);
        }
        continue;
      }

      // Raiders swarm the hauler; no elastic shove between them, or
      // the repeated bounces accelerate it out of the scene
      bool a_frt = entity_has(world, a, C_FREIGHTER);
      bool b_frt = entity_has(world, b, C_FREIGHTER);
      if ((a_frt && entity_has(world, b, C_PIRATE)) ||
          (b_frt && entity_has(world, a, C_PIRATE))) {
        continue;
      }

      // Contact normal from a to b (pick an arbitrary one if centered)
      Vec2f normal = dist > 0.0001f ? vec2f_mul(delta, 1.0f / dist)
                                    : vec2f_new(1.0f, 0.0f);

      float inv_ma = 1.0f / (ra * ra);
      float inv_mb = 1.0f / (rb * rb);
      float inv_sum = inv_ma + inv_mb;

      ta->position = vec2f_sub(ta->position,
                               vec2f_mul(normal, overlap * inv_ma / inv_sum));
      tb->position = vec2f_add(tb->position,
                               vec2f_mul(normal, overlap * inv_mb / inv_sum));

      Vec2f rel = vec2f_sub(world->velocities[b].value,
                            world->velocities[a].value);
      float approach = vec2f_dot(rel, normal);
      if (approach >= 0.0f) continue;

      float impulse = -(1.0f + RESTITUTION) * approach / inv_sum;
      world->velocities[a].value = vec2f_sub(
          world->velocities[a].value, vec2f_mul(normal, impulse * inv_ma));
      world->velocities[b].value = vec2f_add(
          world->velocities[b].value, vec2f_mul(normal, impulse * inv_mb));

      bool a_mine = entity_has(world, a, C_MINE);
      bool b_mine = entity_has(world, b, C_MINE);
      if (a_mine != b_mine) {
        Entity mine = a_mine ? a : b;
        Entity toucher = a_mine ? b : a;
        if (entity_has(world, toucher, C_PIRATE)) {
          mine_explode(world, mine);
          continue;
        }
      }

      bool a_player = entity_has(world, a, C_PLAYER);
      bool b_player = entity_has(world, b, C_PLAYER);
      if (a_player != b_player) {
        Entity ship  = a_player ? a : b;
        Entity other = a_player ? b : a;

        Player *player = &world->players[ship];
        if (player->damage_timer <= 0.0f) {
          int ram_damage = 0;
          if (entity_has(world, other, C_ASTEROID)) {
            ram_damage = 20 + (int)(world->asteroids[other].radius * 0.5f);
          } else if (entity_has(world, other, C_FREIGHTER)) {
            ram_damage = FREIGHTER_RAM_DAMAGE;
            freighter_hit(world, other, FREIGHTER_RAM_HIT,
                          world->transforms[ship].position, true);
          } else if (entity_has(world, other, C_PIRATE)) {
            const PirateStats *pst =
                pirate_stats(world->pirates[other].archetype);
            ram_damage = pst->ram_damage;
            // Kamikazes are spent on contact; the burst rides the same hit
            if (pst->kamikaze) pirate_detonate(world, other);
          }

          if (ram_damage > 0) {
            player_take_damage(player, ram_damage);
            player->damage_timer = SHIP_DAMAGE_COOLDOWN;
            world->wireframes[ship].flash = 0.1f;
            events_emit(EV_PLAYER_HURT, world->transforms[ship].position);
            if (player->hp <= 0) ship_explode(world, ship);
          }
        }
      }

      float impact = -approach;  // closing speed along the normal
      if (impact > IMPACT_DAMAGE_MIN) {
        // Ships hit dense for their size
        float heft_a = a_player ? ra * 2.0f : ra;
        float heft_b = b_player ? rb * 2.0f : rb;
        Vec2f contact = vec2f_add(ta->position, vec2f_mul(normal, ra));

        int dmg_a = (int)(impact / IMPACT_DAMAGE_UNIT * (heft_b / heft_a));
        int dmg_b = (int)(impact / IMPACT_DAMAGE_UNIT * (heft_a / heft_b));

        // Rock-on-rock grinding wears gentler than ship impacts
        if (entity_has(world, a, C_ASTEROID) && entity_has(world, b, C_ASTEROID)) {
          dmg_a /= 2;
          dmg_b /= 2;
        }

        if (dmg_a > 0) {
          if (entity_has(world, a, C_ASTEROID)) asteroid_hit(world, a, dmg_a, contact);
          else if (entity_has(world, a, C_PIRATE))
            pirate_hit(world, a, dmg_a, contact, false);
        }
        if (dmg_b > 0) {
          if (entity_has(world, b, C_ASTEROID)) asteroid_hit(world, b, dmg_b, contact);
          else if (entity_has(world, b, C_PIRATE))
            pirate_hit(world, b, dmg_b, contact, false);
        }
      }
    }
  }
}

void system_signals(World *world, Quest *quest) {
  Entity player = find_player(world);
  if (player == MAX_ENTITIES) return;
  Vec2f ppos = world->transforms[player].position;

  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_SIGNAL | C_TRANSFORM)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;

    Vec2f pos = world->transforms[e].position;
    if (vec2f_length(vec2f_sub(pos, ppos)) > SIGNAL_RANGE) continue;

    // One broadcast per beacon; it stays behind as scenery
    if (world->signals[e].contract && quest->type == QUEST_NONE) {
      quest_grant_bounty(quest, world, pos);
      events_emit(EV_SIGNAL_CONTRACT, pos);
    } else {
      events_emit(EV_SIGNAL, pos);
    }
    world->masks[e] &= ~C_SIGNAL;
  }
}

/** Hide the ship on alternating slices of its i-frames. */
static bool damage_flicker(const World *world, Entity e) {
  if (!entity_has(world, e, C_PLAYER)) return false;

  const Player *player = &world->players[e];
  return player->damage_timer > 0.0f &&
         fmodf(player->damage_timer, 0.15f) > 0.075f;
}

/** Alpha multiplier: 1.0 for immortal entities, remaining lifetime
 *  fraction otherwise. */
static float fade(const World *world, Entity e) {
  if (!entity_has(world, e, C_LIFETIME)) return 1.0f;

  const Lifetime *lt = &world->lifetimes[e];
  if (lt->initial <= 0.0f) return 1.0f;

  float frac = lt->remaining / lt->initial;
  return frac < 0.0f ? 0.0f : frac;
}

#define INTERP_SNAP_DIST 200.0f
#define INTERP_SNAP_ANGLE 90.0f

/** Blend the last two sim steps so motion is smooth at any display
 *  rate; big deltas mean a spawn or a freeze catch-up teleport, which
 *  must snap instead of smear. */
Transform render_transform(const World *world, Entity e, float alpha) {
  Transform cur = world->transforms[e];
  Transform prev = world->prev_transforms[e];

  Vec2f d = vec2f_sub(cur.position, prev.position);
  float da = cur.angle - prev.angle;
  if (fabsf(d.x) > INTERP_SNAP_DIST || fabsf(d.y) > INTERP_SNAP_DIST ||
      fabsf(da) > INTERP_SNAP_ANGLE) {
    return cur;
  }

  Transform out;
  out.position = vec2f_add(prev.position, vec2f_mul(d, alpha));
  out.angle = prev.angle + da * alpha;
  return out;
}

/** Local points to a closed screen-space loop; returns point count. */
static int wireframe_loop(const Transform *tf, const Wireframe *wf,
                          Vec2f view_offset, SDL_FPoint *out) {
  float rad = DEG_TO_RAD(tf->angle);
  Vec2f screen_pos = vec2f_add(tf->position, view_offset);

  for (int i = 0; i < wf->point_count; i++) {
    Vec2f pt = vec2f_add(screen_pos, vec2f_rotate(wf->points[i], rad));
    out[i] = (SDL_FPoint){ .x = pt.x, .y = pt.y };
  }
  out[wf->point_count] = out[0];

  return wf->point_count + 1;
}

void system_render(World *world, SDL_Renderer *renderer, Vec2f view_offset,
                   float alpha) {
  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_TRANSFORM | C_WIREFRAME)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;  // off-screen by definition
    if (damage_flicker(world, e)) continue;

    Transform ti = render_transform(world, e, alpha);
    Wireframe *wf = &world->wireframes[e];
    float rad = DEG_TO_RAD(ti.angle);
    Vec2f screen_pos = vec2f_add(ti.position, view_offset);

    SDL_FPoint loop[WIREFRAME_MAX_POINTS + 1];
    int count = wireframe_loop(&ti, wf, view_offset, loop);

    // Core line — hit flash overrides to white; the halo comes from
    // the bloom pass, not extra strokes
    SDL_Color col = wf->flash > 0.0f ? (SDL_Color){ 255, 255, 255, 255 }
                                     : wf->color;
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b,
                           (Uint8)(col.a * fade(world, e)));
    draw_lines(renderer, loop, count);

    if (entity_has(world, e, C_PLAYER) && world->players[e].thrusting) {
      Player *player = &world->players[e];

      Vec2f local_exhaust = vec2f_new(0.0f, player->exhaust_offset);
      Vec2f exhaust_world = vec2f_add(
                                      screen_pos,
                                      vec2f_rotate(local_exhaust, rad)
                                      );

      // Flame extends backward from ship, longer on upgraded engines
      float fire_scale = sqrtf(player->thrust_force / SHIP_THRUST_FORCE);
      Vec2f flame_dir = vec2f_rotate(vec2f_new(0.0f, 1.0f), rad);
      Vec2f side = vec2f_rotate(vec2f_new(1.0f, 0.0f), rad);
      float flicker = 0.6f + 0.4f * (float)(SDL_GetTicks() % 100) / 100.0f;
      float len = player->flame_length * flicker * fire_scale;
      float half_w = player->flame_length * 0.4f * fire_scale;

      // Icon-style wedge: an open V off the stern with a hot core
      Vec2f apex = vec2f_add(exhaust_world, vec2f_mul(flame_dir, len));
      SDL_FPoint outer[3] = {
        { .x = exhaust_world.x - side.x * half_w,
          .y = exhaust_world.y - side.y * half_w },
        { .x = apex.x, .y = apex.y },
        { .x = exhaust_world.x + side.x * half_w,
          .y = exhaust_world.y + side.y * half_w },
      };
      Vec2f core_apex = vec2f_add(exhaust_world,
                                  vec2f_mul(flame_dir, len * 0.55f));
      SDL_FPoint core[3] = {
        { .x = exhaust_world.x - side.x * half_w * 0.45f,
          .y = exhaust_world.y - side.y * half_w * 0.45f },
        { .x = core_apex.x, .y = core_apex.y },
        { .x = exhaust_world.x + side.x * half_w * 0.45f,
          .y = exhaust_world.y + side.y * half_w * 0.45f },
      };

      SDL_SetRenderDrawColor(renderer, 255, 110, 50, 255);
      draw_lines(renderer, outer, 3);
      SDL_SetRenderDrawColor(renderer, 255, 220, 110, 220);
      draw_lines(renderer, core, 3);
    }
  }
}

/** Lines are drawn a few times with 1px jitter so the downscale blur
 *  has enough energy to produce a visible halo. */
void system_render_glow(World *world, SDL_Renderer *renderer, Vec2f view_offset,
                        float alpha) {
  static const Vec2f JITTER[] = {
    {  0.0f,  0.0f },
    {  1.0f,  0.0f },
    { -1.0f,  0.0f },
    {  0.0f,  1.0f },
    {  0.0f, -1.0f },
  };
  static const int JITTER_COUNT = (int)(sizeof(JITTER) / sizeof(JITTER[0]));

  for (Entity e = 0; e < world->high_water; e++) {
    if (!entity_has(world, e, C_GLOW | C_TRANSFORM | C_WIREFRAME)) continue;
    if (entity_has(world, e, C_FROZEN)) continue;
    if (damage_flicker(world, e)) continue;

    Transform ti = render_transform(world, e, alpha);
    Wireframe *wf = &world->wireframes[e];
    SDL_FPoint loop[WIREFRAME_MAX_POINTS + 1];
    int count = wireframe_loop(&ti, wf, view_offset, loop);

    SDL_Color glow = wf->flash > 0.0f ? (SDL_Color){ 255, 255, 255, 255 }
                                      : wf->glow_color;
    SDL_SetRenderDrawColor(renderer, glow.r, glow.g, glow.b,
                           (Uint8)(255.0f * fade(world, e)));

    for (int j = 0; j < JITTER_COUNT; j++) {
      SDL_FPoint shifted[WIREFRAME_MAX_POINTS + 1];
      for (int i = 0; i < count; i++) {
        shifted[i] = (SDL_FPoint){ loop[i].x + JITTER[j].x,
                                   loop[i].y + JITTER[j].y };
      }
      draw_lines(renderer, shifted, count);
    }
  }
}
