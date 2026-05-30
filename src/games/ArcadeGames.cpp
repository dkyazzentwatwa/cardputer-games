#include "Games.h"
#include "GameUtils.h"

// ---------------------------------------------------------------------------
// ARCADE GAMES (deepened)
//
// Twenty compact action games built on four shared, heavily parameterized
// helpers: runFlyer, runJumpRunner, runCatchOrDodge and runLaneGame. Each
// helper takes a small "variant" enum (and per-game flags) so that games
// sharing a helper still PLAY differently -- distinct objects, hazards,
// pickups, scoring and feel -- rather than being palette swaps.
//
// Universal depth (every game): difficulty pick (speed/spawn curve), lives
// with hearts instead of instant death for action games, a persistent best
// score (microSD) shown in the HUD and on the result screen, a level-up feel
// as the score climbs, and juice (spark bursts + brief screen shake + sound)
// on the key moments. All state is fixed-size stack storage -- no heap in the
// loops. Every loop calls engine.tick() and honors shouldExitGame().
// ---------------------------------------------------------------------------

namespace {

// Convenience colors layered on the engine palette.
constexpr uint16_t COL_GOLD = TFT_YELLOW;
constexpr uint16_t COL_RED = TFT_RED;
constexpr uint16_t COL_GREEN = TFT_GREEN;
constexpr uint16_t COL_MAG = TFT_MAGENTA;

void clearSparks(SparkField& f) {
  for (uint8_t i = 0; i < SparkField::kMax; ++i) f.sparks[i].life = 0;
}

// Shared HUD: score (left) + best tag (right) + a row of life hearts.
void drawHud(GamerEngine& engine, uint16_t score, uint16_t best, uint8_t lives) {
  drawScore(engine, score);
  drawBestTag(engine, best, 0);
  if (lives) drawLives(engine, gameX(engine, 2), gameY(engine, 9), lives);
}

// Level-up flourish: bump LED + sound when the player crosses a tier.
void levelUp(GamerEngine& engine) {
  engine.ledPulse(CRGB::Purple, 90);
  engine.playSound(SOUND_LEVELUP);
}

// =====================================================================
//  FLYER FAMILY  (Flappy / Tunnel / Jetpack / Cave)
// =====================================================================
enum FlyerKind : uint8_t { FLY_FLAPPY = 0, FLY_TUNNEL, FLY_JETPACK, FLY_CAVE };

void runFlyer(GamerEngine& engine, const char* title, const char* key, FlyerKind kind) {
  const uint16_t best = bestLoad(key);
  const bool held = (kind == FLY_JETPACK || kind == FLY_CAVE);

  while (true) {
    uint8_t diff = chooseDifficulty(engine, title,
                                    held ? "Hold SEL to climb" : "Tap SEL to flap");
    if (diff == 255) return;

    // Difficulty: scroll speed and gap generosity.
    const int8_t baseSpeed[3] = {3, 3, 4};
    const int16_t baseGap[3] = {26, 22, 18};

    int16_t y = 30, vy = 0;
    int16_t wallX = 126;
    int16_t gapY = 22, gapTarget = 22;
    int16_t gap = baseGap[diff];
    uint16_t score = 0, combo = 0;
    uint8_t lives = 3;
    int16_t fuel = 100;            // jetpack only
    bool pickupActive = false;     // crystal (cave) / fuel can (jetpack)
    int16_t pickX = 0, pickY = 0;
    bool centerObst = false;       // tunnel center pillar
    uint8_t shake = 0, lastTier = 0;
    uint32_t nextFrame = 0;
    SparkField sparks; clearSparks(sparks);

    bool alive = true;
    while (alive) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }

      // --- thrust input ---
      bool thrust = held ? engine.isHeld(BTN_SELECT) : selectTap(engine);
      if (held) {
        if (thrust && (kind != FLY_JETPACK || fuel > 0)) {
          vy -= 1;
          if (kind == FLY_JETPACK) fuel -= 1;
          engine.playSound(SOUND_THRUST);
        }
      } else if (thrust) {
        vy = -7;
        engine.playSound(SOUND_JUMP);
      }

      if (!frameDue(nextFrame, 34)) { delay(2); continue; }

      vy += 1;
      vy = constrain(vy, -7, 6);
      y += vy;
      wallX -= baseSpeed[diff] + score / 24;

      // Tunnel: gap narrows steadily; corridor center eases toward a target.
      if (kind == FLY_TUNNEL) {
        gap = baseGap[diff] - (int16_t)(score / 6);
        if (gap < 12) gap = 12;
        if (gapY < gapTarget) gapY++;
        else if (gapY > gapTarget) gapY--;
      }

      if (wallX < -8) {
        wallX = 128 + random(0, 10);
        if (kind == FLY_TUNNEL) {
          int16_t hi = 54 - gap;          // keep corridor on-screen
          int16_t lo = 8;
          if (hi <= lo) hi = lo + 1;
          gapTarget = random(lo, hi);
          centerObst = (score > 4) && (random(0, 3) == 0);
        } else {
          gapY = random(10, 44 - (gap - 18 > 0 ? gap - 18 : 0));
          if (gapY < 8) gapY = 8;
        }
        // Cave/jetpack pickups appear in the gap occasionally.
        if ((kind == FLY_CAVE || kind == FLY_JETPACK) && random(0, 2) == 0) {
          pickupActive = true;
          pickX = wallX + 4;
          pickY = gapY + gap / 2;
        }
        score++;
        combo++;
        if (combo >= 5 && combo % 5 == 0) {
          score += 2;  // clean-pass combo bonus (flappy/tunnel feel)
          engine.playSound(SOUND_COMBO);
        }
        engine.ledPulse(CRGB::Aqua, 40);
        engine.playSound(SOUND_SCORE);
        if (kind == FLY_JETPACK) { fuel += 6; if (fuel > 100) fuel = 100; }
      }

      // Tier-based level-up feel every 12 points.
      uint8_t tier = score / 12;
      if (tier != lastTier) { lastTier = tier; levelUp(engine); }

      // Pickup scroll + collect.
      if (pickupActive) {
        pickX -= baseSpeed[diff] + score / 24;
        if (pickX < -6) pickupActive = false;
        else if (rectsOverlap(16, y, 6, 6, pickX, pickY, 5, 5)) {
          pickupActive = false;
          if (kind == FLY_JETPACK) { fuel = 100; engine.playSound(SOUND_POWERUP); }
          else { score += 3; engine.playSound(SOUND_POWERUP); }
          engine.ledPulse(CRGB::Green, 70);
          sparksSpawn(sparks, gameX(engine, pickX), gameY(engine, pickY), COL_GOLD, 6);
        }
      }

      // --- collisions ---
      bool hit = false;
      const bool gapWall = (kind == FLY_FLAPPY || kind == FLY_JETPACK || kind == FLY_CAVE);
      if (y < 0 || y > 58) hit = true;
      if (gapWall) {
        if (wallX < 22 && wallX + 8 > 16 && (y < gapY || y + 6 > gapY + gap)) hit = true;
      } else {
        // Tunnel: stay inside the corridor; gapY eases toward the new target.
        int16_t topY = gapY, botY = gapY + gap;
        if (y < topY || y + 6 > botY) hit = true;
        if (centerObst && rectsOverlap(16, y, 6, 6, wallX, gapY + gap / 2 - 3, 6, 6))
          hit = true;
      }

      if (hit) {
        lives--;
        combo = 0;
        shake = 4;
        engine.ledPulse(CRGB::Red, 120);
        engine.playSound(SOUND_HIT);
        sparksSpawn(sparks, gameX(engine, 18), gameY(engine, y), COL_RED, 8);
        y = 30; vy = 0; wallX = 128;
        if (lives == 0) { alive = false; }
      }

      // --- draw ---
      int16_t ox = shakeOffset(shake), oy = shakeOffset(shake);
      if (shake) shake--;
      engine.clear();
      // hero
      engine.screen().fillRect(gameX(engine, 16 + ox), gameY(engine, y + oy),
                               gameSize(engine, 6), gameSize(engine, 6),
                               kind == FLY_JETPACK ? COL_GOLD : GAMER_WHITE);
      if (held && thrust)
        engine.screen().fillRect(gameX(engine, 17 + ox), gameY(engine, y + 6 + oy),
                                 gameSize(engine, 4), gameSize(engine, 2), COL_RED);

      if (gapWall) {
        engine.screen().fillRect(gameX(engine, wallX + ox), gameY(engine, oy),
                                 gameSize(engine, 8), gameY(engine, gapY), GAMER_ACCENT);
        engine.screen().fillRect(gameX(engine, wallX + ox), gameY(engine, gapY + gap + oy),
                                 gameSize(engine, 8),
                                 engine.height() - gameY(engine, gapY + gap),
                                 GAMER_ACCENT);
      } else {
        // Centered tunnel walls drawn full width for the corridor feel.
        engine.screen().fillRect(gameX(engine, ox), gameY(engine, oy),
                                 engine.width(), gameY(engine, gapY), GAMER_DIM);
        engine.screen().fillRect(gameX(engine, ox), gameY(engine, gapY + gap + oy),
                                 engine.width(), engine.height() - gameY(engine, gapY + gap),
                                 GAMER_DIM);
        if (centerObst)
          engine.screen().fillRect(gameX(engine, wallX + ox), gameY(engine, gapY + gap / 2 - 3 + oy),
                                   gameSize(engine, 6), gameSize(engine, 6), GAMER_ACCENT);
      }
      if (pickupActive)
        engine.screen().fillRect(gameX(engine, pickX + ox), gameY(engine, pickY + oy),
                                 gameSize(engine, 5), gameSize(engine, 5),
                                 kind == FLY_JETPACK ? COL_GREEN : COL_GOLD);

      // Jetpack fuel meter.
      if (kind == FLY_JETPACK) {
        engine.screen().drawRect(gameX(engine, 40), gameY(engine, 1),
                                 gameSize(engine, 40), gameSize(engine, 4), GAMER_DIM);
        uint16_t fc = fuel > 50 ? COL_GREEN : (fuel > 20 ? COL_GOLD : COL_RED);
        engine.screen().fillRect(gameX(engine, 41), gameY(engine, 2),
                                 gameSize(engine, fuel * 38 / 100), gameSize(engine, 2), fc);
      }

      sparksUpdateDraw(sparks, engine);
      drawHud(engine, score, best, lives);
      engine.show();
    }

    bool record = bestSubmit(key, score);
    engine.ledPulse(record ? CRGB::Green : CRGB::Red, 260);
    engine.playSound(record ? SOUND_WIN : SOUND_LOSE);
    if (!resultScreenBest(engine, "GAME OVER", score, best, record)) return;
  }
}

// =====================================================================
//  JUMP FAMILY  (Dino / Platform Hop / Gravity Flip)
// =====================================================================
enum JumpKind : uint8_t { JMP_DINO = 0, JMP_PLATFORM, JMP_GRAVITY };

void runJumpRunner(GamerEngine& engine, const char* title, const char* key, JumpKind kind) {
  const uint16_t best = bestLoad(key);

  while (true) {
    const char* help = kind == JMP_GRAVITY ? "SEL flips gravity"
                       : kind == JMP_PLATFORM ? "SEL jumps platforms"
                                              : "SEL jump  DOWN duck";
    uint8_t diff = chooseDifficulty(engine, title, help);
    if (diff == 255) return;

    const int8_t baseSpeed[3] = {3, 4, 5};

    int16_t y = (kind == JMP_GRAVITY) ? 48 : 49;
    int16_t vy = 0;
    int8_t gravity = 1;
    int16_t hazardX = 128;
    uint8_t hazardType = 0;       // 0 ground, 1 flying (dino), platform height idx
    const int16_t platY = 49;     // ground / run surface
    uint16_t score = 0;
    uint8_t lives = 3;
    uint8_t shake = 0, lastTier = 0;
    uint32_t lastFlip = 0;
    uint8_t duckTimer = 0;        // frames remaining ducked
    uint32_t nextFrame = 0;
    SparkField sparks; clearSparks(sparks);

    bool alive = true;
    while (alive) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }

      // Dino: DOWN press starts a short duck window (re-press to keep ducking).
      if (kind == JMP_DINO && engine.wasDirectionPressed(GAMER_DIR_DOWN) && y >= platY - 1)
        duckTimer = 8;
      bool ducking = (kind == JMP_DINO) && duckTimer > 0;

      if (selectTap(engine)) {
        if (kind == JMP_GRAVITY) {
          gravity = -gravity;
          engine.ledPulse(CRGB::Purple, 70);
          engine.playSound(SOUND_JUMP);
          uint32_t now = millis();
          if (now - lastFlip < 360) {  // quick double-flip bonus
            score += 1; flipCount++;
            engine.playSound(SOUND_COMBO);
          }
          lastFlip = now;
        } else if (y >= platY - 1) {
          vy = (kind == JMP_PLATFORM) ? -10 : -9;
          engine.ledPulse(CRGB::Green, 50);
          engine.playSound(SOUND_JUMP);
        }
      }

      if (!frameDue(nextFrame, 34)) { delay(2); continue; }

      if (duckTimer) duckTimer--;

      if (kind == JMP_GRAVITY) {
        y += gravity * 3;
        y = constrain(y, 7, 49);
      } else {
        vy++;
        y += vy;
        if (y > platY) { y = platY; vy = 0; }
      }

      hazardX -= baseSpeed[diff] + score / 18;
      if (hazardX < -12) {
        hazardX = 128 + random(0, 26);
        score++;
        if (kind == JMP_DINO) {
          hazardType = (score > 3 && random(0, 3) == 0) ? 1 : 0;  // flying bird
        } else if (kind == JMP_PLATFORM) {
          // Block of varying height; some are bounce pads (clear/launch you).
          hazardType = (random(0, 4) == 0) ? 2 : (uint8_t)random(0, 3);  // 2=bouncy
        }
        engine.ledPulse(CRGB::Blue, 40);
        engine.playSound(SOUND_SCORE);
      }

      uint8_t tier = score / 14;
      if (tier != lastTier) { lastTier = tier; levelUp(engine); }

      // Hazard Y/H by kind.
      int16_t hzY, hzH = 12, hzW = 7;
      if (kind == JMP_GRAVITY) { hzY = gravity > 0 ? 52 : 7; hzH = 6; }
      else if (kind == JMP_DINO && hazardType == 1) { hzY = 34; hzH = 6; hzW = 9; }
      else if (kind == JMP_PLATFORM) {
        // Block height grows with type (0 short .. 2 tall/bouncy).
        hzH = 8 + hazardType * 5; hzW = 8; hzY = 58 - hzH;
      } else hzY = 46;

      int16_t heroH = ducking ? 5 : 9;
      int16_t heroY = ducking ? y + 4 : y;
      bool hit = rectsOverlap(16, heroY, 8, heroH, hazardX, hzY, hzW, hzH);
      // Platform bounce pad: landing on top of a bouncy block launches you.
      if (kind == JMP_PLATFORM && hazardType == 2 && hit && vy > 0 && heroY + heroH <= hzY + 5) {
        vy = -12; hit = false;
        engine.playSound(SOUND_POWERUP);
        engine.ledPulse(CRGB::Green, 60);
      }

      if (hit) {
        lives--;
        shake = 4;
        engine.ledPulse(CRGB::Red, 120);
        engine.playSound(SOUND_HIT);
        sparksSpawn(sparks, gameX(engine, 18), gameY(engine, y), COL_RED, 8);
        hazardX = 128 + 20;
        if (lives == 0) alive = false;
      }

      // --- draw ---
      int16_t ox = shakeOffset(shake), oy = shakeOffset(shake);
      if (shake) shake--;
      engine.clear();
      engine.screen().drawFastHLine(0, gameY(engine, 58 + oy), engine.width(), GAMER_WHITE);
      if (kind == JMP_GRAVITY)
        engine.screen().drawFastHLine(0, gameY(engine, 6 + oy), engine.width(), GAMER_WHITE);

      // hero
      engine.screen().drawRect(gameX(engine, 16 + ox), gameY(engine, heroY + oy),
                               gameSize(engine, ducking ? 9 : 7),
                               gameSize(engine, ducking ? 5 : 9),
                               kind == JMP_GRAVITY ? (gravity > 0 ? COL_GREEN : COL_MAG)
                                                   : GAMER_WHITE);
      // hazard / platform block
      uint16_t hzCol = (kind == JMP_DINO && hazardType == 1) ? COL_GOLD
                       : (kind == JMP_PLATFORM && hazardType == 2) ? COL_GREEN
                                                                   : GAMER_ACCENT;
      engine.screen().fillRect(gameX(engine, hazardX + ox), gameY(engine, hzY + oy),
                               gameSize(engine, hzW), gameSize(engine, hzH), hzCol);

      sparksUpdateDraw(sparks, engine);
      drawHud(engine, score, best, lives);
      engine.show();
    }

    bool record = bestSubmit(key, score);
    engine.ledPulse(record ? CRGB::Green : CRGB::Red, 260);
    engine.playSound(record ? SOUND_WIN : SOUND_LOSE);
    if (!resultScreenBest(engine, "GAME OVER", score, best, record)) return;
  }
}

// =====================================================================
//  CATCH / DODGE FAMILY
//  Catch Star / Basket / Balloon / Dodge Rain / Wall Bounce / Brick Drop
// =====================================================================
enum CatchKind : uint8_t {
  CAT_STAR = 0, CAT_BASKET, CAT_BALLOON, CAT_DODGE, CAT_WALL, CAT_BRICK
};

void runCatchOrDodge(GamerEngine& engine, const char* title, const char* key, CatchKind kind) {
  const uint16_t best = bestLoad(key);
  const bool dodgeMode = (kind == CAT_DODGE);

  while (true) {
    uint8_t diff = chooseDifficulty(engine, title,
                                    dodgeMode ? "Avoid the drops" : "L/R to catch");
    if (diff == 255) return;

    const int8_t baseSpeed[3] = {2, 3, 4};

    int16_t playerX = 58;
    // Up to 4 simultaneous items (fixed array).
    const uint8_t MAXI = 4;
    int16_t ix[MAXI], iy[MAXI];
    int8_t ivx[MAXI];        // wall-bounce horizontal drift
    uint8_t itype[MAXI];     // tier / color / bomb / point-value
    bool ialive[MAXI];
    uint8_t activeCount = (kind == CAT_DODGE) ? 1 : 1;
    for (uint8_t i = 0; i < MAXI; ++i) {
      ialive[i] = i < activeCount;
      ix[i] = random(8, 116); iy[i] = -8 - i * 18;
      ivx[i] = random(0, 2) ? 1 : -1;
      itype[i] = random(0, 4);
    }
    uint16_t score = 0;
    uint8_t lives = 3;
    uint8_t combo = 0;          // catch combo multiplier
    uint8_t shield = 0;         // dodge shield charges
    uint8_t stack = 0;          // brick stack height
    uint8_t stackColor = 9;     // current stack color (brick)
    uint8_t speed = baseSpeed[diff];
    uint8_t shake = 0, lastTier = 0;
    uint32_t nextFrame = 0;
    SparkField sparks; clearSparks(sparks);

    bool alive = true;
    while (alive) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (!frameDue(nextFrame, 40)) { delay(2); continue; }

      playerX += heldAxis(engine) * 4;
      playerX = constrain(playerX, 0, 116);

      // Difficulty/score curve: more simultaneous items, faster fall.
      activeCount = 1 + score / 12;
      if (activeCount > MAXI) activeCount = MAXI;
      if (dodgeMode && activeCount < 2 && score > 6) activeCount = 2;
      speed = baseSpeed[diff] + score / 14;
      if (speed > 7) speed = 7;

      uint8_t tier = score / 12;
      if (tier != lastTier) { lastTier = tier; levelUp(engine); }

      for (uint8_t i = 0; i < MAXI; ++i) {
        if (i >= activeCount) { ialive[i] = false; continue; }
        if (!ialive[i]) {
          // respawn slot
          ialive[i] = true;
          ix[i] = random(8, 116); iy[i] = -8;
          ivx[i] = random(0, 2) ? 1 : -1;
          itype[i] = random(0, 4);
          continue;
        }
        iy[i] += speed + (kind == CAT_BASKET ? (itype[i] == 0 ? 1 : 0) : 0);
        if (kind == CAT_WALL) {
          ix[i] += ivx[i];
          if (ix[i] < 2 || ix[i] > 114) ivx[i] = -ivx[i];
        }

        bool caught = rectsOverlap(playerX, 56, 12, 5, ix[i], iy[i], 6, 6);
        bool fell = iy[i] > 64;

        if (caught) {
          if (dodgeMode) {
            if (shield) { shield--; engine.playSound(SOUND_POWERUP); }
            else {
              lives--; shake = 4; combo = 0;
              engine.playSound(SOUND_HIT);
              engine.ledPulse(CRGB::Red, 120);
              sparksSpawn(sparks, gameX(engine, ix[i]), gameY(engine, iy[i]), COL_RED, 8);
              if (lives == 0) { alive = false; break; }
            }
            ialive[i] = false;
            continue;
          }
          // ----- catch scoring per game -----
          int16_t pts = 1;
          uint16_t fx = gameX(engine, ix[i]), fy = gameY(engine, iy[i]);
          switch (kind) {
            case CAT_STAR: {
              // gold/silver/platinum tiers.
              pts = (itype[i] == 0) ? 1 : (itype[i] == 1 ? 2 : 3);
              combo++;
              pts += combo / 4;  // combo multiplier
              sparksSpawn(sparks, fx, fy, itype[i] >= 2 ? GAMER_WHITE : COL_GOLD, 6);
              break;
            }
            case CAT_BASKET: {
              // weighted balls: small=fast few pts, big=heavy more pts.
              pts = (itype[i] == 0) ? 1 : (itype[i] == 1 ? 2 : 3);
              // big ball bounces once before counting feel: extra spark
              sparksSpawn(sparks, fx, fy, GAMER_ACCENT, 5);
              break;
            }
            case CAT_BALLOON: {
              if (itype[i] == 3) {  // BOMB balloon: lose points + life
                pts = -2;
                lives = lives > 1 ? lives - 1 : 1;
                shake = 4; combo = 0;
                engine.playSound(SOUND_HIT);
                sparksSpawn(sparks, fx, fy, COL_RED, 10);
              } else {
                pts = 1; combo++;
                uint16_t c = itype[i] == 0 ? COL_RED : itype[i] == 1 ? COL_GREEN : COL_MAG;
                sparksSpawn(sparks, fx, fy, c, 8);  // pop particles
              }
              break;
            }
            case CAT_WALL: {
              // angled catch bonus: faster horizontal = more points.
              pts = 1 + (abs(ivx[i]) > 1 ? 1 : 0);
              combo++;
              sparksSpawn(sparks, fx, fy, GAMER_ACCENT, 5);
              break;
            }
            case CAT_BRICK: {
              // build a stack; matching color clears it for a bonus.
              uint8_t c = itype[i] % 3;
              if (stack > 0 && c == stackColor) {
                pts = 2 + stack;            // clear bonus
                stack = 0; stackColor = 9;
                engine.playSound(SOUND_COMBO);
                sparksSpawn(sparks, fx, fy, COL_GOLD, 10);
              } else {
                pts = 1; stack++; stackColor = c;
                sparksSpawn(sparks, fx, fy, GAMER_ACCENT, 4);
                if (stack >= 6) {            // overflow risk
                  lives--; stack = 0; shake = 4;
                  engine.playSound(SOUND_HIT);
                  if (lives == 0) { alive = false; }
                }
              }
              break;
            }
            default: break;
          }
          if (pts > 0) {
            if ((kind == CAT_STAR || kind == CAT_BALLOON || kind == CAT_WALL) &&
                combo > 0 && combo % 5 == 0)
              engine.playSound(SOUND_COMBO);
            engine.ledPulse(CRGB::Green, 60);
            engine.playSound(SOUND_SCORE);
          }
          score = (pts < 0 && (uint16_t)(-pts) > score) ? 0 : (uint16_t)(score + pts);
          ialive[i] = false;
          continue;
        }

        if (fell) {
          if (!dodgeMode) {
            // Missing catches costs a life (combo resets).
            combo = 0;
            lives--;
            shake = 3;
            engine.playSound(SOUND_LOSE);
            engine.ledPulse(CRGB::Orange, 90);
            if (lives == 0) { alive = false; break; }
          } else {
            score++;  // survived a drop
            engine.playSound(SOUND_SCORE);
          }
          ialive[i] = false;
        }
      }
      if (!alive) {
        // fall through to draw once more not needed
      }

      // Occasional pickups: dodge shield / star powerup.
      // (folded into item types via a rare slot for dodge)
      if (dodgeMode && shield == 0 && score > 0 && score % 15 == 0 && random(0, 40) == 0)
        shield = 1;

      // --- draw ---
      int16_t ox = shakeOffset(shake), oy = shakeOffset(shake);
      if (shake) shake--;
      engine.clear();
      // player paddle / basket
      engine.screen().drawRect(gameX(engine, playerX + ox), gameY(engine, 56 + oy),
                               gameSize(engine, 12), gameSize(engine, 5),
                               shield ? COL_GREEN : GAMER_WHITE);
      if (kind == CAT_BRICK && stack)  // show carried stack
        for (uint8_t s = 0; s < stack; ++s)
          engine.screen().fillRect(gameX(engine, playerX + 3 + ox),
                                   gameY(engine, 54 - s * 3 + oy),
                                   gameSize(engine, 6), gameSize(engine, 2),
                                   stackColor == 0 ? COL_RED : stackColor == 1 ? COL_GREEN : COL_MAG);

      for (uint8_t i = 0; i < activeCount; ++i) {
        if (!ialive[i]) continue;
        int16_t dx = gameX(engine, ix[i] + ox), dy = gameY(engine, iy[i] + oy);
        switch (kind) {
          case CAT_STAR: {
            uint16_t c = itype[i] == 0 ? COL_GOLD : itype[i] == 1 ? GAMER_DIM : GAMER_WHITE;
            engine.screen().drawLine(dx, dy + gameSize(engine, 3), dx + gameSize(engine, 6),
                                     dy + gameSize(engine, 3), c);
            engine.screen().drawLine(dx + gameSize(engine, 3), dy, dx + gameSize(engine, 3),
                                     dy + gameSize(engine, 6), c);
            break;
          }
          case CAT_BASKET: {
            int16_t r = itype[i] == 0 ? 2 : itype[i] == 1 ? 3 : 4;
            engine.screen().fillCircle(dx + gameSize(engine, 3), dy + gameSize(engine, 3),
                                       gameSize(engine, r), GAMER_ACCENT);
            break;
          }
          case CAT_BALLOON: {
            uint16_t c = itype[i] == 3 ? COL_RED
                         : itype[i] == 0 ? GAMER_ACCENT
                         : itype[i] == 1 ? COL_GREEN : COL_MAG;
            engine.screen().fillCircle(dx + gameSize(engine, 3), dy + gameSize(engine, 3),
                                       gameSize(engine, 3), c);
            if (itype[i] == 3)  // bomb mark
              engine.screen().drawPixel(dx + gameSize(engine, 3), dy, GAMER_WHITE);
            break;
          }
          case CAT_DODGE: {
            // heavier drops at higher score drawn larger.
            int16_t r = score > 20 ? 3 : 2;
            engine.screen().fillCircle(dx + gameSize(engine, 3), dy + gameSize(engine, 3),
                                       gameSize(engine, r), GAMER_ACCENT);
            break;
          }
          case CAT_WALL:
            engine.screen().fillRect(dx, dy, gameSize(engine, 6), gameSize(engine, 6),
                                     GAMER_ACCENT);
            break;
          case CAT_BRICK: {
            uint8_t c = itype[i] % 3;
            engine.screen().fillRect(dx, dy, gameSize(engine, 6), gameSize(engine, 5),
                                     c == 0 ? COL_RED : c == 1 ? COL_GREEN : COL_MAG);
            break;
          }
        }
      }

      sparksUpdateDraw(sparks, engine);
      drawHud(engine, score, best, lives);
      if (combo >= 4 && !dodgeMode) {
        char cb[10]; snprintf(cb, sizeof(cb), "x%u", (unsigned)(1 + combo / 4));
        engine.rightText(cb, gameY(engine, 9), COL_GOLD);
      }
      engine.show();
    }

    bool record = bestSubmit(key, score);
    engine.ledPulse(record ? CRGB::Green : CRGB::Red, 260);
    engine.playSound(record ? SOUND_WIN : SOUND_LOSE);
    if (!resultScreenBest(engine, "GAME OVER", score, best, record)) return;
  }
}

// =====================================================================
//  LANE FAMILY
//  Lane Racer / Traffic / Ski / Boat / Rail / Road Drift
// =====================================================================
enum LaneKind : uint8_t {
  LANE_RACER = 0, LANE_TRAFFIC, LANE_SKI, LANE_BOAT, LANE_RAIL, LANE_DRIFT
};

void runLaneGame(GamerEngine& engine, const char* title, const char* key, LaneKind kind) {
  const uint16_t best = bestLoad(key);
  const bool gates = (kind == LANE_SKI || kind == LANE_BOAT);

  while (true) {
    const char* help = kind == LANE_DRIFT ? "L/R + SEL to drift"
                       : kind == LANE_BOAT ? "Steer vs current"
                       : kind == LANE_RAIL ? "Two moves per row"
                                           : "L/R change lanes";
    uint8_t diff = chooseDifficulty(engine, title, help);
    if (diff == 255) return;

    const int8_t baseSpeed[3] = {3, 4, 5};

    int8_t lane = 1;
    int16_t y = -10, y2 = -40;     // second (rail coupled) row
    int8_t badLane = random(0, 3);
    int8_t badLane2 = random(0, 3); // rail second hazard
    int16_t gateW = 22;            // ski gate width (narrows)
    int16_t current = 0;           // boat sideways drift accumulator
    int8_t currentDir = random(0, 2) ? 1 : -1;
    bool pickupRow = false;        // boat buoy / drift turbo
    int8_t pickupLane = 0;
    uint16_t score = 0;
    uint8_t lives = 3;
    uint8_t turbo = 0;             // road-drift turbo charges
    uint8_t shake = 0, lastTier = 0;
    int8_t laneOffset = 0;         // sub-lane drift visual for boat/drift
    uint8_t speed = baseSpeed[diff];
    uint32_t nextFrame = 0;
    SparkField sparks; clearSparks(sparks);

    bool alive = true;
    while (alive) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }

      bool driftMove = false;
      if (engine.wasPressed(BTN_LEFT)) {
        if (kind == LANE_DRIFT && engine.isHeld(BTN_SELECT) && lane > 0) {
          lane--; driftMove = true; turbo = turbo;  // drift slip
        } else if (lane > 0) lane--;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT)) {
        if (kind == LANE_DRIFT && engine.isHeld(BTN_SELECT) && lane < 2) {
          lane++; driftMove = true;
        } else if (lane < 2) lane++;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (kind == LANE_DRIFT && driftMove) {
        engine.ledPulse(CRGB::Aqua, 60);
        sparksSpawn(sparks, gameX(engine, 17 + lane * 42), gameY(engine, 54), GAMER_ACCENT, 5);
      }

      if (!frameDue(nextFrame, kind == LANE_BOAT ? 46 : 42)) { delay(2); continue; }

      speed = baseSpeed[diff] + score / 12;
      if (turbo) speed += 2;
      if (speed > 8) speed = 8;

      // Boat current pushes the player sideways; steer against it.
      if (kind == LANE_BOAT) {
        current += currentDir;
        if (current > 8) { current = 8; }
        if (current < -8) { current = -8; }
        laneOffset = current;
      }

      y += speed;
      if (kind == LANE_RAIL) y2 += speed;

      if (y > 70) {
        y = -10;
        badLane = random(0, 3);
        score++;
        if (kind == LANE_BOAT) { currentDir = random(0, 2) ? 1 : -1; current = 0; }
        if (kind == LANE_SKI) { gateW -= 1; if (gateW < 12) gateW = 12; }
        // pickups: boat buoy, drift turbo.
        if ((kind == LANE_BOAT || kind == LANE_DRIFT) && random(0, 3) == 0) {
          pickupRow = true; pickupLane = random(0, 3);
        } else pickupRow = false;
        engine.ledPulse(CRGB::Blue, 40);
        engine.playSound(SOUND_SCORE);
      }
      if (kind == LANE_RAIL && y2 > 70) {
        y2 = -10; badLane2 = random(0, 3);
      }

      uint8_t tier = score / 14;
      if (tier != lastTier) { lastTier = tier; levelUp(engine); }

      // Pickup collect.
      if (pickupRow && y > 48 && y < 60 && lane == pickupLane) {
        pickupRow = false;
        if (kind == LANE_DRIFT) { turbo = 3; engine.playSound(SOUND_POWERUP); }
        else { score += 2; engine.playSound(SOUND_POWERUP); }  // boat buoy bonus
        engine.ledPulse(CRGB::Green, 70);
        sparksSpawn(sparks, gameX(engine, 17 + lane * 42), gameY(engine, 54), COL_GOLD, 6);
      }
      if (turbo && y > 48 && y < 60) { /* turbo ticks down per row */ }

      // --- collisions ---
      bool crash = false;
      bool centeredBonus = false;
      if (gates) {
        // badLane marks the OPEN gate (gap); you must pass through it.
        if (y > 48 && y < 62 && lane != badLane) crash = true;
        // Ski: clearing the gate while it sits in the center lane is a bonus.
        if (kind == LANE_SKI && y > 48 && y < 62 && lane == badLane && badLane == 1)
          centeredBonus = true;
      } else {
        if (y > 46 && y < 62 && lane == badLane) crash = true;
        if (kind == LANE_RAIL && y2 > 46 && y2 < 62 && lane == badLane2) crash = true;
      }

      if (centeredBonus && y > 49 && y < 51) {
        score++;  // ski centered-gate bonus
        engine.playSound(SOUND_COMBO);
      }

      if (crash) {
        lives--;
        shake = 4;
        engine.ledPulse(CRGB::Red, 120);
        engine.playSound(SOUND_HIT);
        sparksSpawn(sparks, gameX(engine, 17 + lane * 42), gameY(engine, 54), COL_RED, 8);
        y = -10; y2 = -40;
        badLane = random(0, 3); badLane2 = random(0, 3);
        if (turbo) turbo = 0;
        if (lives == 0) alive = false;
      }
      if (turbo && y == -10) turbo--;  // decay per cleared row

      // --- draw ---
      int16_t ox = shakeOffset(shake), oy = shakeOffset(shake);
      if (shake) shake--;
      engine.clear();
      for (uint8_t i = 1; i < 3; i++)
        engine.screen().drawFastVLine(gameX(engine, i * 42 + ox), gameY(engine, 8),
                                      gameY(engine, 56), GAMER_DIM);

      int16_t playerX = 17 + lane * 42 + (kind == LANE_BOAT ? laneOffset : 0);
      playerX = constrain(playerX, 2, 118);
      // player sprite varies by theme.
      engine.screen().drawRect(gameX(engine, playerX + ox), gameY(engine, 54 + oy),
                               gameSize(engine, 8),
                               gameSize(engine, kind == LANE_BOAT ? 9 : 8),
                               turbo ? COL_GOLD : GAMER_WHITE);

      if (gates) {
        // draw open gates (all lanes except the closed badLane).
        int16_t gw = (kind == LANE_SKI) ? gateW : 20;
        for (uint8_t i = 0; i < 3; i++)
          if (i != badLane)
            engine.screen().fillRect(gameX(engine, 17 + i * 42 - gw / 2 + 4 + ox),
                                     gameY(engine, y + oy),
                                     gameSize(engine, gw), gameSize(engine, 4), GAMER_ACCENT);
      } else {
        // obstacle car/rock; rail draws a second coupled hazard.
        int16_t objX = 17 + badLane * 42;
        // varying widths/speeds at higher score -> wider hazard sometimes.
        int16_t w = (kind == LANE_TRAFFIC || kind == LANE_RACER) && score > 10 && (badLane == 1)
                        ? 10 : 8;
        engine.screen().fillRect(gameX(engine, objX + ox), gameY(engine, y + oy),
                                 gameSize(engine, w), gameSize(engine, 10), GAMER_ACCENT);
        if (kind == LANE_RAIL) {
          int16_t o2 = 17 + badLane2 * 42;
          engine.screen().fillRect(gameX(engine, o2 + ox), gameY(engine, y2 + oy),
                                   gameSize(engine, 8), gameSize(engine, 10), COL_MAG);
        }
      }

      if (pickupRow) {
        int16_t px = 17 + pickupLane * 42;
        engine.screen().fillCircle(gameX(engine, px + 4 + ox), gameY(engine, y + 4 + oy),
                                   gameSize(engine, 3),
                                   kind == LANE_DRIFT ? COL_GREEN : COL_GOLD);
      }

      // boat current indicator
      if (kind == LANE_BOAT) {
        const char* arrow = currentDir > 0 ? ">>" : "<<";
        engine.centerText(arrow, gameY(engine, 1), 1, GAMER_DIM);
      }

      sparksUpdateDraw(sparks, engine);
      drawHud(engine, score, best, lives);
      engine.show();
    }

    bool record = bestSubmit(key, score);
    engine.ledPulse(record ? CRGB::Green : CRGB::Red, 260);
    engine.playSound(record ? SOUND_WIN : SOUND_LOSE);
    if (!resultScreenBest(engine, "GAME OVER", score, best, record)) return;
  }
}

// =====================================================================
//  BREAKOUT  (standalone, deepened)
// =====================================================================
void runBreakoutInternal(GamerEngine& engine) {
  const uint16_t best = bestLoad("breakout");

  while (true) {
    uint8_t diff = chooseDifficulty(engine, "BREAKOUT", "L/R move paddle");
    if (diff == 255) return;

    const int8_t ballSpeed[3] = {2, 2, 3};
    uint8_t level = 1;
    uint16_t score = 0;
    uint8_t lives = 3;

    bool running = true;
    while (running) {
      bool bricks[4][8];
      for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 8; c++) bricks[r][c] = true;
      uint8_t left = 32;
      uint8_t remaining = 32;
      int16_t ballX = 64, ballY = 42;
      int8_t spd = ballSpeed[diff] + (level - 1);
      if (spd > 4) spd = 4;
      int8_t vx = random(0, 2) ? spd : -spd;
      int8_t vy = -spd;
      uint8_t shake = 0;
      uint32_t nextFrame = 0;
      SparkField sparks; clearSparks(sparks);

      bool ballLost = false, cleared = false;
      while (!ballLost && !cleared) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
        if (!frameDue(nextFrame, 26)) { delay(2); continue; }

        int16_t nextLeft = left + heldAxis(engine) * 4;
        left = constrain(nextLeft, 0, 104);
        ballX += vx; ballY += vy;
        if (ballX <= 0 || ballX >= 124) vx = -vx;
        if (ballY <= 0) vy = abs(vy);
        if (rectsOverlap(ballX, ballY, 4, 4, left, 58, 24, 4)) {
          vy = -abs(vy);
          vx += (ballX - (left + 12)) / 8;
          vx = constrain(vx, -4, 4);
          if (vx == 0) vx = 1;
          engine.playSound(SOUND_HIT);
        }
        for (uint8_t r = 0; r < 4; r++)
          for (uint8_t c = 0; c < 8; c++)
            if (bricks[r][c] && rectsOverlap(ballX, ballY, 4, 4, c * 16, 10 + r * 7, 14, 5)) {
              bricks[r][c] = false;
              remaining--;
              score += 5;
              vy = -vy;
              engine.ledPulse(CRGB::Aqua, 50);
              engine.playSound(SOUND_SCORE);
              sparksSpawn(sparks, gameX(engine, c * 16 + 7), gameY(engine, 12 + r * 7),
                          (r % 2) ? COL_GOLD : GAMER_ACCENT, 5);
            }

        int16_t ox = shakeOffset(shake), oy = shakeOffset(shake);
        if (shake) shake--;
        engine.clear();
        for (uint8_t r = 0; r < 4; r++)
          for (uint8_t c = 0; c < 8; c++)
            if (bricks[r][c])
              engine.screen().fillRect(gameX(engine, c * 16 + ox), gameY(engine, 10 + r * 7 + oy),
                                       gameSize(engine, 14), gameSize(engine, 5),
                                       (r % 2) ? GAMER_ACCENT : COL_MAG);
        engine.screen().fillRect(gameX(engine, left + ox), gameY(engine, 58 + oy),
                                 gameSize(engine, 24), gameSize(engine, 4), GAMER_WHITE);
        engine.screen().fillRect(gameX(engine, ballX + ox), gameY(engine, ballY + oy),
                                 gameSize(engine, 4), gameSize(engine, 4), GAMER_WHITE);
        sparksUpdateDraw(sparks, engine);
        drawHud(engine, score, best, lives);
        engine.show();

        if (ballY > 64) ballLost = true;
        if (remaining == 0) cleared = true;
      }

      if (cleared) {
        level++;
        score += 20;
        levelUp(engine);
        engine.playSound(SOUND_WIN);
      } else {
        lives--;
        engine.ledPulse(CRGB::Red, 200);
        engine.playSound(SOUND_LOSE);
        if (lives == 0) running = false;
      }
    }

    bool record = bestSubmit("breakout", score);
    engine.ledPulse(record ? CRGB::Green : CRGB::Red, 260);
    if (!resultScreenBest(engine, "GAME OVER", score, best, record)) return;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Public runners (registered in Games.cpp). Signatures unchanged.
// ---------------------------------------------------------------------------
void runBreakout(GamerEngine& engine) { runBreakoutInternal(engine); }
void runFlappyPico(GamerEngine& engine) { runFlyer(engine, "FLAPPY PICO", "flappy", FLY_FLAPPY); }
void runDinoRunner(GamerEngine& engine) { runJumpRunner(engine, "DINO RUNNER", "dino", JMP_DINO); }
void runJetpackRunner(GamerEngine& engine) { runFlyer(engine, "JETPACK", "jetpack", FLY_JETPACK); }
void runDodgeRain(GamerEngine& engine) { runCatchOrDodge(engine, "DODGE RAIN", "dodgerain", CAT_DODGE); }
void runCatchStar(GamerEngine& engine) { runCatchOrDodge(engine, "CATCH STAR", "catchstar", CAT_STAR); }
void runBasketCatch(GamerEngine& engine) { runCatchOrDodge(engine, "BASKET CATCH", "basket", CAT_BASKET); }
void runBalloonPop(GamerEngine& engine) { runCatchOrDodge(engine, "BALLOON POP", "balloon", CAT_BALLOON); }
void runCaveFlyer(GamerEngine& engine) { runFlyer(engine, "CAVE FLYER", "caveflyer", FLY_CAVE); }
void runTunnelRunner(GamerEngine& engine) { runFlyer(engine, "TUNNEL RUN", "tunnel", FLY_TUNNEL); }
void runWallBounce(GamerEngine& engine) { runCatchOrDodge(engine, "WALL BOUNCE", "wallbounce", CAT_WALL); }
void runGravityFlip(GamerEngine& engine) { runJumpRunner(engine, "GRAVITY FLIP", "gravflip", JMP_GRAVITY); }
void runPlatformHopper(GamerEngine& engine) { runJumpRunner(engine, "PLATFORM HOP", "plathop", JMP_PLATFORM); }
void runBrickDrop(GamerEngine& engine) { runCatchOrDodge(engine, "BRICK DROP", "brickdrop", CAT_BRICK); }
void runLaneRacer(GamerEngine& engine) { runLaneGame(engine, "LANE RACER", "lanerace", LANE_RACER); }
void runTrafficDodge(GamerEngine& engine) { runLaneGame(engine, "TRAFFIC", "traffic", LANE_TRAFFIC); }
void runSkiSlalom(GamerEngine& engine) { runLaneGame(engine, "SKI SLALOM", "ski", LANE_SKI); }
void runBoatSlalom(GamerEngine& engine) { runLaneGame(engine, "BOAT SLALOM", "boat", LANE_BOAT); }
void runRailRunner(GamerEngine& engine) { runLaneGame(engine, "RAIL RUNNER", "rail", LANE_RAIL); }
void runRoadDrift(GamerEngine& engine) { runLaneGame(engine, "ROAD DRIFT", "roaddrift", LANE_DRIFT); }
