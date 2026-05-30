#include "Games.h"
#include "GameUtils.h"

// ===========================================================================
// Six distinct shooter games. They share helper idioms (fixed-size arrays,
// frame pacing, sparks/shake juice, difficulty + waves + best score) but each
// has its own identity, controls and entity logic. No heap is used in loops;
// all bullets/enemies live in fixed caps. Every loop calls engine.tick() and
// honors shouldExitGame()->waitForRelease()->return.
//
// SD best keys: asteroids, invaders, missilecmd, turret, ufo, meteor
// ===========================================================================

namespace {

// Difficulty -> {speedMul%, spawnFaster%, healthBonus} packed per game inline.
inline uint8_t diffPick(GamerEngine& engine, const char* title, const char* help, uint8_t& out) {
  uint8_t d = chooseDifficulty(engine, title, help);
  if (d == 255) return 255;
  out = d;
  return d;
}

// Flash + level-up cue between waves. Returns after a short banner.
void waveBanner(GamerEngine& engine, uint8_t wave) {
  engine.playSound(SOUND_LEVELUP);
  engine.ledPulse(CRGB::Yellow, 120);
  char buf[16];
  snprintf(buf, sizeof(buf), "WAVE %u", (unsigned)wave);
  for (uint8_t f = 0; f < 8; ++f) {
    engine.tick();
    if (engine.shouldExitGame()) return;
    engine.clear();
    if (f & 1) {
      engine.centerText(buf, gameY(engine, 26), 1, GAMER_ACCENT);
    } else {
      engine.centerText(buf, gameY(engine, 26), 1, GAMER_WHITE);
    }
    engine.show();
    delay(45);
  }
}

void clearSparks(SparkField& f) {
  for (uint8_t i = 0; i < SparkField::kMax; ++i) f.sparks[i].life = 0;
}

// =========================================================================
// 1) ASTEROIDS  -- rotate + thrust + shoot; rocks split when hit.
// =========================================================================
void runAsteroids(GamerEngine& engine) {
  const uint16_t best = bestLoad("asteroids");
  uint8_t diff = 1;
  if (diffPick(engine, "ASTEROIDS", "Rotate L/R, hold SEL thrust, tap fire", diff) == 255) return;

  const int8_t baseSpd = (diff == 0) ? 1 : (diff == 1) ? 2 : 3;

  // 8 rocks max; size 2=big,1=med,0=small. 5 bullets.
  struct Rock { int16_t x, y; int8_t vx, vy; uint8_t size; bool active; };
  struct Bul  { int16_t x, y; int8_t vx, vy; uint8_t life; };
  Rock rocks[8];
  Bul  bul[5];
  SparkField sparks;

  while (true) {
    for (auto& b : bul) b.life = 0;
    clearSparks(sparks);
    // ship state: position + angle (0..15) + velocity (fixed point /16)
    int16_t sx = 64 << 4, sy = 32 << 4;
    int16_t svx = 0, svy = 0;
    uint8_t ang = 0;            // 0..15 = *22.5deg
    uint16_t score = 0;
    uint8_t lives = 3, wave = 0;
    uint8_t shootCd = 0, shieldT = 0, rapidT = 0, invuln = 40;
    uint8_t shake = 0;
    uint32_t nextFrame = 0;
    bool dead = false;

    auto spawnWave = [&](uint8_t n) {
      for (uint8_t i = 0; i < 8; ++i) rocks[i].active = false;
      for (uint8_t i = 0; i < n && i < 8; ++i) {
        int16_t rx = random(0, 128), ry = random(0, 64);
        if (abs(rx - 64) < 24 && abs(ry - 32) < 24) rx = (rx + 50) & 127;
        rocks[i] = { (int16_t)(rx << 4), (int16_t)(ry << 4),
                     (int8_t)(random(0, 2) ? baseSpd : -baseSpd),
                     (int8_t)(random(0, 2) ? baseSpd : -baseSpd), 2, true };
      }
    };
    wave = 1;
    waveBanner(engine, wave);
    spawnWave(3);

    while (!dead) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }

      // rotate
      int8_t ax = heldAxis(engine);
      if (ax < 0) ang = (ang + 15) & 15;
      else if (ax > 0) ang = (ang + 1) & 15;

      // thrust while held
      bool thrust = engine.isHeld(BTN_SELECT);
      // fire on tap
      if (selectTap(engine) && shootCd == 0) {
        for (auto& b : bul) if (b.life == 0) {
          // direction vector approx from 16-step angle
          static const int8_t cx[16] = {16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6,0,6,11,15};
          static const int8_t cy[16] = {0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
          b.x = sx; b.y = sy;
          b.vx = cx[ang] >> 1; b.vy = cy[ang] >> 1;
          b.life = 26;
          shootCd = rapidT ? 2 : 6;
          engine.playSound(SOUND_SHOOT);
          sparksSpawn(sparks, gameX(engine, sx >> 4), gameY(engine, sy >> 4), GAMER_ACCENT, 2);
          break;
        }
      }

      if (!frameDue(nextFrame, 33)) { delay(2); continue; }
      if (shootCd) shootCd--;
      if (shieldT) shieldT--;
      if (rapidT) rapidT--;
      if (invuln) invuln--;
      if (shake) shake--;

      // physics: apply thrust
      if (thrust) {
        static const int8_t cx[16] = {16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6,0,6,11,15};
        static const int8_t cy[16] = {0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
        svx += cx[ang] >> 3; svy += cy[ang] >> 3;
        svx = constrain(svx, -40, 40); svy = constrain(svy, -40, 40);
        if (random(0, 3) == 0) engine.playSound(SOUND_THRUST);
      } else {
        svx = (svx * 31) >> 5; svy = (svy * 31) >> 5;  // mild drag
      }
      sx += svx; sy += svy;
      // wrap
      if (sx < 0) sx += 128 << 4; if (sx >= 128 << 4) sx -= 128 << 4;
      if (sy < 0) sy += 64 << 4;  if (sy >= 64 << 4)  sy -= 64 << 4;

      // bullets
      for (auto& b : bul) {
        if (!b.life) continue;
        b.x += b.vx << 4; b.y += b.vy << 4; b.life--;
        if (b.x < 0) b.x += 128 << 4; if (b.x >= 128 << 4) b.x -= 128 << 4;
        if (b.y < 0) b.y += 64 << 4;  if (b.y >= 64 << 4)  b.y -= 64 << 4;
      }

      // rocks move + collide
      uint8_t aliveRocks = 0;
      for (uint8_t i = 0; i < 8; ++i) {
        Rock& r = rocks[i];
        if (!r.active) continue;
        aliveRocks++;
        r.x += r.vx << 4; r.y += r.vy << 4;
        if (r.x < 0) r.x += 128 << 4; if (r.x >= 128 << 4) r.x -= 128 << 4;
        if (r.y < 0) r.y += 64 << 4;  if (r.y >= 64 << 4)  r.y -= 64 << 4;
        int16_t rr = (r.size + 1) * 4;  // radius approx
        // bullet hits
        for (auto& b : bul) {
          if (!b.life) continue;
          if (abs((r.x >> 4) - (b.x >> 4)) < rr && abs((r.y >> 4) - (b.y >> 4)) < rr) {
            b.life = 0;
            sparksSpawn(sparks, gameX(engine, r.x >> 4), gameY(engine, r.y >> 4), GAMER_WHITE, 5);
            score += (r.size + 1) * 5;
            engine.playSound(SOUND_HIT);
            engine.ledPulse(CRGB::Green, 40);
            if (r.size > 0) {
              // split into two smaller
              uint8_t made = 0;
              for (uint8_t j = 0; j < 8 && made < 2; ++j) {
                if (!rocks[j].active) {
                  rocks[j] = { r.x, r.y,
                               (int8_t)(random(0,2)?baseSpd+1:-(baseSpd+1)),
                               (int8_t)(random(0,2)?baseSpd+1:-(baseSpd+1)),
                               (uint8_t)(r.size - 1), true };
                  made++;
                }
              }
              r.active = false;
            } else {
              r.active = false;
              // small chance powerup drop
              if (random(0, 5) == 0) { rapidT = 220; engine.playSound(SOUND_POWERUP); engine.ledPulse(CRGB::Magenta, 120); }
            }
            break;
          }
        }
        if (!r.active) continue;
        // rock hits ship
        if (!invuln && !shieldT &&
            abs((r.x >> 4) - (sx >> 4)) < rr + 3 && abs((r.y >> 4) - (sy >> 4)) < rr + 3) {
          if (lives) lives--;
          invuln = 50; shake = 8;
          svx = svy = 0;
          sparksSpawn(sparks, gameX(engine, sx >> 4), gameY(engine, sy >> 4), TFT_RED, 8);
          engine.playSound(SOUND_HIT);
          engine.ledPulse(CRGB::Red, 150);
          if (lives == 0) dead = true;
        }
      }

      // next wave
      if (aliveRocks == 0 && !dead) {
        wave++;
        shieldT = 90;  // brief reward shield
        engine.playSound(SOUND_POWERUP);
        waveBanner(engine, wave);
        spawnWave(min<uint8_t>(3 + wave, 7));
      }

      // ---- draw ----
      int16_t ox = shake ? shakeOffset(2) : 0, oy = shake ? shakeOffset(2) : 0;
      engine.clear();
      // ship triangle
      {
        static const int8_t cx[16] = {16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6,0,6,11,15};
        static const int8_t cy[16] = {0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
        int16_t px = (sx >> 4) + ox, py = (sy >> 4) + oy;
        int16_t nx = px + (cx[ang] * 5 >> 4), ny = py + (cy[ang] * 5 >> 4);
        uint8_t la = (ang + 6) & 15, ra = (ang + 10) & 15;
        int16_t lx = px + (cx[la] * 4 >> 4), ly = py + (cy[la] * 4 >> 4);
        int16_t rx2 = px + (cx[ra] * 4 >> 4), ry2 = py + (cy[ra] * 4 >> 4);
        uint16_t sc = (invuln && (invuln & 2)) ? GAMER_DIM : GAMER_WHITE;
        engine.screen().drawTriangle(gameX(engine, nx), gameY(engine, ny),
                                     gameX(engine, lx), gameY(engine, ly),
                                     gameX(engine, rx2), gameY(engine, ry2), sc);
        if (shieldT) engine.screen().drawCircle(gameX(engine, px), gameY(engine, py), gameSize(engine, 8), GAMER_ACCENT);
      }
      for (auto& b : bul) if (b.life)
        engine.screen().fillRect(gameX(engine, (b.x >> 4) + ox), gameY(engine, (b.y >> 4) + oy), gameSize(engine, 2), gameSize(engine, 2), GAMER_ACCENT);
      for (uint8_t i = 0; i < 8; ++i) if (rocks[i].active) {
        int16_t rr = (rocks[i].size + 1) * 3;
        engine.screen().drawCircle(gameX(engine, (rocks[i].x >> 4) + ox), gameY(engine, (rocks[i].y >> 4) + oy), gameSize(engine, rr), GAMER_WHITE);
      }
      sparksUpdateDraw(sparks, engine);
      drawLives(engine, gameX(engine, 0), gameY(engine, 0), lives);
      drawScore(engine, score);
      drawBestTag(engine, best, gameY(engine, 8));
      if (rapidT) engine.screen().fillRect(gameX(engine, 0), gameY(engine, 58), gameSize(engine, 12), gameSize(engine, 3), GAMER_ACCENT);
      engine.show();
    }

    engine.playSound(SOUND_LOSE);
    engine.ledPulse(CRGB::Red, 280);
    bool rec = bestSubmit("asteroids", score);
    if (!resultScreenBest(engine, "GAME OVER", score, max(best, score), rec)) return;
  }
}

// =========================================================================
// 2) INVADERS -- descending formation, speeds up as ranks thin; shields.
// =========================================================================
void runInvaders(GamerEngine& engine) {
  const uint16_t best = bestLoad("invaders");
  uint8_t diff = 1;
  if (diffPick(engine, "INVADERS", "L/R move, SEL fire, defend shields", diff) == 255) return;

  const uint8_t COLS = 6, ROWS = 3, MAXE = COLS * ROWS; // 18
  struct Bul { int16_t x, y; bool active; };
  // enemy alive bitmask + type per slot
  bool alive[MAXE];
  uint8_t etype[MAXE];           // 0 weak,1 mid,2 tough(needs 2 hits, marker only)
  uint8_t ehp[MAXE];
  Bul pShot, eShot[3];
  uint8_t shield[4];             // 4 shield blocks 0..3 health
  SparkField sparks;

  const uint8_t baseStep = (diff == 0) ? 1 : (diff == 1) ? 2 : 3;

  while (true) {
    clearSparks(sparks);
    int16_t px = 60;
    pShot.active = false;
    for (auto& e : eShot) e.active = false;
    uint16_t score = 0;
    uint8_t lives = 3, wave = 0;
    uint8_t shieldHp = 3, rapidT = 0, spreadT = 0, shake = 0;
    uint32_t nextFrame = 0, eShotTimer = 0;
    bool dead = false;

    int16_t formX = 8, formY = 8, formDir = 1;
    uint8_t marchCd = 0;

    auto resetWave = [&](uint8_t w) {
      for (uint8_t i = 0; i < MAXE; ++i) {
        alive[i] = true;
        uint8_t row = i / COLS;
        etype[i] = (row == 0) ? 2 : (row == 1) ? 1 : 0;
        ehp[i] = (etype[i] == 2 && w >= 2) ? 2 : 1;
      }
      formX = 8; formY = 6 + (w > 1 ? 4 : 0); formDir = 1;
      for (uint8_t i = 0; i < 4; ++i) shield[i] = shieldHp;
    };

    wave = 1; waveBanner(engine, wave); resetWave(wave);

    while (!dead) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      px += heldAxis(engine) * 4;
      px = constrain(px, 4, 112);
      if (selectTap(engine) && !pShot.active) {
        pShot = { (int16_t)(px + 5), 50, true };
        engine.playSound(SOUND_SHOOT);
        sparksSpawn(sparks, gameX(engine, px + 5), gameY(engine, 50), GAMER_ACCENT, 2);
      }

      if (!frameDue(nextFrame, 30)) { delay(2); continue; }
      if (rapidT) rapidT--;
      if (spreadT) spreadT--;
      if (shake) shake--;

      // count alive -> faster march as thinned
      uint8_t aliveN = 0; int16_t lowest = 0;
      for (uint8_t i = 0; i < MAXE; ++i) if (alive[i]) { aliveN++; uint8_t r = i / COLS; int16_t ey = formY + r * 12; if (ey > lowest) lowest = ey; }
      uint8_t marchEvery = (aliveN > 12) ? 6 : (aliveN > 6) ? 4 : (aliveN > 2) ? 2 : 1;
      marchEvery = max<int>(1, marchEvery - (diff == 2 ? 1 : 0));

      // march formation
      if (++marchCd >= marchEvery) {
        marchCd = 0;
        // find horizontal extents of living cols
        int16_t minc = 99, maxc = -1;
        for (uint8_t i = 0; i < MAXE; ++i) if (alive[i]) { int16_t c = i % COLS; if (c < minc) minc = c; if (c > maxc) maxc = c; }
        int16_t leftPix = formX + minc * 18, rightPix = formX + maxc * 18 + 10;
        if ((formDir > 0 && rightPix >= 124) || (formDir < 0 && leftPix <= 2)) {
          formDir = -formDir;
          formY += 4;  // descend
          engine.playSound(SOUND_UI_MOVE);
        } else {
          formX += formDir * baseStep;
        }
      }

      // player shot
      if (pShot.active) {
        pShot.y -= 6;
        if (pShot.y < 0) pShot.active = false;
        // hit shields? (player shots pass; enemy shots damage). skip
        for (uint8_t i = 0; i < MAXE && pShot.active; ++i) {
          if (!alive[i]) continue;
          int16_t ex = formX + (i % COLS) * 18, ey = formY + (i / COLS) * 12;
          if (rectsOverlap(pShot.x, pShot.y, 2, 6, ex, ey, 10, 8)) {
            pShot.active = false;
            sparksSpawn(sparks, gameX(engine, ex + 5), gameY(engine, ey + 4), GAMER_WHITE, 4);
            if (--ehp[i] == 0) {
              alive[i] = false;
              score += (etype[i] + 1) * 10;
              engine.playSound(SOUND_HIT);
              engine.ledPulse(CRGB::Green, 35);
              if (random(0, 9) == 0) {
                uint8_t p = random(0, 2);
                if (p == 0) { rapidT = 200; } else { spreadT = 200; }
                engine.playSound(SOUND_POWERUP);
                engine.ledPulse(CRGB::Magenta, 120);
              }
            } else {
              engine.playSound(SOUND_HIT);
            }
          }
        }
      }
      // rapid fire: allow refire quickly handled by !pShot.active (rapid shrinks travel time via re-tap not needed); give auto when rapid
      if (rapidT && !pShot.active) {
        pShot = { (int16_t)(px + 5), 50, true };
      }

      // enemy shots: pick random living shooter
      if (++eShotTimer >= (uint32_t)(diff == 2 ? 14 : 20)) {
        eShotTimer = 0;
        for (auto& es : eShot) if (!es.active) {
          // random column lowest enemy
          int8_t pick = -1;
          for (uint8_t t = 0; t < 6; ++t) {
            uint8_t c = random(0, COLS);
            for (int8_t r = ROWS - 1; r >= 0; --r) { uint8_t idx = r * COLS + c; if (alive[idx]) { pick = idx; break; } }
            if (pick >= 0) break;
          }
          if (pick >= 0) {
            es = { (int16_t)(formX + (pick % COLS) * 18 + 4), (int16_t)(formY + (pick / COLS) * 12 + 8), true };
          }
          break;
        }
      }
      for (auto& es : eShot) {
        if (!es.active) continue;
        es.y += 3 + (diff == 2 ? 1 : 0);
        // shield collision
        for (uint8_t s = 0; s < 4; ++s) {
          int16_t shx = 14 + s * 28;
          if (shield[s] > 0 && es.active && rectsOverlap(es.x, es.y, 2, 5, shx, 44, 14, 5)) {
            es.active = false; shield[s]--;
            sparksSpawn(sparks, gameX(engine, es.x), gameY(engine, 44), GAMER_DIM, 4);
            engine.playSound(SOUND_HIT);
          }
        }
        if (!es.active) continue;
        if (rectsOverlap(es.x, es.y, 2, 5, px, 54, 12, 6)) {
          es.active = false;
          if (lives) lives--;
          shake = 8;
          sparksSpawn(sparks, gameX(engine, px + 6), gameY(engine, 54), TFT_RED, 8);
          engine.playSound(SOUND_HIT); engine.ledPulse(CRGB::Red, 150);
          if (lives == 0) dead = true;
        } else if (es.y > 62) es.active = false;
      }

      // invaders reach bottom?
      if (lowest + formY >= 52) { dead = true; }
      if (aliveN == 0 && !dead) {
        wave++;
        if (shieldHp < 4) shieldHp++;
        engine.playSound(SOUND_POWERUP);
        waveBanner(engine, wave);
        resetWave(wave);
      }

      // ---- draw ----
      int16_t ox = shake ? shakeOffset(2) : 0, oy = shake ? shakeOffset(1) : 0;
      engine.clear();
      // player ship
      engine.screen().fillRect(gameX(engine, px + ox), gameY(engine, 56 + oy), gameSize(engine, 12), gameSize(engine, 4), GAMER_WHITE);
      engine.screen().fillRect(gameX(engine, px + 5 + ox), gameY(engine, 53 + oy), gameSize(engine, 2), gameSize(engine, 3), GAMER_WHITE);
      // enemies
      for (uint8_t i = 0; i < MAXE; ++i) {
        if (!alive[i]) continue;
        int16_t ex = formX + (i % COLS) * 18, ey = formY + (i / COLS) * 12;
        if (etype[i] == 2) {
          engine.screen().drawRect(gameX(engine, ex), gameY(engine, ey), gameSize(engine, 10), gameSize(engine, 7), GAMER_ACCENT);
          engine.screen().drawPixel(gameX(engine, ex + 2), gameY(engine, ey + 2), GAMER_ACCENT);
        } else if (etype[i] == 1) {
          engine.screen().fillRect(gameX(engine, ex + 1), gameY(engine, ey + 1), gameSize(engine, 8), gameSize(engine, 5), GAMER_WHITE);
        } else {
          engine.screen().drawRect(gameX(engine, ex + 1), gameY(engine, ey + 1), gameSize(engine, 8), gameSize(engine, 5), GAMER_WHITE);
        }
      }
      // shields
      for (uint8_t s = 0; s < 4; ++s) if (shield[s] > 0) {
        int16_t shx = 14 + s * 28;
        uint16_t col = shield[s] >= 3 ? GAMER_ACCENT : GAMER_DIM;
        engine.screen().fillRect(gameX(engine, shx), gameY(engine, 44), gameSize(engine, 14), gameSize(engine, shield[s] + 1), col);
      }
      if (pShot.active) engine.screen().fillRect(gameX(engine, pShot.x), gameY(engine, pShot.y), gameSize(engine, 2), gameSize(engine, 6), GAMER_ACCENT);
      for (auto& es : eShot) if (es.active) engine.screen().fillRect(gameX(engine, es.x), gameY(engine, es.y), gameSize(engine, 2), gameSize(engine, 5), GAMER_INVERSE);
      sparksUpdateDraw(sparks, engine);
      drawLives(engine, gameX(engine, 0), gameY(engine, 0), lives);
      drawScore(engine, score);
      drawBestTag(engine, best, gameY(engine, 8));
      if (rapidT) engine.screen().fillRect(gameX(engine, 0), gameY(engine, 60), gameSize(engine, 10), gameSize(engine, 2), GAMER_ACCENT);
      if (spreadT) engine.screen().fillRect(gameX(engine, 12), gameY(engine, 60), gameSize(engine, 10), gameSize(engine, 2), GAMER_INVERSE);
      engine.show();
    }

    engine.playSound(SOUND_LOSE);
    engine.ledPulse(CRGB::Red, 280);
    bool rec = bestSubmit("invaders", score);
    if (!resultScreenBest(engine, "INVADED", score, max(best, score), rec)) return;
  }
}

// =========================================================================
// 3) MISSILE COMMAND -- defend cities, intercept incoming w/ timed blasts.
// =========================================================================
void runMissileCommand(GamerEngine& engine) {
  const uint16_t best = bestLoad("missilecmd");
  uint8_t diff = 1;
  if (diffPick(engine, "MISSILE CMD", "L/R aim site, SEL detonate, save cities", diff) == 255) return;

  struct Incoming { int16_t sx; int16_t x, y; int16_t tx; bool active; bool mirv; };
  struct Blast { int16_t x, y; uint8_t r; uint8_t maxR; bool expanding; bool active; };
  const uint8_t MAXI = 8, MAXB = 4, CITIES = 6;
  Incoming inc[MAXI];
  Blast blast[MAXB];
  bool city[CITIES];
  SparkField sparks;

  const uint8_t baseSpeed = (diff == 0) ? 1 : (diff == 1) ? 1 : 2;
  const uint8_t blastR = (diff == 0) ? 16 : (diff == 1) ? 13 : 10;

  while (true) {
    clearSparks(sparks);
    int16_t siteX = 64;
    for (auto& i : inc) i.active = false;
    for (auto& b : blast) b.active = false;
    for (auto& c : city) c = true;
    uint16_t score = 0;
    uint8_t wave = 0, ammo = 12, shake = 0;
    uint32_t nextFrame = 0, spawnTimer = 0;
    uint8_t toSpawn = 0;
    bool dead = false;

    auto startWave = [&](uint8_t w) {
      toSpawn = 5 + w * 2;
      ammo = 12 + w;       // resupply
      spawnTimer = 0;
    };
    wave = 1; waveBanner(engine, wave); startWave(wave);

    auto cityX = [&](uint8_t i) -> int16_t {
      // 3 left of center, 3 right; site (battery) at center
      int16_t slots[CITIES] = {10, 28, 46, 82, 100, 118};
      return slots[i];
    };

    while (!dead) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      siteX += heldAxis(engine) * 4;
      siteX = constrain(siteX, 0, 127);
      if (selectTap(engine) && ammo > 0) {
        for (auto& b : blast) if (!b.active) {
          b = { siteX, 40, 2, blastR, true, true };
          ammo--;
          engine.playSound(SOUND_SHOOT);
          engine.ledPulse(CRGB::Aqua, 40);
          break;
        }
      }

      if (!frameDue(nextFrame, 33)) { delay(2); continue; }
      if (shake) shake--;

      // spawn incoming
      if (toSpawn > 0 && ++spawnTimer >= (uint32_t)(diff == 2 ? 14 : 20)) {
        spawnTimer = 0;
        for (auto& i : inc) if (!i.active) {
          int16_t startX = random(0, 128);
          // target a random surviving city, else center
          int8_t tgt = -1;
          for (uint8_t t = 0; t < 8; ++t) { uint8_t c = random(0, CITIES); if (city[c]) { tgt = c; break; } }
          int16_t txp = (tgt >= 0) ? cityX(tgt) + 4 : 64;
          i = { startX, (int16_t)(startX << 4), 0, txp, true, (bool)(wave >= 3 && random(0, 5) == 0) };
          toSpawn--;
          break;
        }
      }

      // move incoming (fixed point x for slope)
      uint8_t aliveInc = 0;
      for (auto& i : inc) {
        if (!i.active) continue;
        aliveInc++;
        i.y += baseSpeed;
        // lerp x toward target by altitude
        int16_t span = i.tx - i.sx;
        i.x = (int16_t)(((int32_t)i.sx + (int32_t)span * i.y / 56) << 4);
        int16_t ix = i.x >> 4;
        // MIRV split mid-air
        if (i.mirv && i.y == 28) {
          i.mirv = false;
          for (auto& j : inc) if (!j.active) { j = { ix, i.x, i.y, (int16_t)constrain(i.tx + 24, 0, 127), true, false }; break; }
        }
        // reached ground
        if (i.y >= 56) {
          i.active = false;
          // destroy nearest city
          for (uint8_t c = 0; c < CITIES; ++c) if (city[c] && abs(cityX(c) + 4 - ix) < 8) {
            city[c] = false;
            shake = 9;
            sparksSpawn(sparks, gameX(engine, cityX(c) + 4), gameY(engine, 58), TFT_RED, 8);
            engine.playSound(SOUND_HIT); engine.ledPulse(CRGB::Red, 160);
          }
        }
      }

      // blasts expand then collapse; destroy incoming in radius
      for (auto& b : blast) {
        if (!b.active) continue;
        if (b.expanding) { b.r += 2; if (b.r >= b.maxR) b.expanding = false; }
        else { if (b.r > 2) b.r -= 2; else b.active = false; }
        for (auto& i : inc) {
          if (!i.active) continue;
          int16_t ix = i.x >> 4;
          if (abs(ix - b.x) < b.r && abs(i.y - b.y) < b.r) {
            i.active = false;
            score += 8 + wave;
            sparksSpawn(sparks, gameX(engine, ix), gameY(engine, i.y), GAMER_ACCENT, 4);
            engine.playSound(SOUND_HIT); engine.ledPulse(CRGB::Green, 30);
            if (random(0, 12) == 0) { ammo += 4; engine.playSound(SOUND_POWERUP); engine.ledPulse(CRGB::Magenta, 100); }
          }
        }
      }

      // city count
      uint8_t citiesLeft = 0; for (auto& c : city) if (c) citiesLeft++;
      if (citiesLeft == 0) dead = true;

      if (toSpawn == 0 && aliveInc == 0 && !dead) {
        score += citiesLeft * 25;  // bonus per surviving city
        wave++;
        engine.playSound(SOUND_POWERUP);
        waveBanner(engine, wave);
        startWave(wave);
      }

      // ---- draw ----
      int16_t ox = shake ? shakeOffset(2) : 0, oy = shake ? shakeOffset(2) : 0;
      engine.clear();
      engine.screen().drawFastHLine(0, gameY(engine, 60 + oy), engine.width(), GAMER_DIM);
      // cities
      for (uint8_t c = 0; c < CITIES; ++c) if (city[c])
        engine.screen().fillRect(gameX(engine, cityX(c) + ox), gameY(engine, 56 + oy), gameSize(engine, 8), gameSize(engine, 4), GAMER_ACCENT);
      // battery + aim site
      engine.screen().fillRect(gameX(engine, 60 + ox), gameY(engine, 57 + oy), gameSize(engine, 8), gameSize(engine, 3), GAMER_WHITE);
      engine.screen().drawLine(gameX(engine, 64), gameY(engine, 57), gameX(engine, siteX), gameY(engine, 6), GAMER_DIM);
      engine.screen().drawRect(gameX(engine, siteX - 3), gameY(engine, 4), gameSize(engine, 6), gameSize(engine, 6), GAMER_WHITE);
      // incoming trails
      for (auto& i : inc) if (i.active) {
        int16_t ix = i.x >> 4;
        engine.screen().drawLine(gameX(engine, i.sx), gameY(engine, 0), gameX(engine, ix), gameY(engine, i.y), GAMER_DIM);
        uint16_t col = i.mirv ? GAMER_INVERSE : GAMER_WHITE;
        engine.screen().fillRect(gameX(engine, ix), gameY(engine, i.y), gameSize(engine, 2), gameSize(engine, 3), col);
      }
      for (auto& b : blast) if (b.active)
        engine.screen().drawCircle(gameX(engine, b.x), gameY(engine, b.y), gameSize(engine, b.r), GAMER_ACCENT);
      sparksUpdateDraw(sparks, engine);
      // HUD: ammo + score
      engine.screen().setTextSize(engine.textScale());
      engine.screen().setCursor(gameX(engine, 0), gameY(engine, 0));
      engine.screen().print("A:");
      engine.screen().print(ammo);
      drawScore(engine, score);
      drawBestTag(engine, best, gameY(engine, 8));
      engine.show();
    }

    engine.playSound(SOUND_LOSE);
    engine.ledPulse(CRGB::Red, 280);
    bool rec = bestSubmit("missilecmd", score);
    if (!resultScreenBest(engine, "CITIES LOST", score, max(best, score), rec)) return;
  }
}

// =========================================================================
// 4) TURRET DEFENSE -- fixed turret aiming across a firing arc vs waves.
// =========================================================================
void runTurretDef(GamerEngine& engine) {
  const uint16_t best = bestLoad("turret");
  uint8_t diff = 1;
  if (diffPick(engine, "TURRET DEF", "L/R swing arc, SEL fire, hold the line", diff) == 255) return;

  // Turret at bottom center. Aim angle across an arc (0..32 -> ~0..180deg over top).
  struct Bul { int16_t x, y; int16_t vx, vy; uint8_t life; };
  struct Foe { int16_t x, y; int8_t vx, vy; uint8_t type; uint8_t hp; bool active; };
  const uint8_t MAXB = 6, MAXF = 7;
  Bul bul[MAXB];
  Foe foe[MAXF];
  SparkField sparks;

  const uint8_t foeSpeed = (diff == 0) ? 1 : (diff == 1) ? 2 : 2;
  const uint8_t coreHpStart = (diff == 0) ? 5 : (diff == 1) ? 4 : 3;

  // angle table 0..32 across 180 degrees (turret muzzle dir). cos/sin scaled /16
  // angle 0 = left horizon, 16 = straight up, 32 = right horizon.
  auto dirX = [](uint8_t a) -> int16_t { int t = (int)a * 180 / 32; float r = t * 3.14159f / 180.0f; return (int16_t)(-cosf(r) * 16); };
  auto dirY = [](uint8_t a) -> int16_t { int t = (int)a * 180 / 32; float r = t * 3.14159f / 180.0f; return (int16_t)(-sinf(r) * 16); };

  const int16_t TX = 64, TY = 60;

  while (true) {
    clearSparks(sparks);
    for (auto& b : bul) b.life = 0;
    for (auto& f : foe) f.active = false;
    uint8_t aim = 16, core = coreHpStart, wave = 0;
    uint16_t score = 0;
    uint8_t shootCd = 0, rapidT = 0, shake = 0, toSpawn = 0;
    uint32_t nextFrame = 0, spawnTimer = 0;
    bool dead = false;

    auto startWave = [&](uint8_t w) { toSpawn = 4 + w * 2; spawnTimer = 0; };
    wave = 1; waveBanner(engine, wave); startWave(wave);

    while (!dead) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      int8_t ax = heldAxis(engine);
      if (ax) { aim = constrain(aim + ax, 0, 32); }
      if (selectTap(engine) && shootCd == 0) {
        for (auto& b : bul) if (!b.life) {
          b = { (int16_t)(TX + (dirX(aim) >> 1)), (int16_t)(TY + (dirY(aim) >> 1)), (int16_t)(dirX(aim) >> 1), (int16_t)(dirY(aim) >> 1), 24 };
          shootCd = rapidT ? 2 : 7;
          engine.playSound(SOUND_SHOOT);
          sparksSpawn(sparks, gameX(engine, b.x), gameY(engine, b.y), GAMER_ACCENT, 2);
          break;
        }
      }

      if (!frameDue(nextFrame, 33)) { delay(2); continue; }
      if (shootCd) shootCd--;
      if (rapidT) rapidT--;
      if (shake) shake--;

      // spawn foes from top edges descending toward turret
      if (toSpawn > 0 && ++spawnTimer >= (uint32_t)(diff == 2 ? 16 : 24)) {
        spawnTimer = 0;
        for (auto& f : foe) if (!f.active) {
          int16_t fx = random(4, 124), fy = random(0, 8);
          uint8_t type = (wave >= 2 && random(0, 4) == 0) ? 1 : 0;  // 1=armored
          // velocity toward turret
          int16_t dx = TX - fx, dy = TY - fy;
          int16_t mag = max<int>(1, abs(dx) + abs(dy));
          int8_t vx = (int8_t)constrain(dx * foeSpeed / (mag / 4 + 1), -3, 3);
          int8_t vy = (int8_t)constrain(dy * foeSpeed / (mag / 4 + 1), 1, 3);
          f = { fx, fy, vx, vy, type, (uint8_t)(type ? 3 : 1), true };
          toSpawn--;
          break;
        }
      }

      // bullets
      for (auto& b : bul) {
        if (!b.life) continue;
        b.x += b.vx; b.y += b.vy; b.life--;
        if (b.x < 0 || b.x > 127 || b.y < 0 || b.y > 63) b.life = 0;
      }

      // foes move + collide
      uint8_t aliveF = 0;
      for (auto& f : foe) {
        if (!f.active) continue;
        aliveF++;
        f.x += f.vx; f.y += f.vy;
        for (auto& b : bul) {
          if (!b.life) continue;
          if (abs(b.x - f.x) < 6 && abs(b.y - f.y) < 6) {
            b.life = 0;
            sparksSpawn(sparks, gameX(engine, f.x), gameY(engine, f.y), GAMER_WHITE, 3);
            if (--f.hp == 0) {
              f.active = false;
              score += (f.type + 1) * 10;
              engine.playSound(SOUND_HIT); engine.ledPulse(CRGB::Green, 30);
              if (random(0, 10) == 0) { rapidT = 200; engine.playSound(SOUND_POWERUP); engine.ledPulse(CRGB::Magenta, 110); }
            } else engine.playSound(SOUND_HIT);
            break;
          }
        }
        if (!f.active) continue;
        if (abs(f.x - TX) < 7 && abs(f.y - TY) < 7) {
          f.active = false;
          if (core) core--;
          shake = 9;
          sparksSpawn(sparks, gameX(engine, TX), gameY(engine, TY), TFT_RED, 8);
          engine.playSound(SOUND_HIT); engine.ledPulse(CRGB::Red, 160);
          if (core == 0) dead = true;
        }
      }

      if (toSpawn == 0 && aliveF == 0 && !dead) {
        wave++;
        if (core < coreHpStart) { core++; }  // repair 1
        engine.playSound(SOUND_POWERUP);
        waveBanner(engine, wave);
        startWave(wave);
      }

      // ---- draw ----
      int16_t ox = shake ? shakeOffset(2) : 0, oy = shake ? shakeOffset(2) : 0;
      engine.clear();
      // turret base + muzzle line
      engine.screen().fillRect(gameX(engine, TX - 5 + ox), gameY(engine, TY + oy), gameSize(engine, 10), gameSize(engine, 4), GAMER_WHITE);
      engine.screen().drawLine(gameX(engine, TX + ox), gameY(engine, TY + oy),
                               gameX(engine, TX + dirX(aim) + ox), gameY(engine, TY + dirY(aim) + oy), GAMER_ACCENT);
      // arc hint
      engine.screen().drawCircle(gameX(engine, TX), gameY(engine, TY), gameSize(engine, 6), GAMER_DIM);
      for (auto& b : bul) if (b.life)
        engine.screen().fillRect(gameX(engine, b.x), gameY(engine, b.y), gameSize(engine, 2), gameSize(engine, 2), GAMER_ACCENT);
      for (auto& f : foe) if (f.active) {
        if (f.type) { engine.screen().drawRect(gameX(engine, f.x - 3), gameY(engine, f.y - 3), gameSize(engine, 6), gameSize(engine, 6), GAMER_INVERSE); }
        else { engine.screen().fillCircle(gameX(engine, f.x), gameY(engine, f.y), gameSize(engine, 3), GAMER_WHITE); }
      }
      sparksUpdateDraw(sparks, engine);
      // core HP HUD
      engine.screen().setTextSize(engine.textScale());
      engine.screen().setCursor(gameX(engine, 0), gameY(engine, 0));
      engine.screen().print("C:");
      engine.screen().print(core);
      drawScore(engine, score);
      drawBestTag(engine, best, gameY(engine, 8));
      if (rapidT) engine.screen().fillRect(gameX(engine, 0), gameY(engine, 60), gameSize(engine, 10), gameSize(engine, 2), GAMER_ACCENT);
      engine.show();
    }

    engine.playSound(SOUND_LOSE);
    engine.ledPulse(CRGB::Red, 280);
    bool rec = bestSubmit("turret", score);
    if (!resultScreenBest(engine, "OVERRUN", score, max(best, score), rec)) return;
  }
}

// =========================================================================
// 5) UFO DEFENDER -- track fast crossing UFOs; lead your shots.
// =========================================================================
void runUfo(GamerEngine& engine) {
  const uint16_t best = bestLoad("ufo");
  uint8_t diff = 1;
  if (diffPick(engine, "UFO DEFENDER", "L/R swing turret, SEL fire, lead them!", diff) == 255) return;

  // Bottom turret, fires straight up but you swing a small lateral velocity.
  // UFOs cross horizontally at varying heights/speeds; you must lead.
  struct Bul { int16_t x, y; int16_t vx; bool active; };
  struct Ufo { int16_t x, y; int8_t vx; uint8_t type; uint8_t hp; bool active; uint8_t bobT; };
  const uint8_t MAXB = 5, MAXU = 5;
  Bul bul[MAXB];
  Ufo ufo[MAXU];
  SparkField sparks;

  const uint8_t spdLo = (diff == 0) ? 2 : (diff == 1) ? 3 : 4;
  const uint8_t spdHi = spdLo + 2;

  while (true) {
    clearSparks(sparks);
    for (auto& b : bul) b.active = false;
    for (auto& u : ufo) u.active = false;
    int16_t turX = 64;
    uint16_t score = 0;
    uint8_t lives = 3, wave = 0, missStreak = 0;
    uint8_t shake = 0, rapidT = 0, spreadT = 0, combo = 0, toSpawn = 0;
    uint32_t nextFrame = 0, spawnTimer = 0;
    bool dead = false;

    auto startWave = [&](uint8_t w) { toSpawn = 5 + w * 2; spawnTimer = 0; };
    wave = 1; waveBanner(engine, wave); startWave(wave);

    while (!dead) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      turX += heldAxis(engine) * 4;
      turX = constrain(turX, 6, 121);
      if (selectTap(engine)) {
        // fire 1 (or 3 if spread). Lateral velocity = swing intent via current axis.
        int16_t lead = heldAxis(engine) * 3;
        for (auto& b : bul) if (!b.active) {
          b = { (int16_t)turX, 54, lead, true };
          engine.playSound(SOUND_SHOOT);
          sparksSpawn(sparks, gameX(engine, turX), gameY(engine, 54), GAMER_ACCENT, 2);
          if (spreadT) {
            for (auto& b2 : bul) if (!b2.active && &b2 != &b) { b2 = { (int16_t)turX, 54, (int16_t)(lead + 3), true }; break; }
            for (auto& b3 : bul) if (!b3.active && &b3 != &b) { b3 = { (int16_t)turX, 54, (int16_t)(lead - 3), true }; break; }
          }
          break;
        }
      }

      if (!frameDue(nextFrame, 30)) { delay(2); continue; }
      if (shake) shake--;
      if (rapidT) rapidT--;
      if (spreadT) spreadT--;

      if (toSpawn > 0 && ++spawnTimer >= (uint32_t)(rapidT ? 10 : (diff == 2 ? 16 : 22))) {
        spawnTimer = 0;
        for (auto& u : ufo) if (!u.active) {
          bool fromLeft = random(0, 2);
          uint8_t type = (wave >= 2 && random(0, 4) == 0) ? 1 : 0; // 1=fast small
          int8_t sp = random(spdLo, spdHi + 1) + (type ? 1 : 0);
          u = { (int16_t)(fromLeft ? -8 : 135), (int16_t)random(8, 40), (int8_t)(fromLeft ? sp : -sp), type, (uint8_t)(type ? 1 : 2), true, (uint8_t)random(0, 6) };
          toSpawn--;
          break;
        }
      }

      // bullets up
      for (auto& b : bul) {
        if (!b.active) continue;
        b.y -= 5; b.x += b.vx;
        if (b.y < 0 || b.x < 0 || b.x > 127) {
          b.active = false;
          missStreak++; combo = 0;  // a miss breaks combo
        }
      }

      // ufos cross
      uint8_t aliveU = 0;
      for (auto& u : ufo) {
        if (!u.active) continue;
        aliveU++;
        u.x += u.vx;
        u.bobT++;
        int16_t uy = u.y + ((u.bobT >> 2) & 1 ? 1 : 0);
        if (u.x < -10 || u.x > 138) {
          u.active = false;
          // escaped: lose a life
          if (lives) lives--;
          shake = 7; combo = 0;
          engine.playSound(SOUND_LOSE); engine.ledPulse(CRGB::Red, 120);
          if (lives == 0) dead = true;
          continue;
        }
        for (auto& b : bul) {
          if (!b.active) continue;
          if (abs(b.x - u.x) < 7 && abs(b.y - uy) < 5) {
            b.active = false;
            missStreak = 0;
            sparksSpawn(sparks, gameX(engine, u.x), gameY(engine, uy), GAMER_WHITE, 4);
            if (--u.hp == 0) {
              u.active = false;
              combo = (combo < 250) ? combo + 1 : combo;
              uint16_t add = (u.type + 1) * 10 + combo * 2;
              score += add;
              if (combo >= 3) engine.playSound(SOUND_COMBO); else engine.playSound(SOUND_HIT);
              engine.ledPulse(CRGB::Green, 30);
              if (random(0, 9) == 0) {
                if (random(0, 2)) { rapidT = 200; } else { spreadT = 200; }
                engine.playSound(SOUND_POWERUP); engine.ledPulse(CRGB::Magenta, 110);
              }
            } else engine.playSound(SOUND_HIT);
            break;
          }
        }
      }

      if (toSpawn == 0 && aliveU == 0 && !dead) {
        wave++;
        engine.playSound(SOUND_POWERUP);
        waveBanner(engine, wave);
        startWave(wave);
      }

      // ---- draw ----
      int16_t ox = shake ? shakeOffset(2) : 0, oy = shake ? shakeOffset(1) : 0;
      engine.clear();
      // turret
      engine.screen().fillRect(gameX(engine, turX - 4 + ox), gameY(engine, 56 + oy), gameSize(engine, 8), gameSize(engine, 4), GAMER_WHITE);
      engine.screen().drawFastVLine(gameX(engine, turX + ox), gameY(engine, 52 + oy), gameSize(engine, 4), GAMER_WHITE);
      for (auto& b : bul) if (b.active)
        engine.screen().fillRect(gameX(engine, b.x), gameY(engine, b.y), gameSize(engine, 2), gameSize(engine, 3), GAMER_ACCENT);
      for (auto& u : ufo) if (u.active) {
        int16_t uy = u.y + ((u.bobT >> 2) & 1 ? 1 : 0);
        if (u.type) {
          engine.screen().drawCircle(gameX(engine, u.x), gameY(engine, uy), gameSize(engine, 3), GAMER_INVERSE);
        } else {
          engine.screen().fillRect(gameX(engine, u.x - 4), gameY(engine, uy), gameSize(engine, 8), gameSize(engine, 2), GAMER_WHITE);
          engine.screen().drawPixel(gameX(engine, u.x), gameY(engine, uy - 1), GAMER_ACCENT);
        }
      }
      sparksUpdateDraw(sparks, engine);
      drawLives(engine, gameX(engine, 0), gameY(engine, 0), lives);
      drawScore(engine, score);
      drawBestTag(engine, best, gameY(engine, 8));
      if (combo >= 2) {
        char cb[8]; snprintf(cb, sizeof(cb), "x%u", (unsigned)combo);
        engine.screen().setTextSize(engine.textScale());
        engine.screen().setCursor(gameX(engine, 30), gameY(engine, 0));
        engine.screen().print(cb);
      }
      if (rapidT) engine.screen().fillRect(gameX(engine, 0), gameY(engine, 60), gameSize(engine, 10), gameSize(engine, 2), GAMER_ACCENT);
      if (spreadT) engine.screen().fillRect(gameX(engine, 12), gameY(engine, 60), gameSize(engine, 10), gameSize(engine, 2), GAMER_INVERSE);
      engine.show();
    }

    engine.playSound(SOUND_LOSE);
    engine.ledPulse(CRGB::Red, 280);
    bool rec = bestSubmit("ufo", score);
    if (!resultScreenBest(engine, "UFOS WIN", score, max(best, score), rec)) return;
  }
}

// =========================================================================
// 6) METEOR BLAST -- tight aimed shots on falling meteors with combos.
// =========================================================================
void runMeteor(GamerEngine& engine) {
  const uint16_t best = bestLoad("meteor");
  uint8_t diff = 1;
  if (diffPick(engine, "METEOR BLAST", "L/R aim cursor, SEL snipe, chain combos", diff) == 255) return;

  // Aimed crosshair shooter: a cursor you move; SEL fires an instant hitscan
  // beam up the column. Reward precise rapid chains.
  struct Meteor { int16_t x, y; int8_t vy; uint8_t size; uint8_t hp; bool active; bool boss; };
  const uint8_t MAXM = 8;
  Meteor met[MAXM];
  SparkField sparks;

  const uint8_t fallLo = (diff == 0) ? 1 : (diff == 1) ? 1 : 2;
  const uint8_t fallHi = (diff == 0) ? 2 : (diff == 1) ? 3 : 4;

  while (true) {
    clearSparks(sparks);
    for (auto& m : met) m.active = false;
    int16_t cur = 64;
    uint16_t score = 0;
    uint8_t lives = 3, wave = 0;
    uint8_t shake = 0, combo = 0, beamT = 0, cool = 0, toSpawn = 0;
    int16_t beamX = 0;
    uint32_t nextFrame = 0, spawnTimer = 0, comboTimer = 0;
    bool dead = false;

    auto startWave = [&](uint8_t w) { toSpawn = 6 + w * 2; spawnTimer = 0; };
    wave = 1; waveBanner(engine, wave); startWave(wave);

    while (!dead) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cur += heldAxis(engine) * 4;
      cur = constrain(cur, 2, 125);
      if (selectTap(engine) && cool == 0) {
        // hitscan beam at column = cur; hit topmost meteor overlapping
        beamX = cur; beamT = 4; cool = 5;
        engine.playSound(SOUND_SHOOT);
        int8_t hitIdx = -1; int16_t bestY = 999;
        for (uint8_t i = 0; i < MAXM; ++i) {
          if (!met[i].active) continue;
          int16_t half = met[i].size;
          if (cur >= met[i].x - half && cur <= met[i].x + half && met[i].y < bestY) { bestY = met[i].y; hitIdx = i; }
        }
        if (hitIdx >= 0) {
          Meteor& m = met[hitIdx];
          sparksSpawn(sparks, gameX(engine, m.x), gameY(engine, m.y), GAMER_WHITE, 4);
          if (--m.hp == 0) {
            m.active = false;
            comboTimer = 0;
            combo = (combo < 250) ? combo + 1 : combo;
            uint16_t add = (m.boss ? 80 : (m.size >= 5 ? 15 : 10)) + combo * 3;
            score += add;
            if (combo >= 3) engine.playSound(SOUND_COMBO); else engine.playSound(SOUND_HIT);
            engine.ledPulse(CRGB::Green, 30);
            if (m.boss) { engine.playSound(SOUND_WIN); engine.ledPulse(CRGB::Yellow, 160); }
            else if (m.size >= 5) {
              // big meteor splits into 2 small
              uint8_t made = 0;
              for (uint8_t j = 0; j < MAXM && made < 2; ++j) if (!met[j].active) {
                met[j] = { (int16_t)(m.x + (made ? 6 : -6)), m.y, (int8_t)(m.vy + 1), 3, 1, true, false }; made++;
              }
            }
            if (random(0, 12) == 0) { lives = min<uint8_t>(lives + 1, 5); engine.playSound(SOUND_POWERUP); engine.ledPulse(CRGB::Magenta, 110); }
          } else {
            engine.playSound(SOUND_HIT);  // boss chip
          }
        } else {
          // missed shot resets combo
          combo = 0;
        }
      }

      if (!frameDue(nextFrame, 30)) { delay(2); continue; }
      if (shake) shake--;
      if (beamT) beamT--;
      if (cool) cool--;
      // combo decay window
      if (combo > 0 && ++comboTimer > 40) { combo = 0; comboTimer = 0; }

      // spawn meteors / boss every 4th wave
      if (toSpawn > 0 && ++spawnTimer >= (uint32_t)(diff == 2 ? 16 : 22)) {
        spawnTimer = 0;
        for (auto& m : met) if (!m.active) {
          bool boss = (wave % 4 == 0) && toSpawn == 1;  // last spawn of boss wave
          if (boss) m = { (int16_t)random(20, 108), -10, (int8_t)1, 9, (uint8_t)(8 + wave), true, true };
          else {
            uint8_t big = random(0, 3) == 0;
            m = { (int16_t)random(6, 122), (int16_t)random(-12, -4), (int8_t)random(fallLo, fallHi + 1), (uint8_t)(big ? 5 : 3), (uint8_t)(big ? 2 : 1), true, false };
          }
          toSpawn--;
          break;
        }
      }

      uint8_t aliveM = 0;
      for (auto& m : met) {
        if (!m.active) continue;
        aliveM++;
        m.y += m.vy;
        if (m.y > 60) {
          m.active = false;
          if (lives) lives--;
          shake = 8; combo = 0;
          sparksSpawn(sparks, gameX(engine, m.x), gameY(engine, 58), TFT_RED, 6);
          engine.playSound(SOUND_HIT); engine.ledPulse(CRGB::Red, 140);
          if (lives == 0) dead = true;
        }
      }

      if (toSpawn == 0 && aliveM == 0 && !dead) {
        wave++;
        engine.playSound(SOUND_POWERUP);
        waveBanner(engine, wave);
        startWave(wave);
      }

      // ---- draw ----
      int16_t ox = shake ? shakeOffset(2) : 0, oy = shake ? shakeOffset(2) : 0;
      engine.clear();
      engine.screen().drawFastHLine(0, gameY(engine, 60 + oy), engine.width(), GAMER_DIM);
      // meteors
      for (auto& m : met) if (m.active) {
        if (m.boss) {
          engine.screen().fillCircle(gameX(engine, m.x + ox), gameY(engine, m.y + oy), gameSize(engine, 7), GAMER_INVERSE);
          engine.screen().drawCircle(gameX(engine, m.x + ox), gameY(engine, m.y + oy), gameSize(engine, 7), GAMER_WHITE);
        } else {
          engine.screen().fillCircle(gameX(engine, m.x + ox), gameY(engine, m.y + oy), gameSize(engine, m.size >= 5 ? 4 : 2), GAMER_WHITE);
        }
      }
      // beam flash
      if (beamT) engine.screen().drawFastVLine(gameX(engine, beamX), gameY(engine, 0), gameSize(engine, 56), GAMER_ACCENT);
      // aiming cursor / crosshair at bottom
      engine.screen().drawFastVLine(gameX(engine, cur + ox), gameY(engine, 52 + oy), gameSize(engine, 6), GAMER_ACCENT);
      engine.screen().drawLine(gameX(engine, cur - 3), gameY(engine, 57), gameX(engine, cur + 3), gameY(engine, 57), GAMER_ACCENT);
      sparksUpdateDraw(sparks, engine);
      drawLives(engine, gameX(engine, 0), gameY(engine, 0), lives);
      drawScore(engine, score);
      drawBestTag(engine, best, gameY(engine, 8));
      if (combo >= 2) {
        char cb[8]; snprintf(cb, sizeof(cb), "x%u", (unsigned)combo);
        engine.screen().setTextSize(engine.textScale());
        engine.screen().setCursor(gameX(engine, 30), gameY(engine, 0));
        engine.screen().print(cb);
      }
      engine.show();
    }

    engine.playSound(SOUND_LOSE);
    engine.ledPulse(CRGB::Red, 280);
    bool rec = bestSubmit("meteor", score);
    if (!resultScreenBest(engine, "IMPACT", score, max(best, score), rec)) return;
  }
}

}  // namespace

// ---- Registered entry points (signatures fixed; see Games.cpp) -------------
void runAsteroidsLite(GamerEngine& engine)      { runAsteroids(engine); }
void runInvadersLite(GamerEngine& engine)       { runInvaders(engine); }
void runMissileCommandLite(GamerEngine& engine) { runMissileCommand(engine); }
void runTurretDefense(GamerEngine& engine)      { runTurretDef(engine); }
void runUfoDefender(GamerEngine& engine)        { runUfo(engine); }
void runMeteorBlaster(GamerEngine& engine)      { runMeteor(engine); }
