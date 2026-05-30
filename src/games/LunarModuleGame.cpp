#include "Games.h"
#include "GameUtils.h"

// ---------------------------------------------------------------------------
// LUNAR MODULE  (deepened)
//
// A lander skill game. The world is reasoned about in logical 128x64 OLED
// coordinates and scaled to the physical panel with gameX/gameY/gameSize, so
// it looks identical on every Cardputer variant.
//
// Physics runs in fixed point (1/16 sub-pixel) so the lander drifts and falls
// smoothly even at the chunky integer resolution. A fixed terrain heightmap
// (no heap, regenerated in place each level) carves flat landing pads of
// varying width: the narrower the pad, the higher its score multiplier.
//
// Depth: fuel gauge (no thrust when empty -> must coast), sideways wind that
// pushes the lander with an on-screen indicator, difficulty that tunes
// gravity / starting fuel / wind, impact-velocity landing ratings
// (PERFECT / GOOD / HARD) with pad-multiplier bonuses, an accumulating
// multi-level progression, and a persistent best score.
// ---------------------------------------------------------------------------

namespace {

constexpr int16_t kFP = 16;          // fixed-point fraction
constexpr int16_t kCols = 32;        // terrain columns across 128 logical px
constexpr int16_t kColW = 128 / kCols;  // 4 logical px per column
constexpr int16_t kGroundTop = 64;   // logical bottom

// Terrain stored as a height (top-of-rock Y, logical) per column. Pads are
// runs of equal height flagged with a multiplier.
struct World {
  uint8_t height[kCols];   // logical Y of terrain top per column
  uint8_t padMul[kCols];   // 0 = rough terrain, >0 = landing pad multiplier
};

// Carve a level: jagged ridgeline with a few flat pads. `level` increases
// roughness and shrinks pads. Deterministic-ish via randomSeed in caller.
void buildWorld(World& w, uint8_t level) {
  // Base rolling ridge.
  int16_t h = random(40, 50);
  for (int16_t c = 0; c < kCols; ++c) {
    h += random(-3 - level, 4 + level);
    if (h < 30) h = 30;
    if (h > 60) h = 60;
    w.height[c] = static_cast<uint8_t>(h);
    w.padMul[c] = 0;
  }

  // Place 2..3 pads. Narrower pad => bigger multiplier. Higher levels get
  // smaller pads. Pads are spaced out and flattened.
  uint8_t padCount = level >= 3 ? 3 : 2;
  for (uint8_t p = 0; p < padCount; ++p) {
    // widths shrink with level: wide=5 cols, mid=4, narrow=3/2
    int8_t maxW = 5 - (level / 2);
    if (maxW < 2) maxW = 2;
    int8_t padW = static_cast<int8_t>(random(2, maxW + 1));
    // segment the field so pads don't overlap
    int16_t segLo = 2 + (kCols - 4) * p / padCount;
    int16_t segHi = 2 + (kCols - 4) * (p + 1) / padCount - padW;
    if (segHi <= segLo) segHi = segLo + 1;
    int16_t start = random(segLo, segHi + 1);
    int16_t flatY = random(38, 56);
    // multiplier: 2 cols ->x4, 3 ->x3, 4 ->x2, 5 ->x1 (+level kicker)
    uint8_t mul = static_cast<uint8_t>(6 - padW);
    if (mul < 1) mul = 1;
    for (int8_t i = 0; i < padW && start + i < kCols; ++i) {
      w.height[start + i] = static_cast<uint8_t>(flatY);
      w.padMul[start + i] = mul;
    }
  }
}

// Terrain top (logical Y) at a logical x.
int16_t terrainAt(const World& w, int16_t lx) {
  int16_t c = lx / kColW;
  if (c < 0) c = 0;
  if (c >= kCols) c = kCols - 1;
  return w.height[c];
}

uint8_t padMulAt(const World& w, int16_t lx) {
  int16_t c = lx / kColW;
  if (c < 0) c = 0;
  if (c >= kCols) c = kCols - 1;
  return w.padMul[c];
}

// Draw the terrain (filled) and highlight pads. ox/oy are logical shake.
void drawWorld(GamerEngine& engine, const World& w, int16_t ox, int16_t oy) {
  for (int16_t c = 0; c < kCols; ++c) {
    int16_t lx = c * kColW + ox;
    int16_t top = w.height[c] + oy;
    int16_t px = gameX(engine, lx);
    int16_t pw = max<int16_t>(1, gameX(engine, kColW));
    int16_t py = gameY(engine, top);
    int16_t ph = engine.height() - py;
    if (ph > 0) {
      engine.screen().fillRect(px, py, pw, ph, GAMER_DIM);
    }
    if (w.padMul[c]) {
      // Bright pad cap with a marker.
      engine.screen().fillRect(px, py, pw, max<int16_t>(1, gameSize(engine, 1)),
                               TFT_GREEN);
    } else {
      engine.screen().drawFastHLine(px, py, pw, GAMER_WHITE);
    }
  }
}

// Compact lander sprite at logical x,y (top-left). Returns nothing.
void drawLander(GamerEngine& engine, int16_t lx, int16_t ly, bool thrust,
                bool tilt) {
  const int16_t s = gameSize(engine, 1);
  int16_t x = gameX(engine, lx);
  int16_t y = gameY(engine, ly);
  // body 6x4 logical
  engine.screen().fillRect(x + s, y + s, s * 4, s * 2, GAMER_WHITE);
  engine.screen().fillRect(x + s * 2, y, s * 2, s, GAMER_ACCENT);  // cockpit
  // legs
  engine.screen().drawLine(x + s, y + s * 3, x, y + s * 5, GAMER_WHITE);
  engine.screen().drawLine(x + s * 5, y + s * 3, x + s * 6, y + s * 5, GAMER_WHITE);
  engine.screen().drawFastHLine(x, y + s * 5, s, GAMER_WHITE);
  engine.screen().drawFastHLine(x + s * 6, y + s * 5, s, GAMER_WHITE);
  if (thrust) {
    int16_t fx = x + s * 3 + (tilt ? shakeOffset(s) : 0);
    engine.screen().drawTriangle(x + s * 2, y + s * 3, x + s * 4, y + s * 3,
                                 fx, y + s * 3 + s * 3, TFT_ORANGE);
  }
}

}  // namespace

void runLunarModule(GamerEngine& engine) {
  const uint16_t best = bestLoad("lunar");

  while (true) {
    uint8_t diff = chooseDifficulty(engine, "LUNAR", "Land soft on the pads");
    if (diff == 255) return;

    // Difficulty tuning: gravity (fp accel/frame), starting fuel, wind range.
    const int16_t gravity[3] = {2, 3, 4};            // fp units / frame
    const int16_t startFuel[3] = {160, 120, 90};
    const int16_t windMax[3] = {0, 2, 4};            // peak |wind| fp
    const int16_t thrustPow[3] = {7, 8, 9};          // upward fp impulse

    randomSeed(micros());

    uint8_t level = 1;
    int32_t score = 0;
    int16_t fuel = startFuel[diff];
    bool running = true;

    while (running) {
      // --- level setup -----------------------------------------------------
      World world;
      buildWorld(world, level);

      // Lander state in fixed point.
      int32_t fx = 10 * kFP;          // logical x*16
      int32_t fy = 4 * kFP;
      int32_t vx = (int32_t)random(2, 6) * (random(0, 2) ? 1 : -1);
      int32_t vy = 0;
      // Wind: constant per level, gusts a little. Sign+magnitude.
      int16_t wind = windMax[diff] ? (int16_t)random(-windMax[diff], windMax[diff] + 1) : 0;
      int16_t windGust = 0;

      bool thrustFlash = false;
      uint32_t thrustUntil = 0;
      uint8_t shake = 0;
      uint32_t nextFrame = 0;
      SparkField sparks;
      for (uint8_t i = 0; i < SparkField::kMax; ++i) sparks.sparks[i].life = 0;

      enum { PLAYING, CRASHED, LANDED } phase = PLAYING;
      uint8_t rating = 0;       // 0 hard,1 good,2 perfect
      int16_t gained = 0;

      while (true) {
        engine.tick();
        if (engine.shouldExitGame()) {
          engine.waitForRelease();
          return;
        }
        if (!frameDue(nextFrame, 70)) { delay(2); continue; }

        if (phase == PLAYING) {
          // --- input ---------------------------------------------------------
          bool steerL = engine.isHeld(BTN_LEFT) && fuel > 0;
          bool steerR = engine.isHeld(BTN_RIGHT) && fuel > 0;
          bool thrust = engine.isHeld(BTN_SELECT) && fuel > 0;

          if (steerL) { vx -= 1; fuel--; }
          if (steerR) { vx += 1; fuel--; }
          if (thrust) {
            vy -= thrustPow[diff];
            fuel--;
            if (!thrustFlash || (int32_t)(millis() - thrustUntil) >= 0) {
              engine.playSound(SOUND_THRUST);
            }
            thrustFlash = true;
            thrustUntil = millis() + 120;
            engine.ledPulse(CRGB::Orange, 60);
          }
          if (fuel < 0) fuel = 0;

          // --- physics -------------------------------------------------------
          vy += gravity[diff];
          // wind drifts horizontal velocity; gust wobble keeps it lively
          windGust = wind ? (int16_t)(wind + random(-1, 2)) : 0;
          vx += windGust >= 0 ? (windGust + 8) / 16 : -((-windGust + 8) / 16);
          // clamp velocities
          if (vx > 6 * kFP / 4) vx = 6 * kFP / 4;
          if (vx < -6 * kFP / 4) vx = -6 * kFP / 4;
          if (vy > 8 * kFP) vy = 8 * kFP;

          fx += vx;
          fy += vy;

          int16_t lx = fx / kFP;
          // walls: clamp and bounce gently
          if (lx < 0) { fx = 0; vx = -vx / 2; }
          if (lx > 128 - 7) { fx = (128 - 7) * kFP; vx = -vx / 2; }
          lx = fx / kFP;
          int16_t ly = fy / kFP;

          // thrust particles under engine
          if (thrustFlash && (int32_t)(millis() - thrustUntil) < 0) {
            sparksSpawn(sparks, gameX(engine, lx + 3),
                        gameY(engine, ly + 6), TFT_ORANGE, 2);
          } else {
            thrustFlash = false;
          }

          // --- ground collision ---------------------------------------------
          int16_t footY = ly + 6;          // landing legs logical y
          int16_t centerX = lx + 3;
          int16_t tLeft = terrainAt(world, lx);
          int16_t tRight = terrainAt(world, lx + 6);
          int16_t tMid = terrainAt(world, centerX);
          int16_t ground = min(tMid, min(tLeft, tRight));
          if (footY >= ground) {
            // Did we touch a pad with both feet roughly level?
            uint8_t mulL = padMulAt(world, lx);
            uint8_t mulR = padMulAt(world, lx + 6);
            uint8_t mul = padMulAt(world, centerX);
            bool onPad = mul > 0 && mulL > 0 && mulR > 0;
            int16_t impactV = vy / kFP;        // logical px/frame down
            int16_t driftV = abs(vx) / kFP;
            bool levelish = abs(tLeft - tRight) <= 1;

            if (onPad && levelish && impactV <= 4 && driftV <= 1) {
              // rating by impact velocity
              if (impactV <= 1 && driftV == 0) rating = 2;       // PERFECT
              else if (impactV <= 2) rating = 1;                 // GOOD
              else rating = 0;                                   // HARD
              int16_t base = 100 * mul;
              int16_t ratingBonus = rating == 2 ? 150 : rating == 1 ? 75 : 0;
              int16_t fuelBonus = fuel / 2;
              gained = base + ratingBonus + fuelBonus + level * 25;
              phase = LANDED;
              engine.playSound(rating == 2 ? SOUND_LEVELUP : SOUND_WIN);
              engine.ledPulse(CRGB::Green, 400);
            } else {
              phase = CRASHED;
              shake = 8;
              sparksSpawn(sparks, gameX(engine, centerX), gameY(engine, footY),
                          TFT_ORANGE, SparkField::kMax / 2);
              sparksSpawn(sparks, gameX(engine, centerX), gameY(engine, footY),
                          GAMER_WHITE, SparkField::kMax / 2);
              engine.playSound(SOUND_LOSE);
              engine.ledPulse(CRGB::Red, 350);
            }
          }
        }

        // --- render ----------------------------------------------------------
        int16_t ox = 0, oy = 0;
        if (shake) { ox = shakeOffset(2); oy = shakeOffset(2); }
        engine.clear();
        drawWorld(engine, world, ox, oy);

        int16_t ly = fy / kFP, lx = fx / kFP;
        if (phase != CRASHED) {
          drawLander(engine, lx + ox, ly + oy, thrustFlash, true);
        }
        sparksUpdateDraw(sparks, engine);

        // HUD: fuel bar (top-left), velocity, wind indicator, score, best
        engine.screen().setTextSize(1);
        // fuel bar
        int16_t fbx = gameX(engine, 2), fby = gameY(engine, 2);
        int16_t fbw = gameX(engine, 40), fbh = max<int16_t>(3, gameY(engine, 4));
        engine.screen().drawRect(fbx, fby, fbw, fbh, GAMER_WHITE);
        int16_t maxF = startFuel[diff];
        int16_t fillW = maxF > 0 ? (int32_t)(fbw - 2) * fuel / maxF : 0;
        if (fillW < 0) fillW = 0;
        uint16_t fcol = fuel > maxF / 3 ? TFT_GREEN
                        : fuel > maxF / 6 ? TFT_ORANGE : TFT_RED;
        engine.screen().fillRect(fbx + 1, fby + 1, fillW, fbh - 2, fcol);

        // level + live descent / drift readout. Descent speed colored by the
        // safe-landing threshold (<=2 px/frame soft) so the player can judge it.
        int16_t descent = vy / kFP;            // +down logical px/frame
        int16_t drift = abs(vx) / kFP;
        char hud[14];
        snprintf(hud, sizeof(hud), "L%u", (unsigned)level);
        engine.screen().setCursor(gameX(engine, 46), gameY(engine, 1));
        engine.screen().setTextColor(GAMER_WHITE, GAMER_BLACK);
        engine.screen().print(hud);
        char vel[14];
        snprintf(vel, sizeof(vel), "V%d H%d", (int)(descent < 0 ? 0 : descent),
                 (int)drift);
        uint16_t vcol = (descent <= 2 && drift <= 1) ? TFT_GREEN
                        : descent <= 4 ? TFT_ORANGE : TFT_RED;
        engine.screen().setCursor(gameX(engine, 46), gameY(engine, 8));
        engine.screen().setTextColor(vcol, GAMER_BLACK);
        engine.screen().print(vel);

        // score (right)
        char sc[10];
        snprintf(sc, sizeof(sc), "%ld", (long)score);
        engine.rightText(sc);
        drawBestTag(engine, best, gameY(engine, 8));

        // wind arrow indicator (center-top) if any wind
        if (wind != 0) {
          int16_t wy = gameY(engine, 3);
          int16_t wcx = engine.width() / 2;
          int16_t len = gameX(engine, 6);
          int16_t dir = wind > 0 ? 1 : -1;
          engine.screen().drawFastHLine(wcx - len / 2, wy, len, GAMER_ACCENT);
          // arrow head
          int16_t hxp = wcx + dir * (len / 2);
          engine.screen().drawLine(hxp, wy, hxp - dir * gameX(engine, 2),
                                   wy - gameY(engine, 2), GAMER_ACCENT);
          engine.screen().drawLine(hxp, wy, hxp - dir * gameX(engine, 2),
                                   wy + gameY(engine, 2), GAMER_ACCENT);
        }

        engine.show();

        if (shake) shake--;

        // --- phase resolution ------------------------------------------------
        if (phase == LANDED) {
          score += gained;
          // banner
          engine.clear();
          const char* word = rating == 2 ? "PERFECT" : rating == 1 ? "GOOD" : "HARD";
          uint16_t col = rating == 2 ? TFT_GREEN : rating == 1 ? GAMER_ACCENT : TFT_ORANGE;
          engine.centerText(word, gameY(engine, 14), 2, col);
          char b[20];
          snprintf(b, sizeof(b), "+%d", gained);
          engine.centerText(b, gameY(engine, 34), 1, GAMER_WHITE);
          char b2[20];
          snprintf(b2, sizeof(b2), "Level %u", (unsigned)(level + 1));
          engine.centerText(b2, gameY(engine, 46), 1, GAMER_DIM);
          engine.show();
          delay(1100);
          // progression: refuel a little, harder next
          level++;
          fuel += startFuel[diff] / 4;
          if (fuel > startFuel[diff]) fuel = startFuel[diff];
          break;  // next level
        }
        if (phase == CRASHED) {
          // let explosion sparks finish
          for (uint8_t f = 0; f < 10; ++f) {
            engine.tick();
            if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
            if (!frameDue(nextFrame, 60)) { delay(2); --f; continue; }
            engine.clear();
            drawWorld(engine, world, shakeOffset(2), 0);
            sparksUpdateDraw(sparks, engine);
            engine.show();
            if (shake) shake--;
          }
          bool record = bestSubmit("lunar", (uint16_t)min<int32_t>(score, 65535));
          if (!resultScreenBest(engine, "CRASHED", (int16_t)min<int32_t>(score, 32767),
                                 best > score ? best : (uint16_t)min<int32_t>(score, 65535),
                                 record)) {
            return;
          }
          running = false;
          break;  // back to difficulty pick
        }
      }  // inner frame loop
    }    // level loop
  }      // session loop
}
