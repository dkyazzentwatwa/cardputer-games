#include "Games.h"

namespace {
constexpr uint8_t CELL = GAMER_DISPLAY_IS_SH8601 ? 24 : 8;
constexpr uint8_t GRID_W = SCREEN_WIDTH / CELL;
constexpr uint8_t GRID_H = (SCREEN_HEIGHT - (GAMER_DISPLAY_IS_SH8601 ? 44 : 0)) / CELL;
constexpr uint8_t MAX_SNAKE = GRID_W * GRID_H;

enum Direction : uint8_t {
  DIR_UP = 0,
  DIR_RIGHT = 1,
  DIR_DOWN = 2,
  DIR_LEFT = 3
};

struct Cell {
  int8_t x;
  int8_t y;
};

Direction turnLeft(Direction direction) {
  return static_cast<Direction>((direction + 3) % 4);
}

Direction turnRight(Direction direction) {
  return static_cast<Direction>((direction + 1) % 4);
}

bool sameCell(const Cell& a, const Cell& b) {
  return a.x == b.x && a.y == b.y;
}

bool snakeContains(const Cell snake[], uint8_t length, const Cell& cell) {
  for (uint8_t i = 0; i < length; i++) {
    if (sameCell(snake[i], cell)) return true;
  }
  return false;
}

Cell placeFood(const Cell snake[], uint8_t length) {
  Cell open[MAX_SNAKE];
  uint8_t openCount = 0;
  for (uint8_t y = 0; y < GRID_H; y++) {
    for (uint8_t x = 0; x < GRID_W; x++) {
      Cell candidate = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
      if (!snakeContains(snake, length, candidate)) {
        open[openCount++] = candidate;
      }
    }
  }
  if (openCount == 0) return {0, 0};
  return open[random(0, openCount)];
}

void drawSnake(GamerEngine& engine, const Cell snake[], uint8_t length, const Cell& food, uint16_t score) {
  engine.clear();
  const int16_t ox = (engine.width() - GRID_W * CELL) / 2;
  const int16_t oy = GAMER_DISPLAY_IS_SH8601 ? 34 : 0;
  engine.screen().fillRect(ox + food.x * CELL + CELL / 4, oy + food.y * CELL + CELL / 4,
                           CELL / 2, CELL / 2, GAMER_ACCENT);
  for (uint8_t i = 0; i < length; i++) {
    engine.screen().drawRect(ox + snake[i].x * CELL, oy + snake[i].y * CELL, CELL, CELL,
                             i == length - 1 ? GAMER_ACCENT : GAMER_WHITE);
  }
  char scoreText[8];
  snprintf(scoreText, sizeof(scoreText), "%u", score);
  engine.rightText(scoreText);
  engine.show();
}
}

void runSnake(GamerEngine& engine) {
  while (true) {
    if (!engine.waitForSelectOrExit("SNAKE", "L/R turn", "SEL start")) return;

    Cell snake[MAX_SNAKE];
    uint8_t length = 1;
    snake[0] = {GRID_W / 2, GRID_H / 2};
    Direction direction = static_cast<Direction>(random(0, 4));
    Cell food = placeFood(snake, length);
    uint16_t score = 0;
    uint16_t frameDelay = 170;
    uint32_t nextFrame = 0;
    bool alive = true;

    drawSnake(engine, snake, length, food, score);

    while (alive) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }
      if (engine.wasPressed(BTN_LEFT)) {
        direction = turnLeft(direction);
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT)) {
        direction = turnRight(direction);
        engine.playSound(SOUND_UI_MOVE);
      }

      uint32_t now = millis();
      if (now < nextFrame) {
        delay(2);
        continue;
      }
      nextFrame = now + frameDelay;

      Cell head = snake[length - 1];
      if (direction == DIR_UP) head.y--;
      else if (direction == DIR_DOWN) head.y++;
      else if (direction == DIR_LEFT) head.x--;
      else head.x++;

      if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H || snakeContains(snake, length, head)) {
        alive = false;
        break;
      }

      bool ate = sameCell(head, food);
      if (!ate) {
        for (uint8_t i = 1; i < length; i++) {
          snake[i - 1] = snake[i];
        }
        snake[length - 1] = head;
      } else if (length < MAX_SNAKE) {
        snake[length++] = head;
        score++;
        frameDelay = frameDelay > 85 ? frameDelay - 5 : 80;
        food = placeFood(snake, length);
        engine.ledPulse(CRGB::Green, 90);
        engine.playSound(SOUND_SCORE);
      }

      drawSnake(engine, snake, length, food, score);
    }

    engine.ledPulse(CRGB::Red, 280);
    engine.playSound(SOUND_LOSE);
    char detail[18];
    snprintf(detail, sizeof(detail), "Length %u", static_cast<unsigned>(length));
    if (!engine.showResult("GAME OVER", detail)) return;
  }
}
