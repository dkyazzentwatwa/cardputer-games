#include "Games.h"
#include "GameUtils.h"

namespace {
constexpr uint16_t LUNAR_FRAME_MS = 90;

void drawLander(GamerEngine& engine, int16_t x, int16_t y, bool thrust) {
  const int16_t s = gameSize(engine, 3);
  engine.screen().drawRect(x + s, y + s, s * 3, s * 2, GAMER_WHITE);
  engine.screen().drawRect(x + s * 2, y, s * 2, s, GAMER_WHITE);
  engine.screen().drawFastVLine(x, y + s * 3, s * 2, GAMER_WHITE);
  engine.screen().drawFastVLine(x + s * 5, y + s * 3, s * 2, GAMER_WHITE);
  if (thrust) {
    engine.screen().drawFastVLine(x + s * 3, y + s * 4, s * 3, GAMER_ACCENT);
  }
}
}

void runLunarModule(GamerEngine& engine) {
  while (true) {
    if (!engine.waitForSelectOrExit("LUNAR MODULE", "L/R steer", "SEL thrust")) return;

    uint8_t level = 1;
    int16_t x = 8;
    int16_t y = 2;
    int8_t vx = 1;
    int8_t vy = 0;
    int8_t fuel = 30;
    bool thrustFlash = false;
    uint32_t thrustUntil = 0;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (checkExit(engine)) return;

      const uint32_t now = millis();
      if (!frameDue(nextFrame, LUNAR_FRAME_MS)) {
        delay(2);
        continue;
      }

      if (engine.isHeld(BTN_LEFT) && fuel > 0) {
        vx--;
        fuel--;
      }
      if (engine.isHeld(BTN_RIGHT) && fuel > 0) {
        vx++;
        fuel--;
      }
      if (engine.wasPressed(BTN_SELECT) && fuel > 0) {
        vy -= 2;
        fuel--;
        thrustFlash = true;
        thrustUntil = now + 140;
        engine.ledPulse(CRGB::Orange, 90);
        engine.playSound(SOUND_THRUST);
      }

      vy++;
      x += vx;
      y += vy / 2;

      if (x < 0) {
        x = 0;
        vx = 1;
      }
      if (x > engine.width() - gameSize(engine, 18)) {
        x = engine.width() - gameSize(engine, 18);
        vx = -1;
      }
      if (fuel < 0) fuel = 0;
      if (thrustFlash && static_cast<int32_t>(now - thrustUntil) >= 0) thrustFlash = false;

      engine.clear();
      drawStat(engine, "F:", fuel);
      char velocity[8];
      snprintf(velocity, sizeof(velocity), "V:%d", vy);
      engine.rightText(velocity);
      const int16_t padW = gameX(engine, 22);
      const int16_t padX = engine.width() - padW - gameX(engine, 12);
      const int16_t groundY = engine.height() - gameY(engine, 2);
      engine.screen().drawRect(padX, groundY, padW, max<int16_t>(2, gameSize(engine, 2)),
                               GAMER_WHITE);
      drawLander(engine, x, y, thrustFlash);
      engine.show();

      if (y >= engine.height() - gameY(engine, 12)) {
        bool landed = x >= padX - gameSize(engine, 4) && x <= padX + padW &&
                      abs(vx) <= 3 && vy <= 6;
        if (landed) {
          engine.ledPulse(CRGB::Green, 450);
          engine.playSound(SOUND_WIN);
          engine.clear();
          engine.centerText("LANDING OK", 18);
          char levelText[18];
          snprintf(levelText, sizeof(levelText), "Level %u", static_cast<unsigned>(level + 1));
          engine.centerText(levelText, 36);
          engine.show();
          delay(900);
          level++;
          x = 8;
          y = 2;
          vx = level;
          vy = 0;
          fuel = (32 - level) > 18 ? 32 - level : 18;
        } else {
          engine.ledPulse(CRGB::Red, 300);
          engine.playSound(SOUND_LOSE);
          char detail[18];
          snprintf(detail, sizeof(detail), "Level %u", static_cast<unsigned>(level));
          if (!engine.showResult("CRASH", detail)) return;
          break;
        }
      }
    }
  }
}
