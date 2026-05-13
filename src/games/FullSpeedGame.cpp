#include "Games.h"
#include "GameUtils.h"

namespace {
void drawRoad(GamerEngine& engine, int16_t shift) {
  const int16_t horizonY = gameY(engine, 34);
  const int16_t roadBottom = engine.height() - 1;
  engine.screen().drawLine(gameX(engine, 18) + shift / 2, horizonY,
                           gameX(engine, 110) + shift / 2, horizonY, GAMER_WHITE);
  engine.screen().drawLine(gameX(engine, 46) + shift, horizonY, gameX(engine, 16), roadBottom,
                           GAMER_WHITE);
  engine.screen().drawLine(gameX(engine, 82) + shift, horizonY, gameX(engine, 112), roadBottom,
                           GAMER_WHITE);
  engine.screen().drawLine(gameX(engine, 62) + shift / 2, horizonY + gameSize(engine, 4),
                           gameX(engine, 60), roadBottom, GAMER_DIM);
  engine.screen().drawLine(gameX(engine, 66) + shift / 2, horizonY + gameSize(engine, 4),
                           gameX(engine, 68), roadBottom, GAMER_DIM);
}

void drawBike(GamerEngine& engine, int16_t x, int16_t y) {
  const int16_t s = gameSize(engine, 3);
  engine.screen().drawRect(x - s, y - s * 2, s * 2, s, GAMER_WHITE);
  engine.screen().drawRect(x - s / 2, y - s / 2, s, s * 2, GAMER_WHITE);
  engine.screen().fillCircle(x, y - s * 3, max<int16_t>(1, s / 2), GAMER_ACCENT);
}
}

void runFullSpeed(GamerEngine& engine) {
  while (true) {
    if (!engine.waitForSelectOrExit("FULL SPEED", "L/R steer", "SEL start")) return;

    int16_t playerX = 0;
    int16_t roadShift = 0;
    int8_t roadDir = 1;
    int16_t obstacleY = -20;
    int16_t obstacleX = 0;
    uint8_t speed = 1;
    uint16_t score = 0;
    uint32_t nextFrame = 0;

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

      if (engine.isHeld(BTN_LEFT)) playerX -= 3;
      if (engine.isHeld(BTN_RIGHT)) playerX += 3;

      roadShift += roadDir;
      if (roadShift <= -28 || roadShift >= 24) roadDir = -roadDir;

      obstacleY += (speed + 1) * (GAMER_DISPLAY_IS_SH8601 ? 4 : 1);
      if (obstacleY > engine.height()) {
        obstacleY = -gameY(engine, 10);
        obstacleX = random(-34, 35);
        score++;
        engine.ledPulse(CRGB::Aqua, 70);
        engine.playSound(SOUND_SCORE);
        if (score % 8 == 0 && speed < 5) speed++;
      }

      int16_t roadCenter = roadShift;
      if (roadShift < -12) playerX++;
      else if (roadShift > 12) playerX--;

      bool crash = playerX < -46 || playerX > 46;
      if (obstacleY > gameY(engine, 42) && obstacleY < engine.height() &&
          abs(playerX - obstacleX - roadCenter / 2) < 7) {
        crash = true;
      }

      engine.clear();
      engine.screen().setTextSize(engine.textScale());
      engine.screen().setCursor(0, 0);
      engine.screen().print("S:");
      engine.screen().print(score);
      char speedText[8];
      snprintf(speedText, sizeof(speedText), "%uk", static_cast<unsigned>((score + 1) * 5));
      engine.rightText(speedText);
      drawRoad(engine, roadShift);
      drawBike(engine, gameX(engine, 64) + playerX * (GAMER_DISPLAY_IS_SH8601 ? 3 : 1),
               engine.height() - gameY(engine, 8));
      if (obstacleY > -gameY(engine, 8)) {
        drawBike(engine,
                 gameX(engine, 64) + obstacleX * (GAMER_DISPLAY_IS_SH8601 ? 3 : 1) +
                     roadCenter,
                 obstacleY);
      }
      engine.show();

      if (crash) {
        engine.ledPulse(CRGB::Red, 300);
        engine.playSound(SOUND_LOSE);
        char detail[18];
        snprintf(detail, sizeof(detail), "Score %u", score);
        if (!engine.showResult("CRASH", detail)) return;
        break;
      }
    }
  }
}
