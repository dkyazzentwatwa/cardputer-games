#include "Games.h"
#include "GameUtils.h"

namespace {
struct ShotState {
  int16_t x;
  int16_t y;
  bool active;
};

void drawTarget(GamerEngine& engine, int16_t x, int16_t y, uint8_t style) {
  if (style == 0) {
    engine.screen().drawCircle(gameX(engine, x + 4), gameY(engine, y + 4), gameSize(engine, 4),
                               GAMER_ACCENT);
  } else if (style == 1) {
    engine.screen().drawRect(gameX(engine, x), gameY(engine, y), gameSize(engine, 10),
                             gameSize(engine, 7), GAMER_ACCENT);
    engine.screen().drawFastHLine(gameX(engine, x + 2), gameY(engine, y + 7),
                                  gameSize(engine, 6), GAMER_ACCENT);
  } else if (style == 2) {
    engine.screen().drawTriangle(gameX(engine, x + 5), gameY(engine, y),
                                 gameX(engine, x), gameY(engine, y + 8),
                                 gameX(engine, x + 10), gameY(engine, y + 8), GAMER_ACCENT);
  } else {
    engine.screen().fillRect(gameX(engine, x), gameY(engine, y), gameSize(engine, 8),
                             gameSize(engine, 8), GAMER_ACCENT);
  }
}

void runTurretPattern(GamerEngine& engine, const char* title, uint8_t style, bool sideTravel, bool manyTargets) {
  while (true) {
    if (!runIntro(engine, title, "L/R aim SEL fire")) return;
    int16_t turretX = 60;
    int16_t targetX[3] = {20, 70, 110};
    int16_t targetY[3] = {-10, -32, -54};
    int8_t targetDx[3] = {1, -1, 1};
    ShotState shot = {0, 0, false};
    uint16_t score = 0;
    uint8_t lives = 3;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      turretX += heldAxis(engine) * 4;
      turretX = constrain(turretX, 0, 120);
      if (selectTap(engine) && !shot.active) {
        shot = {static_cast<int16_t>(turretX + 4), 52, true};
        engine.ledPulse(CRGB::Blue, 35);
        engine.playSound(SOUND_SHOOT);
      }
      if (!frameDue(nextFrame, 34)) {
        delay(2);
        continue;
      }

      if (shot.active) {
        shot.y -= 6;
        if (shot.y < 0) shot.active = false;
      }

      uint8_t count = manyTargets ? 3 : 1;
      for (uint8_t i = 0; i < count; i++) {
        targetY[i] += 2 + score / 18;
        if (sideTravel) {
          targetX[i] += targetDx[i];
          if (targetX[i] < 0 || targetX[i] > 118) targetDx[i] = -targetDx[i];
        }
        if (targetY[i] > 64) {
          targetY[i] = -random(10, 45);
          targetX[i] = random(4, 116);
          if (lives > 0) lives--;
          engine.ledPulse(CRGB::Red, 90);
          engine.playSound(SOUND_HIT);
        }
        if (shot.active && rectsOverlap(shot.x, shot.y, 2, 7, targetX[i], targetY[i], 10, 10)) {
          shot.active = false;
          targetY[i] = -random(12, 60);
          targetX[i] = random(4, 116);
          score++;
          engine.ledPulse(CRGB::Green, 65);
          engine.playSound(SOUND_SCORE);
        }
      }

      engine.clear();
      engine.screen().drawRect(gameX(engine, turretX), gameY(engine, 56), gameSize(engine, 10),
                               gameSize(engine, 6), GAMER_WHITE);
      engine.screen().drawFastVLine(gameX(engine, turretX + 5), gameY(engine, 50),
                                    gameSize(engine, 7), GAMER_WHITE);
      if (shot.active) engine.screen().fillRect(gameX(engine, shot.x), gameY(engine, shot.y),
                                                gameSize(engine, 2), gameSize(engine, 7),
                                                GAMER_WHITE);
      for (uint8_t i = 0; i < count; i++) drawTarget(engine, targetX[i], targetY[i], style);
      drawStat(engine, "L:", lives);
      drawScore(engine, score);
      engine.show();

      if (lives == 0) {
        engine.ledPulse(CRGB::Red, 280);
        engine.playSound(SOUND_LOSE);
        if (!resultScreen(engine, "GAME OVER", score)) return;
        break;
      }
    }
  }
}

void runMissilePattern(GamerEngine& engine, const char* title, bool wideBlast) {
  while (true) {
    if (!runIntro(engine, title, "L/R site SEL")) return;
    int16_t siteX = 60;
    int16_t threatX = random(8, 120);
    int16_t threatY = -8;
    int16_t blastX = -20;
    int16_t blastY = -20;
    uint8_t blastLife = 0;
    uint16_t score = 0;
    uint8_t base = 5;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      siteX += heldAxis(engine) * 4;
      siteX = constrain(siteX, 0, 124);
      if (selectTap(engine) && blastLife == 0) {
        blastX = siteX;
        blastY = 36;
        blastLife = wideBlast ? 10 : 6;
        engine.ledPulse(CRGB::Aqua, 50);
        engine.playSound(SOUND_SHOOT);
      }
      if (!frameDue(nextFrame, 42)) {
        delay(2);
        continue;
      }
      threatY += 2 + score / 16;
      if (blastLife > 0) blastLife--;
      int16_t radius = wideBlast ? 12 : 8;
      if (blastLife > 0 && abs(threatX - blastX) < radius && abs(threatY - blastY) < radius) {
        threatY = -random(10, 40);
        threatX = random(8, 120);
        score++;
        engine.ledPulse(CRGB::Green, 70);
        engine.playSound(SOUND_SCORE);
      }
      if (threatY > 62) {
        threatY = -random(10, 40);
        threatX = random(8, 120);
        if (base > 0) base--;
        engine.ledPulse(CRGB::Red, 90);
        engine.playSound(SOUND_HIT);
      }

      engine.clear();
      engine.screen().drawFastHLine(0, gameY(engine, 63), engine.width(), GAMER_WHITE);
      engine.screen().drawFastVLine(gameX(engine, siteX), gameY(engine, 50), gameSize(engine, 10),
                                    GAMER_WHITE);
      engine.screen().drawLine(gameX(engine, siteX - 3), gameY(engine, 56),
                               gameX(engine, siteX + 3), gameY(engine, 56), GAMER_WHITE);
      engine.screen().fillRect(gameX(engine, threatX), gameY(engine, threatY), gameSize(engine, 4),
                               gameSize(engine, 8), GAMER_ACCENT);
      if (blastLife > 0) engine.screen().drawCircle(gameX(engine, blastX), gameY(engine, blastY),
                                                    gameSize(engine, radius), GAMER_ACCENT);
      drawStat(engine, "B:", base);
      drawScore(engine, score);
      engine.show();

      if (base == 0) {
        engine.ledPulse(CRGB::Red, 260);
        engine.playSound(SOUND_LOSE);
        if (!resultScreen(engine, "BASE LOST", score)) return;
        break;
      }
    }
  }
}
}

void runAsteroidsLite(GamerEngine& engine) { runTurretPattern(engine, "ASTEROIDS", 0, true, true); }
void runInvadersLite(GamerEngine& engine) { runTurretPattern(engine, "INVADERS", 1, false, true); }
void runMissileCommandLite(GamerEngine& engine) { runMissilePattern(engine, "MISSILE CMD", true); }
void runTurretDefense(GamerEngine& engine) { runTurretPattern(engine, "TURRET DEF", 2, false, false); }
void runUfoDefender(GamerEngine& engine) { runTurretPattern(engine, "UFO DEFENDER", 1, true, false); }
void runMeteorBlaster(GamerEngine& engine) { runMissilePattern(engine, "METEOR BLAST", false); }
