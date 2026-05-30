#include "Games.h"
#include "GameUtils.h"

namespace {
constexpr uint16_t PONG_FRAME_MS = 24;
}

void runPong(GamerEngine& engine) {
  const int16_t ballSize = GAMER_DISPLAY_IS_SH8601 ? 12 : 4;
  const int16_t paddleWidth = engine.width() / (GAMER_DISPLAY_IS_SH8601 ? 4 : 7);
  const int16_t paddleHeight = GAMER_DISPLAY_IS_SH8601 ? 10 : 4;
  const int16_t paddleY = engine.height() - (GAMER_DISPLAY_IS_SH8601 ? 32 : 10);
  const int16_t paddleSpeed = GAMER_DISPLAY_IS_SH8601 ? 10 : 4;

  while (true) {
    if (!engine.waitForSelectOrExit("PONG", "L/R move paddle", "SEL start")) return;

    int16_t ballX = engine.width() / 2;
    int16_t ballY = engine.height() / 4;
    int8_t ballVx = random(0, 2) == 0 ? -paddleSpeed / 2 : paddleSpeed / 2;
    int8_t ballVy = GAMER_DISPLAY_IS_SH8601 ? 7 : 2;
    int16_t paddleX = (engine.width() - paddleWidth) / 2;
    uint16_t score = 0;
    uint32_t nextFrame = 0;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }

      if (!frameDue(nextFrame, PONG_FRAME_MS)) {
        delay(2);
        continue;
      }

      if (engine.isHeld(BTN_LEFT)) paddleX -= paddleSpeed;
      if (engine.isHeld(BTN_RIGHT)) paddleX += paddleSpeed;
      paddleX = constrain(paddleX, 0, static_cast<int16_t>(engine.width() - paddleWidth));

      ballX += ballVx;
      ballY += ballVy;

      bool collision = false;
      if (ballX <= 0) {
        ballX = 0;
        ballVx = -ballVx;
        collision = true;
      }
      if (ballX + ballSize >= engine.width()) {
        ballX = engine.width() - ballSize;
        ballVx = -ballVx;
        collision = true;
      }
      if (ballY <= 0) {
        ballY = 0;
        ballVy = -ballVy;
        collision = true;
      }
      if (ballY + ballSize >= paddleY && ballX + ballSize >= paddleX && ballX <= paddleX + paddleWidth) {
        ballY = paddleY - ballSize;
        ballVy = -abs(ballVy);
        int16_t offset = ballX - (paddleX + paddleWidth / 2);
        int8_t nextVx = ballVx + offset / (GAMER_DISPLAY_IS_SH8601 ? 22 : 8);
        const int8_t maxVx = GAMER_DISPLAY_IS_SH8601 ? 12 : 4;
        if (nextVx < -maxVx) nextVx = -maxVx;
        if (nextVx > maxVx) nextVx = maxVx;
        ballVx = nextVx;
        if (ballVx == 0) ballVx = random(0, 2) == 0 ? -1 : 1;
        score += 10;
        collision = true;
        engine.playSound(SOUND_SCORE);
      }

      if (ballY + ballSize > engine.height()) {
        engine.ledPulse(CRGB::Red, 280);
        engine.playSound(SOUND_LOSE);
        if (!resultScreen(engine, "GAME OVER", score)) return;
        break;
      }

      if (collision) {
        engine.ledPulse(CRGB::Blue, 45);
        engine.playSound(SOUND_HIT);
      }

      engine.clear();
      engine.screen().fillRect(paddleX, paddleY, paddleWidth, paddleHeight, GAMER_WHITE);
      engine.screen().fillRect(ballX, ballY, ballSize, ballSize, GAMER_WHITE);
      drawScore(engine, score);
      engine.show();
    }
  }
}
