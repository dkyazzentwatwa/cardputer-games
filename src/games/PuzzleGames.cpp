#include "Games.h"
#include "GameUtils.h"

namespace {
void drawCellValue(GamerEngine& engine, int16_t x, int16_t y, uint8_t value, uint8_t size) {
  if (value == 0) engine.screen().drawRect(x, y, size - 1, size - 1, GAMER_WHITE);
  else if (value == 1) engine.screen().fillRect(x + 2, y + 2, size - 4, size - 4, GAMER_WHITE);
  else {
    engine.screen().drawLine(x + 2, y + 2, x + size - 4, y + size - 4, GAMER_WHITE);
    engine.screen().drawLine(x + size - 4, y + 2, x + 2, y + size - 4, GAMER_WHITE);
  }
}

int8_t cursorStep(GamerEngine& engine, int8_t current, int8_t count) {
  const int8_t previous = current;
  if (engine.wasPressed(BTN_LEFT)) current = current == 0 ? count - 1 : current - 1;
  if (engine.wasPressed(BTN_RIGHT)) current = (current + 1) % count;
  if (current != previous) {
    engine.playSound(SOUND_UI_MOVE);
  }
  return current;
}

void flood(uint8_t board[5][5], uint8_t x, uint8_t y, uint8_t from, uint8_t to) {
  if (x >= 5 || y >= 5 || board[y][x] != from || from == to) return;
  board[y][x] = to;
  if (x > 0) flood(board, x - 1, y, from, to);
  if (x < 4) flood(board, x + 1, y, from, to);
  if (y > 0) flood(board, x, y - 1, from, to);
  if (y < 4) flood(board, x, y + 1, from, to);
}

bool allSame(uint8_t board[5][5]) {
  uint8_t first = board[0][0];
  for (uint8_t y = 0; y < 5; y++) for (uint8_t x = 0; x < 5; x++) if (board[y][x] != first) return false;
  return true;
}

void drawTinyBoard(GamerEngine& engine, const uint8_t* board, uint8_t cols, uint8_t rows,
                   uint8_t cursor, uint8_t cell, int16_t ox, int16_t oy) {
  const int16_t scaledCell = gameSize(engine, cell);
  const int16_t scaledOx = gameX(engine, ox);
  const int16_t scaledOy = gameY(engine, oy);
  for (uint8_t i = 0; i < cols * rows; i++) {
    uint8_t x = i % cols;
    uint8_t y = i / cols;
    const int16_t px = scaledOx + x * scaledCell;
    const int16_t py = scaledOy + y * scaledCell;
    if (board[i] != 0) engine.screen().fillRect(px + scaledCell / 6, py + scaledCell / 6,
                                                scaledCell * 2 / 3, scaledCell * 2 / 3,
                                                GAMER_ACCENT);
    else engine.screen().drawRect(px + scaledCell / 6, py + scaledCell / 6,
                                  scaledCell * 2 / 3, scaledCell * 2 / 3, GAMER_WHITE);
    if (i == cursor) drawCursorBox(engine, px + 1, py + 1, scaledCell - 2, scaledCell - 2);
  }
}

bool boardResult(GamerEngine& engine, const char* title, int16_t score) {
  engine.ledPulse(CRGB::Green, 260);
  return resultScreen(engine, title, score);
}

void add2048Tile(uint8_t board[4][4]) {
  uint8_t empty[16];
  uint8_t emptyCount = 0;
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (board[y][x] == 0) {
        empty[emptyCount++] = y * 4 + x;
      }
    }
  }
  if (emptyCount == 0) return;
  const uint8_t spot = empty[random(0, emptyCount)];
  board[spot / 4][spot % 4] = random(0, 10) == 0 ? 2 : 1;
}

bool slide2048Line(uint8_t line[4], uint16_t& score) {
  uint8_t packed[4] = {0, 0, 0, 0};
  uint8_t count = 0;
  bool changed = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (line[i] != 0) {
      packed[count++] = line[i];
    }
  }
  for (uint8_t i = 0; i < 3; i++) {
    if (packed[i] != 0 && packed[i] == packed[i + 1]) {
      packed[i]++;
      score += static_cast<uint16_t>(1U << packed[i]);
      for (uint8_t j = i + 1; j < 3; j++) {
        packed[j] = packed[j + 1];
      }
      packed[3] = 0;
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (line[i] != packed[i]) {
      changed = true;
    }
    line[i] = packed[i];
  }
  return changed;
}

bool move2048(uint8_t board[4][4], GamerDirection direction, uint16_t& score) {
  bool changed = false;
  for (uint8_t index = 0; index < 4; index++) {
    uint8_t line[4];
    for (uint8_t pos = 0; pos < 4; pos++) {
      const uint8_t readPos = (direction == GAMER_DIR_RIGHT || direction == GAMER_DIR_DOWN) ? 3 - pos : pos;
      const uint8_t x = (direction == GAMER_DIR_LEFT || direction == GAMER_DIR_RIGHT) ? readPos : index;
      const uint8_t y = (direction == GAMER_DIR_UP || direction == GAMER_DIR_DOWN) ? readPos : index;
      line[pos] = board[y][x];
    }
    const bool lineChanged = slide2048Line(line, score);
    changed = changed || lineChanged;
    for (uint8_t pos = 0; pos < 4; pos++) {
      const uint8_t writePos = (direction == GAMER_DIR_RIGHT || direction == GAMER_DIR_DOWN) ? 3 - pos : pos;
      const uint8_t x = (direction == GAMER_DIR_LEFT || direction == GAMER_DIR_RIGHT) ? writePos : index;
      const uint8_t y = (direction == GAMER_DIR_UP || direction == GAMER_DIR_DOWN) ? writePos : index;
      board[y][x] = line[pos];
    }
  }
  return changed;
}

bool canMove2048(uint8_t board[4][4]) {
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (board[y][x] == 0) return true;
      if (x < 3 && board[y][x] == board[y][x + 1]) return true;
      if (y < 3 && board[y][x] == board[y + 1][x]) return true;
    }
  }
  return false;
}

bool has2048Tile(uint8_t board[4][4]) {
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (board[y][x] >= 11) return true;
    }
  }
  return false;
}

void tile2048Label(uint8_t value, char* label, size_t labelSize) {
  if (value == 0) {
    snprintf(label, labelSize, "");
  } else if (value >= 10) {
    snprintf(label, labelSize, "%uK", static_cast<unsigned>(1U << (value - 10)));
  } else {
    snprintf(label, labelSize, "%u", static_cast<unsigned>(1U << value));
  }
}

void draw2048Board(GamerEngine& engine, uint8_t board[4][4], uint16_t score) {
  engine.clear();
  engine.screen().setTextSize(1);
  engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
  engine.screen().setCursor(0, 0);
  engine.screen().print("2048");
  drawScore(engine, static_cast<int16_t>(min<uint16_t>(score, 32767)));

  const int16_t cellW = 22;
  const int16_t cellH = 13;
  const int16_t ox = 20;
  const int16_t oy = 10;
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      const int16_t px = ox + x * cellW;
      const int16_t py = oy + y * cellH;
      const uint8_t value = board[y][x];
      const uint16_t border = value == 0 ? GAMER_DIM : GAMER_ACCENT;
      if (value != 0) {
        engine.screen().fillRect(px + 1, py + 1, cellW - 2, cellH - 2,
                                 value >= 8 ? GAMER_INVERSE : GAMER_ACCENT);
      }
      engine.screen().drawRect(px, py, cellW - 1, cellH - 1, border);
      if (value != 0) {
        char label[5];
        tile2048Label(value, label, sizeof(label));
        const uint8_t len = strlen(label);
        engine.screen().setTextColor(value >= 8 ? GAMER_BLACK : GAMER_BLACK,
                                     value >= 8 ? GAMER_INVERSE : GAMER_ACCENT);
        engine.screen().setCursor(px + max<int16_t>(2, (cellW - len * 6) / 2),
                                  py + 3);
        engine.screen().print(label);
      }
    }
  }
  engine.show();
}
}

void runLightsOut(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "LIGHTS OUT", "L/R cell SEL")) return;
    uint8_t cells[9];
    for (uint8_t i = 0; i < 9; i++) cells[i] = random(0, 2);
    uint8_t cursor = 0;
    uint16_t moves = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = cursorStep(engine, cursor, 9);
      if (selectTap(engine)) {
        int8_t x = cursor % 3;
        int8_t y = cursor / 3;
        int8_t dx[5] = {0, -1, 1, 0, 0};
        int8_t dy[5] = {0, 0, 0, -1, 1};
        for (uint8_t i = 0; i < 5; i++) {
          int8_t nx = x + dx[i];
          int8_t ny = y + dy[i];
          if (nx >= 0 && nx < 3 && ny >= 0 && ny < 3) cells[ny * 3 + nx] = !cells[ny * 3 + nx];
        }
        moves++;
      }
      bool won = true;
      for (uint8_t i = 0; i < 9; i++) if (cells[i]) won = false;
      engine.clear();
      drawMiniGrid(engine, 3, 3, 14, 43, 13);
      drawTinyBoard(engine, cells, 3, 3, cursor, 14, 43, 13);
      drawScore(engine, moves);
      engine.show();
      if (won) {
        if (!boardResult(engine, "CLEARED", moves)) return;
        break;
      }
      delay(20);
    }
  }
}

void runMinefield(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "MINEFIELD", "reveal cells")) return;
    bool mine[16] = {false};
    bool open[16] = {false};
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t p;
      do { p = random(0, 16); } while (mine[p]);
      mine[p] = true;
    }
    uint8_t cursor = 0;
    uint8_t safe = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = cursorStep(engine, cursor, 16);
      if (selectTap(engine) && !open[cursor]) {
        if (mine[cursor]) {
          engine.ledPulse(CRGB::Red, 260);
          if (!resultScreen(engine, "BOOM", safe)) return;
          break;
        }
        open[cursor] = true;
        safe++;
        engine.ledPulse(CRGB::Green, 60);
        engine.playSound(SOUND_SCORE);
      }
      engine.clear();
      for (uint8_t i = 0; i < 16; i++) {
        uint8_t x = i % 4;
        uint8_t y = i / 4;
        int16_t px = 36 + x * 14;
        int16_t py = 8 + y * 14;
        engine.screen().drawRect(px, py, 12, 12, GAMER_WHITE);
        if (open[i]) engine.screen().fillCircle(px + 6, py + 6, 2, GAMER_WHITE);
        if (i == cursor) drawCursorBox(engine, px, py, 12, 12);
      }
      drawScore(engine, safe);
      engine.show();
      if (safe == 12) {
        if (!boardResult(engine, "SAFE", safe)) return;
        break;
      }
      delay(20);
    }
  }
}

void runSokobanMicro(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "SOKOBAN", "L/R turn SEL")) return;
    int8_t px = 1, py = 1, bx = 3, by = 2, tx = 6, ty = 4;
    uint8_t face = 1;
    uint16_t moves = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (engine.wasPressed(BTN_LEFT)) {
        face = (face + 3) % 4;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT)) {
        face = (face + 1) % 4;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (selectTap(engine)) {
        int8_t dx = face == 1 ? 1 : (face == 3 ? -1 : 0);
        int8_t dy = face == 2 ? 1 : (face == 0 ? -1 : 0);
        int8_t nx = px + dx, ny = py + dy;
        if (nx == bx && ny == by) {
          int8_t nbx = bx + dx, nby = by + dy;
          if (nbx > 0 && nbx < 8 && nby > 0 && nby < 6) {
            bx = nbx; by = nby; px = nx; py = ny; moves++;
          }
        } else if (nx > 0 && nx < 8 && ny > 0 && ny < 6) {
          px = nx; py = ny; moves++;
        }
      }
      engine.clear();
      engine.screen().drawRect(10, 8, 9 * 12, 7 * 8, GAMER_WHITE);
      engine.screen().drawCircle(10 + tx * 12 + 6, 8 + ty * 8 + 4, 4, GAMER_WHITE);
      engine.screen().fillRect(10 + bx * 12 + 2, 8 + by * 8 + 1, 8, 6, GAMER_WHITE);
      engine.screen().drawRect(10 + px * 12 + 3, 8 + py * 8 + 1, 6, 6, GAMER_WHITE);
      drawScore(engine, moves);
      engine.show();
      if (bx == tx && by == ty) {
        if (!boardResult(engine, "BOX SET", moves)) return;
        break;
      }
      delay(20);
    }
  }
}

void runSlidingPuzzle(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "SLIDING", "pick tile")) return;
    uint8_t tiles[9] = {1, 2, 3, 4, 0, 5, 7, 8, 6};
    uint8_t cursor = 0;
    uint16_t moves = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = cursorStep(engine, cursor, 9);
      if (selectTap(engine)) {
        uint8_t blank = 0;
        for (uint8_t i = 0; i < 9; i++) if (tiles[i] == 0) blank = i;
        int8_t dx = abs(static_cast<int8_t>(cursor % 3) - static_cast<int8_t>(blank % 3));
        int8_t dy = abs(static_cast<int8_t>(cursor / 3) - static_cast<int8_t>(blank / 3));
        if (dx + dy == 1) {
          tiles[blank] = tiles[cursor];
          tiles[cursor] = 0;
          moves++;
        }
      }
      engine.clear();
      drawMiniGrid(engine, 3, 3, 16, 40, 10);
      for (uint8_t i = 0; i < 9; i++) {
        int16_t x = 40 + (i % 3) * 16 + 5;
        int16_t y = 10 + (i / 3) * 16 + 4;
        if (tiles[i] != 0) {
          engine.screen().setCursor(x, y);
          engine.screen().print(tiles[i]);
        }
        if (i == cursor) drawCursorBox(engine, 40 + (i % 3) * 16 + 1, 10 + (i / 3) * 16 + 1, 14, 14);
      }
      drawScore(engine, moves);
      engine.show();
      bool won = true;
      for (uint8_t i = 0; i < 8; i++) if (tiles[i] != i + 1) won = false;
      if (won) {
        if (!boardResult(engine, "SOLVED", moves)) return;
        break;
      }
      delay(20);
    }
  }
}

void runMemoryMatch(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "MEMORY", "match pairs")) return;
    uint8_t cards[8] = {1, 2, 3, 4, 1, 2, 3, 4};
    for (uint8_t i = 0; i < 20; i++) {
      uint8_t a = random(0, 8), b = random(0, 8);
      uint8_t t = cards[a]; cards[a] = cards[b]; cards[b] = t;
    }
    bool matched[8] = {false};
    bool shown[8] = {false};
    int8_t first = -1;
    uint8_t cursor = 0;
    uint8_t pairs = 0;
    uint16_t moves = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = cursorStep(engine, cursor, 8);
      if (selectTap(engine) && !matched[cursor]) {
        shown[cursor] = true;
        if (first < 0) first = cursor;
        else {
          moves++;
          if (cards[first] == cards[cursor] && first != cursor) {
            matched[first] = matched[cursor] = true;
            pairs++;
            engine.ledPulse(CRGB::Green, 80);
            engine.playSound(SOUND_SCORE);
          } else {
            engine.playSound(SOUND_ERROR);
            engine.show();
            delay(350);
            shown[first] = shown[cursor] = false;
          }
          first = -1;
        }
      }
      engine.clear();
      for (uint8_t i = 0; i < 8; i++) {
        int16_t x = 24 + (i % 4) * 20;
        int16_t y = 14 + (i / 4) * 20;
        engine.screen().drawRect(x, y, 16, 16, GAMER_WHITE);
        if (shown[i] || matched[i]) {
          engine.screen().setCursor(x + 5, y + 4);
          engine.screen().print(cards[i]);
        }
        if (i == cursor) drawCursorBox(engine, x, y, 16, 16);
      }
      drawScore(engine, moves);
      engine.show();
      if (pairs == 4) {
        if (!boardResult(engine, "MATCHED", moves)) return;
        break;
      }
      delay(20);
    }
  }
}

void runSimon(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "SIMON", "L/R/SEL repeat")) return;
    uint8_t seq[16];
    for (uint8_t i = 0; i < 16; i++) seq[i] = random(0, 3);
    uint8_t level = 1;
    bool failed = false;
    while (!failed && level <= 16) {
      for (uint8_t i = 0; i < level; i++) {
        engine.clear();
        const char* label = seq[i] == 0 ? "LEFT" : (seq[i] == 1 ? "RIGHT" : "SELECT");
        engine.centerText(label, 28);
        engine.show();
        engine.ledPulse(seq[i] == 0 ? CRGB::Blue : (seq[i] == 1 ? CRGB::Green : CRGB::Purple), 260);
        engine.playSound(seq[i] == 0 ? SOUND_SIMON_LEFT :
                         (seq[i] == 1 ? SOUND_SIMON_RIGHT : SOUND_SIMON_SELECT));
        delay(420);
        engine.clear();
        engine.show();
        delay(120);
      }
      for (uint8_t i = 0; i < level; i++) {
        uint8_t input = 9;
        while (input == 9) {
          engine.tick();
          if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
          if (engine.wasPressed(BTN_LEFT)) {
            input = 0;
            engine.playSound(SOUND_SIMON_LEFT);
          }
          if (engine.wasPressed(BTN_RIGHT)) {
            input = 1;
            engine.playSound(SOUND_SIMON_RIGHT);
          }
          if (selectTap(engine)) {
            input = 2;
            engine.playSound(SOUND_SIMON_SELECT);
          }
          delay(10);
        }
        if (input != seq[i]) {
          failed = true;
          break;
        }
      }
      if (!failed) {
        level++;
        engine.ledPulse(CRGB::Green, 180);
        engine.playSound(SOUND_SCORE);
      }
    }
    engine.playSound(failed ? SOUND_LOSE : SOUND_WIN);
    if (!resultScreen(engine, failed ? "MISSED" : "PERFECT", level - 1)) return;
  }
}

void runMastermind(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "MASTERMIND", "3 digits 0-2")) return;
    uint8_t secret[3] = {static_cast<uint8_t>(random(0, 3)), static_cast<uint8_t>(random(0, 3)), static_cast<uint8_t>(random(0, 3))};
    uint8_t guess[3] = {0, 0, 0};
    uint8_t pos = 0;
    uint8_t attempts = 0;
    uint8_t lastExact = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (engine.wasPressed(BTN_LEFT)) {
        guess[pos] = guess[pos] == 0 ? 2 : guess[pos] - 1;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT)) {
        guess[pos] = (guess[pos] + 1) % 3;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (selectTap(engine)) {
        pos++;
        if (pos >= 3) {
          pos = 0;
          attempts++;
          lastExact = 0;
          for (uint8_t i = 0; i < 3; i++) if (guess[i] == secret[i]) lastExact++;
          if (lastExact == 3) {
            if (!boardResult(engine, "CRACKED", attempts)) return;
            break;
          }
          if (attempts >= 9) {
            engine.ledPulse(CRGB::Red, 220);
            engine.playSound(SOUND_LOSE);
            if (!resultScreen(engine, "LOCKED", lastExact)) return;
            break;
          }
        }
      }
      engine.clear();
      engine.centerText("GUESS", 8);
      for (uint8_t i = 0; i < 3; i++) {
        engine.screen().setCursor(45 + i * 14, 28);
        engine.screen().print(guess[i]);
        if (i == pos) drawCursorBox(engine, 42 + i * 14, 24, 12, 14);
      }
      engine.screen().setCursor(0, 54);
      engine.screen().print("Exact:");
      engine.screen().print(lastExact);
      drawScore(engine, attempts);
      engine.show();
      delay(20);
    }
  }
}

void runNumberGuess(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "NUMBER GUESS", "L/R set SEL")) return;
    uint8_t target = random(0, 32);
    uint8_t guess = 16;
    uint8_t tries = 0;
    int8_t hint = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (engine.wasPressed(BTN_LEFT) && guess > 0) {
        guess--;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT) && guess < 31) {
        guess++;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (selectTap(engine)) {
        tries++;
        if (guess == target) {
          if (!boardResult(engine, "FOUND", tries)) return;
          break;
        }
        engine.playSound(SOUND_ERROR);
        hint = guess < target ? 1 : -1;
      }
      engine.clear();
      engine.centerText("PICK 0-31", 8);
      char text[12];
      snprintf(text, sizeof(text), "%u", guess);
      engine.centerText(text, 28, 2);
      engine.centerText(hint == 0 ? "SEL try" : (hint > 0 ? "HIGHER" : "LOWER"), 52);
      engine.show();
      delay(20);
    }
  }
}

void runTwentyFortyEight(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "2048", "swipe arrows")) return;
    uint8_t board[4][4] = {};
    uint16_t score = 0;
    bool won = false;
    add2048Tile(board);
    add2048Tile(board);

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }

      GamerDirection direction = GAMER_DIR_UP;
      bool hasMove = true;
      if (engine.wasDirectionPressed(GAMER_DIR_UP)) direction = GAMER_DIR_UP;
      else if (engine.wasDirectionPressed(GAMER_DIR_DOWN)) direction = GAMER_DIR_DOWN;
      else if (engine.wasDirectionPressed(GAMER_DIR_LEFT)) direction = GAMER_DIR_LEFT;
      else if (engine.wasDirectionPressed(GAMER_DIR_RIGHT)) direction = GAMER_DIR_RIGHT;
      else hasMove = false;

      if (hasMove) {
        if (move2048(board, direction, score)) {
          add2048Tile(board);
          engine.ledPulse(CRGB::Aqua, 60);
          engine.playSound(SOUND_UI_MOVE);
        } else {
          engine.playSound(SOUND_ERROR);
        }
      }

      draw2048Board(engine, board, score);

      if (!won && has2048Tile(board)) {
        won = true;
        if (!boardResult(engine, "2048!", min<uint16_t>(score, 32767))) return;
        break;
      }
      if (!canMove2048(board)) {
        engine.ledPulse(CRGB::Red, 220);
        engine.playSound(SOUND_LOSE);
        if (!resultScreen(engine, "NO MOVES", min<uint16_t>(score, 32767))) return;
        break;
      }
      delay(20);
    }
  }
}

void runFloodFill(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "FLOOD FILL", "L/R color SEL")) return;
    uint8_t board[5][5];
    for (uint8_t y = 0; y < 5; y++) for (uint8_t x = 0; x < 5; x++) board[y][x] = random(0, 3);
    uint8_t pick = 0;
    uint8_t moves = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (engine.wasPressed(BTN_LEFT)) {
        pick = pick == 0 ? 2 : pick - 1;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT)) {
        pick = (pick + 1) % 3;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (selectTap(engine)) {
        uint8_t from = board[0][0];
        flood(board, 0, 0, from, pick);
        moves++;
      }
      engine.clear();
      for (uint8_t y = 0; y < 5; y++) {
        for (uint8_t x = 0; x < 5; x++) drawCellValue(engine, 35 + x * 11, 8 + y * 11, board[y][x], 10);
      }
      drawCellValue(engine, 8, 44, pick, 12);
      drawScore(engine, moves);
      engine.show();
      if (allSame(board)) {
        if (!boardResult(engine, "FILLED", moves)) return;
        break;
      }
      delay(20);
    }
  }
}

void runBoxPush(GamerEngine& engine) { runSokobanMicro(engine); }

void runLaserMirror(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "LASER MIRROR", "rotate mirrors")) return;
    uint8_t mirror[9] = {0, 1, 0, 1, 0, 1, 0, 1, 0};
    uint8_t cursor = 0;
    uint8_t moves = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = cursorStep(engine, cursor, 9);
      if (selectTap(engine)) {
        mirror[cursor] = !mirror[cursor];
        moves++;
      }
      int8_t x = 0, y = 1, dx = 1, dy = 0;
      bool hitGoal = false;
      for (uint8_t step = 0; step < 12; step++) {
        x += dx; y += dy;
        if (x == 4 && y == 1) hitGoal = true;
        if (x < 1 || x > 3 || y < 0 || y > 2) break;
        uint8_t m = mirror[(y * 3) + (x - 1)];
        if (m == 0) {
          int8_t t = dx; dx = -dy; dy = -t;
        } else {
          int8_t t = dx; dx = dy; dy = t;
        }
      }
      engine.clear();
      engine.screen().setCursor(0, 28);
      engine.screen().print(">");
      engine.screen().setCursor(116, 28);
      engine.screen().print("X");
      for (uint8_t i = 0; i < 9; i++) {
        int16_t px = 40 + (i % 3) * 16;
        int16_t py = 14 + (i / 3) * 14;
        engine.screen().drawLine(px + 3, mirror[i] ? py + 3 : py + 10, px + 12, mirror[i] ? py + 10 : py + 3, GAMER_WHITE);
        if (i == cursor) drawCursorBox(engine, px, py, 14, 12);
      }
      drawScore(engine, moves);
      engine.show();
      if (hitGoal) {
        if (!boardResult(engine, "ALIGNED", moves)) return;
        break;
      }
      delay(20);
    }
  }
}
