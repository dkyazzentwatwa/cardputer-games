#include "Games.h"
#include "GameUtils.h"

namespace {
uint8_t win3(const uint8_t board[9]) {
  const uint8_t lines[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
    {0, 4, 8}, {2, 4, 6}
  };
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t a = board[lines[i][0]];
    if (a != 0 && a == board[lines[i][1]] && a == board[lines[i][2]]) return a;
  }
  return 0;
}

bool full3(const uint8_t board[9]) {
  for (uint8_t i = 0; i < 9; i++) if (board[i] == 0) return false;
  return true;
}

void drawMark(GamerEngine& engine, int16_t x, int16_t y, uint8_t mark) {
  if (mark == 1) {
    engine.screen().drawLine(x + 3, y + 3, x + 11, y + 11, GAMER_WHITE);
    engine.screen().drawLine(x + 11, y + 3, x + 3, y + 11, GAMER_WHITE);
  } else if (mark == 2) {
    engine.screen().drawCircle(x + 7, y + 7, 5, GAMER_WHITE);
  }
}

bool winConnect(const uint8_t board[4][4], uint8_t who) {
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (board[y][x] != who) continue;
      if (x == 0 && board[y][1] == who && board[y][2] == who && board[y][3] == who) return true;
      if (y == 0 && board[1][x] == who && board[2][x] == who && board[3][x] == who) return true;
      if (x == 0 && y == 0 && board[1][1] == who && board[2][2] == who && board[3][3] == who) return true;
      if (x == 3 && y == 0 && board[1][2] == who && board[2][1] == who && board[3][0] == who) return true;
    }
  }
  return false;
}

bool dropDisc(uint8_t board[4][4], uint8_t col, uint8_t who) {
  for (int8_t y = 3; y >= 0; y--) {
    if (board[y][col] == 0) {
      board[y][col] = who;
      return true;
    }
  }
  return false;
}

void drawReflexBar(GamerEngine& engine, int16_t pos, int16_t target, int16_t score) {
  engine.clear();
  engine.screen().drawRect(gameX(engine, 8), gameY(engine, 28), gameX(engine, 112),
                           gameSize(engine, 8), GAMER_WHITE);
  engine.screen().fillRect(gameX(engine, target - 5), gameY(engine, 26), gameSize(engine, 10),
                           gameSize(engine, 12), GAMER_ACCENT);
  engine.screen().fillRect(gameX(engine, pos), gameY(engine, 24), gameSize(engine, 3),
                           gameSize(engine, 16), GAMER_INVERSE);
  drawScore(engine, score);
  engine.show();
}

void runWaitGame(GamerEngine& engine, const char* title, bool allowLeftRight) {
  while (true) {
    if (!runIntro(engine, title, "wait for GO")) return;
    uint32_t start = millis();
    uint32_t readyAt = start + random(900, 2800);
    bool early = false;
    while (millis() < readyAt) {
      engine.tick();
      if (checkExit(engine)) return;
      if (selectTap(engine) || (allowLeftRight && (engine.wasPressed(BTN_LEFT) || engine.wasPressed(BTN_RIGHT)))) {
        early = true;
        engine.playSound(SOUND_ERROR);
      }
      engine.clear();
      engine.centerText("WAIT", gameY(engine, 26), GAMER_DISPLAY_IS_SH8601 ? 4 : 2);
      engine.show();
      delay(10);
    }
    if (early) {
      engine.ledPulse(CRGB::Red, 240);
      if (!resultScreen(engine, "TOO SOON", 0)) return;
      continue;
    }
    uint32_t goAt = millis();
    engine.playSound(SOUND_TIMER_GO);
    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      engine.clear();
      engine.centerText("GO", gameY(engine, 24), GAMER_DISPLAY_IS_SH8601 ? 5 : 2);
      engine.show();
      bool pressed = selectTap(engine) || (allowLeftRight && (engine.wasPressed(BTN_LEFT) || engine.wasPressed(BTN_RIGHT)));
      if (pressed) {
        int16_t ms = millis() - goAt;
        engine.ledPulse(CRGB::Green, 180);
        engine.playSound(SOUND_SCORE);
        if (!resultScreen(engine, "TIME MS", ms)) return;
        break;
      }
      delay(5);
    }
  }
}
}

void runTicTacToe(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "TIC TAC TOE", "L/R cell SEL")) return;
    uint8_t board[9] = {0};
    uint8_t cursor = 0;
    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      cursor = gameCursorStep(engine, cursor, 9);
      if (selectTap(engine) && board[cursor] == 0) {
        board[cursor] = 1;
        engine.playSound(SOUND_SCORE);
        if (win3(board) == 0 && !full3(board)) {
          uint8_t open[9];
          uint8_t count = 0;
          for (uint8_t i = 0; i < 9; i++) if (board[i] == 0) open[count++] = i;
          board[open[random(0, count)]] = 2;
        }
      }
      engine.clear();
      drawMiniGrid(engine, 3, 3, 16, 40, 10);
      for (uint8_t i = 0; i < 9; i++) {
        drawMark(engine, 40 + (i % 3) * 16 + 1, 10 + (i / 3) * 16 + 1, board[i]);
        if (i == cursor) drawCursorBox(engine, 40 + (i % 3) * 16 + 1, 10 + (i / 3) * 16 + 1, 14, 14);
      }
      engine.show();
      uint8_t winner = win3(board);
      if (winner || full3(board)) {
        if (!resultScreen(engine, winner == 1 ? "YOU WIN" : (winner == 2 ? "CPU WINS" : "DRAW"), winner)) return;
        break;
      }
      delay(20);
    }
  }
}

void runConnectFour(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "CONNECT FOUR", "L/R col SEL")) return;
    uint8_t board[4][4] = {{0}};
    uint8_t col = 0;
    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      col = gameCursorStep(engine, col, 4);
      if (selectTap(engine) && dropDisc(board, col, 1)) {
        engine.playSound(SOUND_SCORE);
        if (!winConnect(board, 1)) {
          for (uint8_t tries = 0; tries < 8; tries++) {
            uint8_t cpu = random(0, 4);
            if (dropDisc(board, cpu, 2)) break;
          }
        }
      }
      engine.clear();
      for (uint8_t y = 0; y < 4; y++) {
        for (uint8_t x = 0; x < 4; x++) {
          int16_t px = 36 + x * 15;
          int16_t py = 8 + y * 13;
          engine.screen().drawCircle(px + 6, py + 6, 5, GAMER_WHITE);
          if (board[y][x] == 1) engine.screen().fillCircle(px + 6, py + 6, 3, GAMER_WHITE);
          if (board[y][x] == 2) engine.screen().drawLine(px + 2, py + 2, px + 10, py + 10, GAMER_WHITE);
        }
      }
      engine.screen().drawFastHLine(36 + col * 15, 61, 12, GAMER_WHITE);
      engine.show();
      if (winConnect(board, 1) || winConnect(board, 2)) {
        bool won = winConnect(board, 1);
        if (!resultScreen(engine, won ? "YOU WIN" : "CPU WINS", won ? 1 : 0)) return;
        break;
      }
      delay(20);
    }
  }
}

void runNim(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "NIM", "take 1-3")) return;
    uint8_t pile = 15;
    uint8_t take = 1;
    while (pile > 0) {
      engine.tick();
      if (checkExit(engine)) return;
      if (engine.wasPressed(BTN_LEFT) && take > 1) {
        take--;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (engine.wasPressed(BTN_RIGHT) && take < 3) {
        take++;
        engine.playSound(SOUND_UI_MOVE);
      }
      if (selectTap(engine)) {
        if (take > pile) take = pile;
        pile -= take;
        engine.playSound(SOUND_SCORE);
        if (pile == 0) {
          if (!resultScreen(engine, "YOU WIN", pile)) return;
          break;
        }
        uint8_t cpu = (pile - 1) % 4;
        if (cpu == 0) cpu = 1;
        if (cpu > pile) cpu = pile;
        pile -= cpu;
        if (pile == 0) {
          if (!resultScreen(engine, "CPU WINS", pile)) return;
          break;
        }
      }
      engine.clear();
      engine.centerText("PILE", gameY(engine, 6), engine.textScale());
      for (uint8_t i = 0; i < pile; i++) {
        engine.screen().fillRect(gameX(engine, 18 + (i % 10) * 9), gameY(engine, 22 + (i / 10) * 10),
                                 gameSize(engine, 5), gameSize(engine, 7), GAMER_WHITE);
      }
      engine.screen().setTextSize(engine.textScale());
      engine.screen().setCursor(0, gameY(engine, 54));
      engine.screen().print("Take:");
      engine.screen().print(take);
      drawScore(engine, pile);
      engine.show();
      delay(20);
    }
  }
}

void runDotsBoxesLite(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "DOTS BOXES", "claim edges")) return;
    bool edge[12] = {false};
    uint8_t cursor = 0;
    uint8_t claimed = 0;
    while (claimed < 12) {
      engine.tick();
      if (checkExit(engine)) return;
      cursor = gameCursorStep(engine, cursor, 12);
      if (selectTap(engine) && !edge[cursor]) {
        edge[cursor] = true;
        claimed++;
        engine.playSound(SOUND_SCORE);
        if (claimed < 12) {
          for (uint8_t tries = 0; tries < 18; tries++) {
            uint8_t cpu = random(0, 12);
            if (!edge[cpu]) {
              edge[cpu] = true;
              claimed++;
              break;
            }
          }
        }
      }
      engine.clear();
      for (uint8_t y = 0; y < 3; y++) for (uint8_t x = 0; x < 3; x++) engine.screen().fillCircle(44 + x * 18, 14 + y * 18, 2, GAMER_WHITE);
      for (uint8_t i = 0; i < 12; i++) {
        bool horiz = i < 6;
        uint8_t n = horiz ? i : i - 6;
        int16_t x = 44 + (n % 2) * 18;
        int16_t y = 14 + (n / 2) * 18;
        if (horiz && edge[i]) engine.screen().drawFastHLine(x, y, 18, GAMER_WHITE);
        if (!horiz && edge[i]) engine.screen().drawFastVLine(44 + (n % 3) * 18, 14 + (n / 3) * 18, 18, GAMER_WHITE);
        if (i == cursor) {
          if (horiz) engine.screen().drawRect(x + 3, y - 3, 12, 6, GAMER_WHITE);
          else engine.screen().drawRect(44 + (n % 3) * 18 - 3, 14 + (n / 3) * 18 + 3, 6, 12, GAMER_WHITE);
        }
      }
      drawScore(engine, claimed);
      engine.show();
      delay(20);
    }
    if (!resultScreen(engine, "DONE", claimed)) return;
  }
}

void runReactionTimer(GamerEngine& engine) { runWaitGame(engine, "REACTION", false); }
void runQuickDraw(GamerEngine& engine) { runWaitGame(engine, "QUICK DRAW", true); }

void runStopTheBar(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "STOP BAR", "stop center")) return;
    int16_t pos = 8;
    int8_t dir = 3;
    int16_t score = 0;
    uint32_t nextFrame = 0;
    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      if (selectTap(engine)) {
        int16_t diff = abs(pos - 63);
        score = 50 - diff;
        if (score < 0) score = 0;
        engine.playSound(score > 42 ? SOUND_WIN : SOUND_SCORE);
        if (!resultScreen(engine, "STOPPED", score)) return;
        break;
      }
      if (!frameDue(nextFrame, 22)) { delay(2); continue; }
      pos += dir;
      if (pos < 8 || pos > 117) dir = -dir;
      drawReflexBar(engine, pos, 63, score);
    }
  }
}

void runStackTower(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "STACK TOWER", "SEL lock")) return;
    int16_t x = 0;
    int8_t dir = 4;
    int16_t baseX = 44;
    int16_t width = 34;
    int16_t y = 56;
    uint8_t rows = 0;
    uint32_t nextFrame = 0;
    while (true) {
      engine.tick();
      if (checkExit(engine)) return;
      if (selectTap(engine)) {
        int16_t left = x > baseX ? x : baseX;
        int16_t right = (x + width) < (baseX + width) ? (x + width) : (baseX + width);
        width = right - left;
        if (width <= 3) {
          engine.ledPulse(CRGB::Red, 220);
          engine.playSound(SOUND_LOSE);
          if (!resultScreen(engine, "TOPPLED", rows)) return;
          break;
        }
        baseX = left;
        y -= 6;
        rows++;
        engine.playSound(SOUND_SCORE);
        x = random(0, 80);
        if (y < 12) {
          if (!resultScreen(engine, "TOWER", rows)) return;
          break;
        }
      }
      if (!frameDue(nextFrame, 28)) { delay(2); continue; }
      x += dir;
      if (x < 0 || x + width > 128) dir = -dir;
      engine.clear();
      engine.screen().fillRect(gameX(engine, baseX), gameY(engine, y + 6), gameSize(engine, width),
                               gameSize(engine, 5), GAMER_WHITE);
      engine.screen().drawRect(gameX(engine, x), gameY(engine, y), gameSize(engine, width),
                               gameSize(engine, 5), GAMER_ACCENT);
      drawScore(engine, rows);
      engine.show();
    }
  }
}

void runLockPick(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "LOCK PICK", "hit the gap")) return;
    int16_t mark = 0;
    int16_t target = random(15, 113);
    int8_t dir = 3;
    uint8_t pins = 0;
    uint32_t nextFrame = 0;
    while (pins < 5) {
      engine.tick();
      if (checkExit(engine)) return;
      if (selectTap(engine)) {
        if (abs(mark - target) <= 5) {
          pins++;
          target = random(15, 113);
          engine.ledPulse(CRGB::Green, 90);
          engine.playSound(SOUND_SCORE);
        } else {
          engine.ledPulse(CRGB::Red, 160);
          engine.playSound(SOUND_ERROR);
          if (!resultScreen(engine, "MISSED", pins)) return;
          break;
        }
      }
      if (!frameDue(nextFrame, 24)) { delay(2); continue; }
      mark += dir;
      if (mark < 8 || mark > 120) dir = -dir;
      drawReflexBar(engine, mark, target, pins);
    }
    if (pins >= 5 && !resultScreen(engine, "OPEN", pins)) return;
  }
}

void runPixelWhack(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "PIXEL WHACK", "find target")) return;
    uint8_t cursor = 0;
    uint8_t target = random(0, 12);
    uint8_t score = 0;
    while (score < 15) {
      engine.tick();
      if (checkExit(engine)) return;
      cursor = gameCursorStep(engine, cursor, 12);
      if (selectTap(engine)) {
        if (cursor == target) {
          score++;
          target = random(0, 12);
          engine.ledPulse(CRGB::Green, 60);
          engine.playSound(SOUND_SCORE);
        } else {
          engine.ledPulse(CRGB::Red, 80);
          engine.playSound(SOUND_ERROR);
        }
      }
      engine.clear();
      for (uint8_t i = 0; i < 12; i++) {
        int16_t x = gameX(engine, 24 + (i % 4) * 20);
        int16_t y = gameY(engine, 12 + (i / 4) * 15);
        int16_t w = gameSize(engine, 14);
        int16_t h = gameSize(engine, 12);
        if (i == target) engine.screen().fillRect(x + w / 3, y + h / 3, w / 3, h / 3, GAMER_ACCENT);
        engine.screen().drawRect(x, y, w, h, GAMER_WHITE);
        if (i == cursor) drawCursorBox(engine, x, y, w, h);
      }
      drawScore(engine, score);
      engine.show();
      delay(20);
    }
    if (!resultScreen(engine, "CLEARED", score)) return;
  }
}

void runPulseMatch(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "PULSE MATCH", "match target")) return;
    int16_t pos = 8;
    int8_t dir = 2;
    int16_t target = random(24, 104);
    uint8_t score = 0;
    uint32_t nextFrame = 0;
    while (score < 10) {
      engine.tick();
      if (checkExit(engine)) return;
      if (selectTap(engine)) {
        if (abs(pos - target) < 7) {
          score++;
          target = random(24, 104);
          engine.ledPulse(CRGB::Green, 120);
          engine.playSound(SOUND_SCORE);
        } else {
          engine.ledPulse(CRGB::Red, 120);
          engine.playSound(SOUND_ERROR);
        }
      }
      if (!frameDue(nextFrame, 30)) { delay(2); continue; }
      pos += dir;
      if (pos < 8 || pos > 117) dir = -dir;
      CRGB color = abs(pos - target) < 7 ? CRGB::Green : CRGB::Blue;
      engine.ledPulse(color, 35);
      drawReflexBar(engine, pos, target, score);
    }
    if (!resultScreen(engine, "MATCHED", score)) return;
  }
}
