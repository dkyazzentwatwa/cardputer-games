#include "Games.h"
#include "GameUtils.h"

namespace {
void gameOverPulse(GamerEngine& engine) {
  engine.ledPulse(CRGB::Red, 260);
}

bool restartScore(GamerEngine& engine, int16_t score) {
  return resultScreen(engine, "GAME OVER", score);
}

void drawRunner(GamerEngine& engine, int16_t heroX, int16_t heroY, int16_t hazardX,
                int16_t hazardY, uint16_t score, bool duck = false) {
  engine.clear();
  engine.screen().drawFastHLine(0, gameY(engine, 58), engine.width(), GAMER_WHITE);
  engine.screen().drawRect(gameX(engine, heroX), gameY(engine, heroY),
                           gameSize(engine, duck ? 9 : 7), gameSize(engine, duck ? 5 : 9),
                           GAMER_WHITE);
  engine.screen().fillRect(gameX(engine, hazardX), gameY(engine, hazardY),
                           gameSize(engine, 6), gameSize(engine, 12), GAMER_ACCENT);
  drawScore(engine, score);
  engine.show();
}

void runJumpRunner(GamerEngine& engine, const char* title, bool flipMode) {
  while (true) {
    if (!runIntro(engine, title, flipMode ? "SEL flips" : "SEL jumps")) return;
    int16_t y = flipMode ? 48 : 49;
    int16_t vy = 0;
    int8_t gravity = flipMode ? 1 : 1;
    int16_t hazardX = 128;
    uint16_t score = 0;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }
      if (selectTap(engine)) {
        if (flipMode) {
          gravity = -gravity;
          engine.ledPulse(CRGB::Purple, 80);
          engine.playSound(SOUND_JUMP);
        } else if (y >= 49) {
          vy = -9;
          engine.ledPulse(CRGB::Green, 60);
          engine.playSound(SOUND_JUMP);
        }
      }
      if (!frameDue(nextFrame, 34)) {
        delay(2);
        continue;
      }

      if (flipMode) {
        y += gravity * 3;
        y = constrain(y, 7, 49);
      } else {
        vy++;
        y += vy;
        if (y > 49) {
          y = 49;
          vy = 0;
        }
      }

      uint8_t scoreBoost = score / 12;
      if (scoreBoost > 4) scoreBoost = 4;
      hazardX -= 3 + scoreBoost;
      if (hazardX < -8) {
        hazardX = 128 + random(0, 24);
        score++;
        engine.ledPulse(CRGB::Blue, 45);
        engine.playSound(SOUND_SCORE);
      }
      int16_t hazardY = flipMode ? (gravity > 0 ? 52 : 7) : 46;
      bool hit = rectsOverlap(16, y, 8, 8, hazardX, hazardY, 7, 12);
      drawRunner(engine, 16, y, hazardX, hazardY, score);
      if (hit) {
        gameOverPulse(engine);
        if (!restartScore(engine, score)) return;
        break;
      }
    }
  }
}

void runFlyer(GamerEngine& engine, const char* title, bool useGapWall, bool selectHeld) {
  while (true) {
    if (!runIntro(engine, title, selectHeld ? "hold SEL up" : "SEL hop")) return;
    int16_t y = 30;
    int16_t vy = 0;
    int16_t wallX = 126;
    int16_t gapY = 22;
    uint16_t score = 0;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }
      if ((selectHeld && engine.isHeld(BTN_SELECT)) || (!selectHeld && selectTap(engine))) {
        vy -= selectHeld ? 1 : 7;
        engine.ledPulse(CRGB::Green, 35);
        engine.playSound(selectHeld ? SOUND_THRUST : SOUND_JUMP);
      }
      if (!frameDue(nextFrame, 34)) {
        delay(2);
        continue;
      }
      vy += 1;
      vy = constrain(vy, -7, 6);
      y += vy;
      wallX -= 3;
      if (wallX < -8) {
        wallX = 128;
        gapY = random(12, 40);
        score++;
        engine.ledPulse(CRGB::Aqua, 45);
        engine.playSound(SOUND_SCORE);
      }

      engine.clear();
      engine.screen().fillRect(gameX(engine, 16), gameY(engine, y), gameSize(engine, 6),
                               gameSize(engine, 6), GAMER_WHITE);
      if (useGapWall) {
        engine.screen().fillRect(gameX(engine, wallX), 0, gameSize(engine, 8), gameY(engine, gapY),
                                 GAMER_ACCENT);
        engine.screen().fillRect(gameX(engine, wallX), gameY(engine, gapY + 20),
                                 gameSize(engine, 8), engine.height() - gameY(engine, gapY + 20),
                                 GAMER_ACCENT);
      } else {
        int16_t topLine = gapY - 15;
        int16_t bottomLine = gapY + 23;
        if (topLine < 2) topLine = 2;
        if (bottomLine > 63) bottomLine = 63;
        engine.screen().drawFastHLine(0, gameY(engine, topLine), engine.width(), GAMER_WHITE);
        engine.screen().drawFastHLine(0, gameY(engine, bottomLine), engine.width(), GAMER_WHITE);
        engine.screen().fillRect(gameX(engine, wallX), gameY(engine, gapY - 4),
                                 gameSize(engine, 8), gameSize(engine, 8), GAMER_ACCENT);
      }
      drawScore(engine, score);
      engine.show();

      bool hit = y < 0 || y > 58;
      if (useGapWall && wallX < 22 && wallX + 8 > 16 && (y < gapY || y + 6 > gapY + 20)) hit = true;
      if (!useGapWall && rectsOverlap(16, y, 6, 6, wallX, gapY - 4, 8, 8)) hit = true;
      if (hit) {
        gameOverPulse(engine);
        if (!restartScore(engine, score)) return;
        break;
      }
    }
  }
}

void runCatchOrDodge(GamerEngine& engine, const char* title, bool catchMode, uint8_t shape) {
  while (true) {
    if (!runIntro(engine, title, catchMode ? "catch with L/R" : "dodge with L/R")) return;
    int16_t playerX = 58;
    int16_t itemX = random(4, 120);
    int16_t itemY = -8;
    uint16_t score = 0;
    uint8_t speed = 2;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }
      if (!frameDue(nextFrame, 42)) {
        delay(2);
        continue;
      }
      playerX += heldAxis(engine) * 4;
      playerX = constrain(playerX, 0, 116);
      itemY += speed;
      if (itemY > 66) {
        if (catchMode) {
          gameOverPulse(engine);
          if (!restartScore(engine, score)) return;
          break;
        }
        itemY = -8;
        itemX = random(4, 120);
        score++;
      }

      bool hit = rectsOverlap(playerX, 56, 12, 5, itemX, itemY, 6, 6);
      if (hit) {
        if (catchMode) {
          score++;
          itemY = -8;
          itemX = random(4, 120);
          speed = 2 + score / 8;
          if (speed > 6) speed = 6;
          engine.ledPulse(CRGB::Green, 70);
          engine.playSound(SOUND_SCORE);
        } else {
          gameOverPulse(engine);
          if (!restartScore(engine, score)) return;
          break;
        }
      }

      engine.clear();
      engine.screen().drawRect(gameX(engine, playerX), gameY(engine, 56), gameSize(engine, 12),
                               gameSize(engine, 5), GAMER_WHITE);
      if (shape == 0) engine.screen().fillRect(gameX(engine, itemX), gameY(engine, itemY),
                                               gameSize(engine, 5), gameSize(engine, 5),
                                               GAMER_ACCENT);
      else if (shape == 1) engine.screen().drawCircle(gameX(engine, itemX + 3),
                                                       gameY(engine, itemY + 3),
                                                       gameSize(engine, 3), GAMER_ACCENT);
      else {
        engine.screen().drawLine(gameX(engine, itemX), gameY(engine, itemY + 3),
                                 gameX(engine, itemX + 6), gameY(engine, itemY + 3),
                                 GAMER_ACCENT);
        engine.screen().drawLine(gameX(engine, itemX + 3), gameY(engine, itemY),
                                 gameX(engine, itemX + 3), gameY(engine, itemY + 6),
                                 GAMER_ACCENT);
      }
      drawScore(engine, score);
      engine.show();
    }
  }
}

void runLaneGame(GamerEngine& engine, const char* title, bool gates, bool river) {
  while (true) {
    if (!runIntro(engine, title, "L/R lanes")) return;
    int8_t lane = 1;
    int16_t y = -10;
    int8_t badLane = random(0, 3);
    uint16_t score = 0;
    uint32_t nextFrame = 0;
    uint8_t speed = 3;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }
      if (engine.wasPressed(BTN_LEFT) && lane > 0) {
        lane--;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT) && lane < 2) {
        lane++;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (!frameDue(nextFrame, river ? 48 : 42)) {
        delay(2);
        continue;
      }
      y += speed;
      if (y > 70) {
        y = -10;
        badLane = random(0, 3);
        score++;
        speed = 3 + score / 10;
        if (speed > 7) speed = 7;
        engine.ledPulse(CRGB::Blue, 45);
        engine.playSound(SOUND_SCORE);
      }

      engine.clear();
      for (uint8_t i = 1; i < 3; i++) {
        engine.screen().drawFastVLine(gameX(engine, i * 42), gameY(engine, 8),
                                      gameY(engine, 56), GAMER_DIM);
      }
      int16_t playerX = 17 + lane * 42;
      engine.screen().drawRect(gameX(engine, playerX), gameY(engine, 54), gameSize(engine, 8),
                               gameSize(engine, 8), GAMER_WHITE);
      if (gates) {
        for (uint8_t i = 0; i < 3; i++) {
          if (i != badLane) {
            engine.screen().fillRect(gameX(engine, 12 + i * 42), gameY(engine, y),
                                     gameSize(engine, 20), gameSize(engine, 4), GAMER_ACCENT);
          }
        }
      } else {
        int16_t objectX = 17 + badLane * 42;
        if (river) engine.screen().drawCircle(gameX(engine, objectX + 4), gameY(engine, y + 4),
                                              gameSize(engine, 5), GAMER_ACCENT);
        else engine.screen().fillRect(gameX(engine, objectX), gameY(engine, y),
                                      gameSize(engine, 8), gameSize(engine, 10), GAMER_ACCENT);
      }
      drawScore(engine, score);
      engine.show();

      bool crash = false;
      if (gates && y > 48 && y < 62 && lane != badLane) crash = true;
      if (!gates && y > 46 && y < 62 && lane == badLane) crash = true;
      if (crash) {
        gameOverPulse(engine);
        if (!restartScore(engine, score)) return;
        break;
      }
    }
  }
}

void runBreakoutInternal(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "BREAKOUT", "L/R paddle")) return;
    bool bricks[4][8];
    for (uint8_t r = 0; r < 4; r++) for (uint8_t c = 0; c < 8; c++) bricks[r][c] = true;
    uint8_t left = 32;
    uint8_t remaining = 32;
    int16_t ballX = 64;
    int16_t ballY = 42;
    int8_t vx = random(0, 2) ? 2 : -2;
    int8_t vy = -2;
    uint16_t score = 0;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }
      if (!frameDue(nextFrame, 28)) {
        delay(2);
        continue;
      }
      int16_t nextLeft = left + heldAxis(engine) * 4;
      if (nextLeft < 0) nextLeft = 0;
      if (nextLeft > 104) nextLeft = 104;
      left = nextLeft;
      ballX += vx;
      ballY += vy;
      if (ballX <= 0 || ballX >= 124) vx = -vx;
      if (ballY <= 0) vy = abs(vy);
      if (rectsOverlap(ballX, ballY, 4, 4, left, 58, 24, 4)) {
        vy = -abs(vy);
        vx += (ballX - (left + 12)) / 8;
        vx = constrain(vx, -4, 4);
        if (vx == 0) vx = 1;
        engine.playSound(SOUND_HIT);
      }
      for (uint8_t r = 0; r < 4; r++) {
        for (uint8_t c = 0; c < 8; c++) {
          if (bricks[r][c] && rectsOverlap(ballX, ballY, 4, 4, c * 16, 10 + r * 7, 14, 5)) {
            bricks[r][c] = false;
            remaining--;
            score += 5;
            vy = -vy;
            engine.ledPulse(CRGB::Aqua, 55);
            engine.playSound(SOUND_SCORE);
          }
        }
      }
      engine.clear();
      for (uint8_t r = 0; r < 4; r++) {
        for (uint8_t c = 0; c < 8; c++) {
          if (bricks[r][c]) {
            engine.screen().fillRect(gameX(engine, c * 16), gameY(engine, 10 + r * 7),
                                     gameSize(engine, 14), gameSize(engine, 5), GAMER_ACCENT);
          }
        }
      }
      engine.screen().fillRect(gameX(engine, left), gameY(engine, 58), gameSize(engine, 24),
                               gameSize(engine, 4), GAMER_WHITE);
      engine.screen().fillRect(gameX(engine, ballX), gameY(engine, ballY), gameSize(engine, 4),
                               gameSize(engine, 4), GAMER_WHITE);
      drawScore(engine, score);
      engine.show();
      if (ballY > 64 || remaining == 0) {
        engine.ledPulse(remaining == 0 ? CRGB::Green : CRGB::Red, 300);
        engine.playSound(remaining == 0 ? SOUND_WIN : SOUND_LOSE);
        if (!restartScore(engine, score)) return;
        break;
      }
    }
  }
}
}

void runBreakout(GamerEngine& engine) { runBreakoutInternal(engine); }
void runFlappyPico(GamerEngine& engine) { runFlyer(engine, "FLAPPY PICO", true, false); }
void runDinoRunner(GamerEngine& engine) { runJumpRunner(engine, "DINO RUNNER", false); }
void runJetpackRunner(GamerEngine& engine) { runFlyer(engine, "JETPACK", false, true); }
void runDodgeRain(GamerEngine& engine) { runCatchOrDodge(engine, "DODGE RAIN", false, 0); }
void runCatchStar(GamerEngine& engine) { runCatchOrDodge(engine, "CATCH STAR", true, 2); }
void runBasketCatch(GamerEngine& engine) { runCatchOrDodge(engine, "BASKET CATCH", true, 1); }
void runBalloonPop(GamerEngine& engine) { runCatchOrDodge(engine, "BALLOON POP", true, 1); }
void runCaveFlyer(GamerEngine& engine) { runFlyer(engine, "CAVE FLYER", true, true); }
void runTunnelRunner(GamerEngine& engine) { runFlyer(engine, "TUNNEL RUN", false, false); }
void runWallBounce(GamerEngine& engine) { runCatchOrDodge(engine, "WALL BOUNCE", true, 0); }
void runGravityFlip(GamerEngine& engine) { runJumpRunner(engine, "GRAVITY FLIP", true); }
void runPlatformHopper(GamerEngine& engine) { runJumpRunner(engine, "PLATFORM HOP", false); }
void runBrickDrop(GamerEngine& engine) { runCatchOrDodge(engine, "BRICK DROP", true, 0); }
void runLaneRacer(GamerEngine& engine) { runLaneGame(engine, "LANE RACER", false, false); }
void runTrafficDodge(GamerEngine& engine) { runLaneGame(engine, "TRAFFIC", false, false); }
void runSkiSlalom(GamerEngine& engine) { runLaneGame(engine, "SKI SLALOM", true, false); }
void runBoatSlalom(GamerEngine& engine) { runLaneGame(engine, "BOAT SLALOM", true, true); }
void runRailRunner(GamerEngine& engine) { runLaneGame(engine, "RAIL RUNNER", false, false); }
void runRoadDrift(GamerEngine& engine) { runLaneGame(engine, "ROAD DRIFT", true, false); }
