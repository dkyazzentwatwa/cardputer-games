#include "Games.h"
#include "GameUtils.h"

// =====================================================================
// FULL SPEED - high-speed lane-dodging racer.
//
// Depth: Easy/Normal/Hard (traffic density + base speed); gear tiers that
// climb with distance (visible GEAR + speed HUD, level-up cue); traffic
// variety (cars / trucks / bikes with different widths + relative speeds);
// near-miss bonus when squeezing past closely (combo cue + sparks); a fuel
// gauge that depletes over distance and is refilled by pickups; persistent
// best distance (HI tag + NEW BEST). Juice: sparks on near-miss/refuel/crash,
// crash shake, sound. Fixed memory: traffic[] + sparks are static arrays.
//
// SD key: "fullspeed".  Signature/exit contract unchanged.
// =====================================================================

namespace {

constexpr int16_t kRoadL = 16;   // road left edge at bottom (logical px)
constexpr int16_t kRoadR = 112;  // road right edge at bottom
constexpr int16_t kHorizon = 34;
constexpr uint8_t kTraffic = 5;  // max simultaneous oncoming vehicles
constexpr int16_t kPlayerMin = -46;
constexpr int16_t kPlayerMax = 46;
constexpr uint8_t kDashes = 4;   // scrolling centre-line dashes (motion feel)

// Vehicle kinds: width + relative speed factor (in eighths) + color.
struct Kind {
  int16_t halfW;
  uint8_t spd8;  // speed multiplier *8 relative to scroll
  uint16_t color;
};
const Kind kKinds[3] = {
    {7, 8, GAMER_WHITE},    // car
    {11, 6, GAMER_DIM},     // truck (wide, slower closing)
    {4, 11, GAMER_ACCENT},  // bike (narrow, fast)
};

struct Car {
  bool active;
  uint8_t kind;
  int16_t x;     // logical lateral offset (road-relative, like playerX)
  int16_t y;     // logical screen Y (0=horizon area .. height)
  bool scored;   // counted for near-miss / pass
};

struct Pickup {
  bool active;
  int16_t x;
  int16_t y;
};

// Scrolling centre-line dashes: phase advances with distance so the road
// reads as motion. Each dash sits at a logical Y that perspective-narrows.
void drawDashes(GamerEngine& engine, int16_t roadCenter, uint16_t phase) {
  const int16_t bottom = 64;
  for (uint8_t i = 0; i < kDashes; ++i) {
    int16_t ly = (int16_t)(kHorizon + 6 + ((phase + i * 30) % 120) * (bottom - kHorizon - 6) / 120);
    // perspective: width + dash length grow as the dash nears the player
    int16_t t = ly - kHorizon;          // 0..~30
    int16_t cx = gameX(engine, 64) + (roadCenter * t) / 30;
    int16_t w = max<int16_t>(1, gameSize(engine, t / 14));
    int16_t h = max<int16_t>(1, gameSize(engine, 1 + t / 16));
    engine.screen().fillRect(cx - w / 2, gameY(engine, ly), w, h, GAMER_DIM);
  }
}

void drawRoad(GamerEngine& engine, int16_t shift, uint16_t dashPhase) {
  const int16_t horizonY = gameY(engine, kHorizon);
  const int16_t roadBottom = engine.height() - 1;
  engine.screen().drawLine(gameX(engine, 18) + shift / 2, horizonY,
                           gameX(engine, 110) + shift / 2, horizonY, GAMER_WHITE);
  engine.screen().drawLine(gameX(engine, 46) + shift, horizonY, gameX(engine, kRoadL),
                           roadBottom, GAMER_WHITE);
  engine.screen().drawLine(gameX(engine, 82) + shift, horizonY, gameX(engine, kRoadR),
                           roadBottom, GAMER_WHITE);
  drawDashes(engine, shift, dashPhase);
}

// Lateral logical offset -> screen X, with perspective road sway.
int16_t laneX(GamerEngine& engine, int16_t lateral, int16_t roadCenter) {
  return gameX(engine, 64) + lateral + roadCenter;
}

void drawBike(GamerEngine& engine, int16_t x, int16_t y, uint16_t color) {
  const int16_t s = gameSize(engine, 3);
  engine.screen().drawRect(x - s, y - s * 2, s * 2, s, GAMER_WHITE);
  engine.screen().drawRect(x - s / 2, y - s / 2, s, s * 2, GAMER_WHITE);
  engine.screen().fillCircle(x, y - s * 3, max<int16_t>(1, s / 2), color);
}

// Oncoming vehicle: width scales with kind. Drawn as a rounded car body.
void drawCar(GamerEngine& engine, const Kind& k, int16_t x, int16_t y) {
  const int16_t hw = gameSize(engine, k.halfW);
  const int16_t h = gameSize(engine, 9);
  engine.screen().fillRoundRect(x - hw, y - h, hw * 2, h, gameSize(engine, 2), k.color);
  engine.screen().drawRoundRect(x - hw, y - h, hw * 2, h, gameSize(engine, 2), GAMER_BLACK);
  // windshield stripe
  engine.screen().fillRect(x - hw + gameSize(engine, 1), y - h + gameSize(engine, 1),
                           hw * 2 - gameSize(engine, 2), gameSize(engine, 2), GAMER_BLACK);
}

void drawFuel(GamerEngine& engine, uint8_t fuel) {  // 0..100
  const int16_t x = gameX(engine, 40), y = gameY(engine, 0) + gameSize(engine, 1);
  const int16_t w = gameSize(engine, 30), h = gameSize(engine, 5);
  engine.screen().drawRect(x, y, w, h, GAMER_DIM);
  int16_t fill = (int16_t)((w - 2) * fuel / 100);
  uint16_t fc = fuel > 35 ? TFT_GREEN : (fuel > 15 ? TFT_ORANGE : TFT_RED);
  if (fill > 0) engine.screen().fillRect(x + 1, y + 1, fill, h - 2, fc);
}

}  // namespace

void runFullSpeed(GamerEngine& engine) {
  static Car traffic[kTraffic];
  static SparkField sparks;
  const uint16_t best = bestLoad("fullspeed");

  while (true) {
    if (!engine.waitForSelectOrExit("FULL SPEED", "L/R steer", "SEL start")) return;
    uint8_t diff = chooseDifficulty(engine, "FULL SPEED", "Dodge + grab fuel");
    if (diff == 255) return;

    // Difficulty tuning: base scroll speed and spawn cadence.
    const uint8_t baseSpd[3] = {3, 4, 5};
    const uint16_t spawnMs[3] = {620, 480, 360};   // lower = denser traffic
    const uint8_t fuelDrain[3] = {1, 1, 2};         // fuel cost per pass

    for (uint8_t i = 0; i < kTraffic; ++i) traffic[i].active = false;
    for (uint8_t i = 0; i < SparkField::kMax; ++i) sparks.sparks[i].life = 0;

    Pickup fuelCan = {false, 0, 0};
    int16_t playerX = 0;
    int16_t roadShift = 0;
    int8_t roadDir = 1;
    uint8_t speed = baseSpd[diff];  // scroll speed (climbs with gear)
    uint8_t gear = 1;
    uint16_t distance = 0;           // primary score
    uint16_t combo = 0;              // chained near-misses
    uint8_t fuel = 100;
    uint8_t shake = 0;
    uint32_t nextFrame = 0;
    uint32_t nextSpawn = 0;
    uint32_t nextPickup = 4000;
    uint32_t gearShownUntil = 0;
    bool gearFlash = false;
    uint16_t dashPhase = 0;          // scrolling road dashes
    uint16_t bestNear = 0;           // longest near-miss chain this run (juice)
    uint16_t nextMilestone = 250;    // distance milestone for bonus fuel

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }

      uint32_t now = millis();
      if (now < nextFrame) {
        delay(2);
        continue;
      }
      nextFrame = now + 70;

      // --- input -----------------------------------------------------
      if (engine.isHeld(BTN_LEFT)) playerX -= 3;
      if (engine.isHeld(BTN_RIGHT)) playerX += 3;

      // --- road sway -------------------------------------------------
      roadShift += roadDir;
      if (roadShift <= -28 || roadShift >= 24) roadDir = -roadDir;
      int16_t roadCenter = roadShift;
      if (roadShift < -12) playerX++;
      else if (roadShift > 12) playerX--;

      bool crash = playerX < kPlayerMin || playerX > kPlayerMax;
      if (playerX < kPlayerMin) playerX = kPlayerMin;
      if (playerX > kPlayerMax) playerX = kPlayerMax;

      // --- gear/distance progression ---------------------------------
      distance++;
      dashPhase = (dashPhase + speed * 3) % 120;  // dashes scroll with speed
      uint8_t newGear = 1 + distance / 90;
      if (newGear > 9) newGear = 9;
      if (newGear > gear) {
        gear = newGear;
        if (speed < 12) speed++;
        engine.playSound(SOUND_LEVELUP);
        engine.ledPulse(CRGB::Yellow, 120);
        gearShownUntil = now + 900;
        gearFlash = true;
      }

      // fuel slowly drains with distance, faster at higher gears
      if (distance % (8 - (gear > 6 ? 6 : gear / 2)) == 0 && fuel > 0) fuel--;

      // distance milestones grant a small fuel reward + cue (rewards survival)
      if (distance >= nextMilestone) {
        nextMilestone += 250;
        fuel = (fuel > 85) ? 100 : fuel + 15;
        engine.playSound(SOUND_SCORE);
        engine.ledPulse(CRGB::Cyan, 120);
      }

      // --- spawn traffic --------------------------------------------
      if (now >= nextSpawn) {
        nextSpawn = now + spawnMs[diff] - (gear * 12);
        for (uint8_t i = 0; i < kTraffic; ++i) {
          if (!traffic[i].active) {
            traffic[i].active = true;
            traffic[i].kind = (uint8_t)random(0, 3);
            traffic[i].x = random(kPlayerMin + 6, kPlayerMax - 5);
            traffic[i].y = -8;
            traffic[i].scored = false;
            break;
          }
        }
      }

      // --- spawn fuel pickup -----------------------------------------
      if (!fuelCan.active && now >= nextPickup) {
        fuelCan.active = true;
        fuelCan.x = random(kPlayerMin + 6, kPlayerMax - 5);
        fuelCan.y = -8;
      }

      const int16_t playerY = engine.height() - gameY(engine, 8);
      const int16_t playerLY = 56;  // logical player band

      // --- advance traffic + collision/near-miss --------------------
      for (uint8_t i = 0; i < kTraffic; ++i) {
        Car& c = traffic[i];
        if (!c.active) continue;
        const Kind& k = kKinds[c.kind];
        c.y += (speed * k.spd8) / 8 + 1;

        int16_t carScreenY = gameY(engine, c.y);
        // collision band near the player
        if (c.y > 48 && c.y < 64) {
          int16_t dx = abs(playerX - c.x);
          int16_t hit = k.halfW + 5;
          if (dx < hit) {
            crash = true;
          } else if (!c.scored && dx < hit + 6) {
            // near miss: squeezed past closely
            c.scored = true;
            combo++;
            if (combo > bestNear) bestNear = combo;
            distance += combo;  // bonus scales with chain
            engine.playSound(SOUND_COMBO);
            engine.ledPulse(CRGB::Magenta, 90);
            sparksSpawn(sparks, laneX(engine, playerX, roadCenter), playerY,
                        TFT_MAGENTA, 6);
          }
        }
        if (c.y > 70) {  // passed safely off-bottom
          if (!c.scored) {
            c.scored = true;
            combo = 0;  // clean pass with no near-miss breaks the chain
          }
          c.active = false;
        }
      }

      // --- advance fuel pickup --------------------------------------
      if (fuelCan.active) {
        fuelCan.y += (speed * 8) / 8 + 1;
        if (fuelCan.y > 48 && fuelCan.y < 64 && abs(playerX - fuelCan.x) < 9) {
          fuelCan.active = false;
          nextPickup = now + (uint32_t)random(5000, 9000);
          fuel = (fuel > 70) ? 100 : fuel + 30;
          engine.playSound(SOUND_POWERUP);
          engine.ledPulse(CRGB::Green, 120);
          sparksSpawn(sparks, laneX(engine, playerX, roadCenter), playerY, TFT_GREEN, 8);
        } else if (fuelCan.y > 70) {
          fuelCan.active = false;
          nextPickup = now + (uint32_t)random(4000, 8000);
        }
      }

      if (fuel == 0) crash = true;  // out of fuel = run ends

      // --- render ----------------------------------------------------
      int16_t sx = shakeOffset(shake);
      int16_t sy = shakeOffset(shake);
      if (shake) shake--;

      engine.clear();
      engine.screen().setTextSize(engine.textScale());
      engine.screen().setCursor(0, 0);
      engine.screen().print("D:");
      engine.screen().print(distance);
      drawBestTag(engine, best);
      drawFuel(engine, fuel);

      // gear indicator (logical, right-of-center under HUD)
      char gtxt[6];
      snprintf(gtxt, sizeof(gtxt), "G%u", gear);
      engine.screen().setTextSize(1);
      engine.screen().setTextColor(
          (gearFlash && now < gearShownUntil) ? TFT_YELLOW : GAMER_DIM, GAMER_BLACK);
      engine.screen().setCursor(gameX(engine, 76), gameY(engine, 9));
      engine.screen().print(gtxt);
      if (combo > 1) {
        char ctxt[8];
        snprintf(ctxt, sizeof(ctxt), "x%u", combo);
        engine.screen().setTextColor(TFT_MAGENTA, GAMER_BLACK);
        engine.screen().setCursor(gameX(engine, 92), gameY(engine, 9));
        engine.screen().print(ctxt);
      }
      engine.screen().setTextColor(GAMER_WHITE, GAMER_BLACK);

      drawRoad(engine, roadShift + sx, dashPhase);

      // fuel pickup (green can with cross)
      if (fuelCan.active && fuelCan.y > -8) {
        int16_t fx = laneX(engine, fuelCan.x, roadCenter) + sx;
        int16_t fy = gameY(engine, fuelCan.y) + sy;
        int16_t r = gameSize(engine, 4);
        engine.screen().fillRect(fx - r, fy - r, r * 2, r * 2, TFT_GREEN);
        engine.screen().drawFastHLine(fx - r / 2, fy, r, GAMER_BLACK);
        engine.screen().drawFastVLine(fx, fy - r / 2, r, GAMER_BLACK);
      }

      // oncoming traffic
      for (uint8_t i = 0; i < kTraffic; ++i) {
        Car& c = traffic[i];
        if (!c.active || c.y <= -8) continue;
        drawCar(engine, kKinds[c.kind], laneX(engine, c.x, roadCenter) + sx,
                gameY(engine, c.y) + sy);
      }

      // player bike
      drawBike(engine, laneX(engine, playerX, roadCenter) + sx, playerY + sy, GAMER_ACCENT);

      sparksUpdateDraw(sparks, engine);
      engine.show();

      if (crash) {
        shake = 6;
        engine.ledPulse(CRGB::Red, 300);
        engine.playSound(SOUND_LOSE);
        sparksSpawn(sparks, laneX(engine, playerX, roadCenter), playerY, TFT_RED, 12);
        // brief shake-out frames
        for (uint8_t f = 0; f < 6; ++f) {
          engine.tick();
          int16_t ssx = shakeOffset(shake), ssy = shakeOffset(shake);
          if (shake) shake--;
          engine.clear();
          drawRoad(engine, roadShift + ssx, dashPhase);
          drawBike(engine, laneX(engine, playerX, roadCenter) + ssx, playerY + ssy, TFT_RED);
          sparksUpdateDraw(sparks, engine);
          if (bestNear > 1) {
            char ntxt[16];
            snprintf(ntxt, sizeof(ntxt), "BEST CHAIN x%u", bestNear);
            engine.centerText(ntxt, gameY(engine, 2), 1, TFT_MAGENTA);
          }
          engine.show();
          delay(40);
        }
        const bool record = bestSubmit("fullspeed", distance);
        const char* why = (fuel == 0) ? "OUT OF FUEL" : "CRASH";
        if (!resultScreenBest(engine, why, distance, best, record)) return;
        break;
      }
    }
  }
}
