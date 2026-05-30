#include "Games.h"
#include "GameUtils.h"

namespace {

// =========================================================================
// Shared board helpers
// =========================================================================
uint8_t win3(const uint8_t board[9]) {
  static const uint8_t lines[8][3] = {
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

// --- Tic Tac Toe AI (player=1, cpu=2) ------------------------------------
// Minimax with full-depth search (perfect play). Score from cpu's view.
int8_t tttMinimax(uint8_t board[9], bool cpuTurn, int8_t depth) {
  uint8_t w = win3(board);
  if (w == 2) return 10 - depth;
  if (w == 1) return depth - 10;
  if (full3(board)) return 0;
  int8_t best = cpuTurn ? -127 : 127;
  for (uint8_t i = 0; i < 9; i++) {
    if (board[i] != 0) continue;
    board[i] = cpuTurn ? 2 : 1;
    int8_t v = tttMinimax(board, !cpuTurn, depth + 1);
    board[i] = 0;
    if (cpuTurn) { if (v > best) best = v; }
    else { if (v < best) best = v; }
  }
  return best;
}

uint8_t tttBestMove(uint8_t board[9]) {
  int8_t best = -127;
  uint8_t move = 0;
  for (uint8_t i = 0; i < 9; i++) {
    if (board[i] != 0) continue;
    board[i] = 2;
    int8_t v = tttMinimax(board, false, 0);
    board[i] = 0;
    if (v > best) { best = v; move = i; }
  }
  return move;
}

// Normal: try win, then block, then center, then corner, then random.
uint8_t tttHeuristic(uint8_t board[9]) {
  for (uint8_t who = 2; who >= 1; who--) {  // 2=win, 1=block
    for (uint8_t i = 0; i < 9; i++) {
      if (board[i] != 0) continue;
      board[i] = who;
      bool w = (win3(board) == who);
      board[i] = 0;
      if (w) return i;
    }
  }
  if (board[4] == 0) return 4;
  static const uint8_t corners[4] = {0, 2, 6, 8};
  uint8_t pool[9], n = 0;
  for (uint8_t i = 0; i < 4; i++) if (board[corners[i]] == 0) pool[n++] = corners[i];
  if (n > 0) return pool[random(0, n)];
  n = 0;
  for (uint8_t i = 0; i < 9; i++) if (board[i] == 0) pool[n++] = i;
  return pool[random(0, n)];
}

uint8_t tttRandom(uint8_t board[9]) {
  uint8_t pool[9], n = 0;
  for (uint8_t i = 0; i < 9; i++) if (board[i] == 0) pool[n++] = i;
  return pool[random(0, n)];
}

void tttCpuMove(uint8_t board[9], uint8_t diff) {
  uint8_t m;
  if (diff == 0) m = tttRandom(board);
  else if (diff == 1) m = tttHeuristic(board);
  else m = tttBestMove(board);
  board[m] = 2;
}

// =========================================================================
// Connect Four (4x4) helpers and AI
// =========================================================================
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

int8_t c4DropRow(const uint8_t board[4][4], uint8_t col) {
  for (int8_t y = 3; y >= 0; y--) if (board[y][col] == 0) return y;
  return -1;
}

bool dropDisc(uint8_t board[4][4], uint8_t col, uint8_t who) {
  int8_t y = c4DropRow(board, col);
  if (y < 0) return false;
  board[y][col] = who;
  return true;
}

bool c4Tie(const uint8_t board[4][4]) {
  for (uint8_t x = 0; x < 4; x++) if (board[0][x] == 0) return false;
  return true;
}

// Minimax for Connect Four (depth-limited). who-to-move toggles cpu(2)/player(1).
int8_t c4Minimax(uint8_t board[4][4], bool cpuTurn, int8_t depth, int8_t maxDepth) {
  if (winConnect(board, 2)) return 20 - depth;
  if (winConnect(board, 1)) return depth - 20;
  if (c4Tie(board) || depth >= maxDepth) return 0;
  int8_t best = cpuTurn ? -127 : 127;
  for (uint8_t c = 0; c < 4; c++) {
    int8_t y = c4DropRow(board, c);
    if (y < 0) continue;
    board[y][c] = cpuTurn ? 2 : 1;
    int8_t v = c4Minimax(board, !cpuTurn, depth + 1, maxDepth);
    board[y][c] = 0;
    if (cpuTurn) { if (v > best) best = v; }
    else { if (v < best) best = v; }
  }
  return best;
}

uint8_t c4RandomCol(const uint8_t board[4][4]) {
  uint8_t pool[4], n = 0;
  for (uint8_t c = 0; c < 4; c++) if (c4DropRow(board, c) >= 0) pool[n++] = c;
  return n ? pool[random(0, n)] : 0;
}

// Normal: immediate win, else block, else center-biased.
uint8_t c4Normal(uint8_t board[4][4]) {
  for (uint8_t who = 2; who >= 1; who--) {
    for (uint8_t c = 0; c < 4; c++) {
      int8_t y = c4DropRow(board, c);
      if (y < 0) continue;
      board[y][c] = who;
      bool w = winConnect(board, who);
      board[y][c] = 0;
      if (w) return c;
    }
    if (who == 1) break;
  }
  static const uint8_t order[4] = {1, 2, 0, 3};
  for (uint8_t i = 0; i < 4; i++) if (c4DropRow(board, order[i]) >= 0) return order[i];
  return c4RandomCol(board);
}

uint8_t c4Best(uint8_t board[4][4]) {
  int8_t best = -127;
  uint8_t move = 0;
  static const uint8_t order[4] = {1, 2, 0, 3};
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t c = order[i];
    int8_t y = c4DropRow(board, c);
    if (y < 0) continue;
    board[y][c] = 2;
    int8_t v = c4Minimax(board, false, 0, 7);
    board[y][c] = 0;
    if (v > best) { best = v; move = c; }
  }
  return move;
}

void c4CpuMove(uint8_t board[4][4], uint8_t diff) {
  uint8_t c;
  if (diff == 0) c = c4RandomCol(board);
  else if (diff == 1) c = c4Normal(board);
  else c = c4Best(board);
  dropDisc(board, c, 2);
}

// =========================================================================
// Reflex helpers
// =========================================================================
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

// Reaction / Quick Draw: best-of-3 average, false starts, ms feedback.
void runWaitGame(GamerEngine& engine, const char* title, bool allowLeftRight, const char* key) {
  uint8_t diff = chooseDifficulty(engine, title, "wait for GO");
  if (diff == 255) return;
  // Difficulty tunes the random delay window (Hard = shorter prompts).
  uint16_t loDelay = diff == 0 ? 1200 : (diff == 1 ? 800 : 500);
  uint16_t hiDelay = diff == 0 ? 3000 : (diff == 1 ? 2600 : 2000);
  uint16_t best = bestLoad(key);

  while (true) {
    uint32_t sum = 0;
    uint8_t round = 0;
    bool aborted = false;
    while (round < 3) {
      char hdr[20];
      snprintf(hdr, sizeof(hdr), "ROUND %u/3", (unsigned)(round + 1));
      if (!runIntro(engine, title, hdr)) return;

      uint32_t readyAt = millis() + random(loDelay, hiDelay);
      bool early = false;
      while (millis() < readyAt) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
        if (selectTap(engine) || (allowLeftRight && (engine.wasPressed(BTN_LEFT) || engine.wasPressed(BTN_RIGHT)))) {
          early = true;
          engine.playSound(SOUND_ERROR);
          break;
        }
        engine.clear();
        engine.centerText("WAIT", gameY(engine, 22), GAMER_DISPLAY_IS_SH8601 ? 4 : 2);
        drawBestTag(engine, best, 0);
        engine.show();
        delay(10);
      }
      if (early) {
        engine.ledPulse(CRGB::Red, 240);
        if (!resultScreen(engine, "TOO SOON", 0)) return;
        aborted = true;
        break;
      }

      uint32_t goAt = millis();
      engine.playSound(SOUND_TIMER_GO);
      int16_t ms = -1;
      while (ms < 0) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
        bool pressed = selectTap(engine) || (allowLeftRight && (engine.wasPressed(BTN_LEFT) || engine.wasPressed(BTN_RIGHT)));
        if (pressed) {
          ms = (int16_t)(millis() - goAt);
          if (ms < 1) ms = 1;
        }
        engine.clear();
        engine.centerText("GO", gameY(engine, 20), GAMER_DISPLAY_IS_SH8601 ? 5 : 2);
        engine.show();
        delay(4);
      }
      // Green/Yellow/Red feedback by speed.
      CRGB col = ms < 220 ? CRGB::Green : (ms < 360 ? CRGB::Yellow : CRGB::Red);
      engine.ledPulse(col, 180);
      engine.playSound(ms < 220 ? SOUND_WIN : SOUND_SCORE);
      char d[20];
      snprintf(d, sizeof(d), "%d ms", ms);
      if (!engine.showResult(ms < 220 ? "LIGHTNING" : (ms < 360 ? "GOOD" : "SLOW"), d)) return;
      sum += (uint32_t)ms;
      round++;
    }
    if (aborted) continue;

    uint16_t avg = (uint16_t)(sum / 3);
    bool rec = bestSubmitLow(key, avg);
    if (!resultScreenBest(engine, "AVG MS", avg, best, rec)) return;
    if (rec) best = avg;
  }
}

}  // namespace

// =========================================================================
// TIC TAC TOE  (Easy=random, Normal=heuristic, Hard=minimax/perfect)
// =========================================================================
void runTicTacToe(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "TIC TAC TOE", "L/R cell SEL");
  if (diff == 255) return;
  uint16_t best = bestLoad("ttt");
  uint8_t streak = 0;
  uint16_t wins = 0, losses = 0, draws = 0;
  SparkField sparks;

  while (true) {
    char sub[22];
    snprintf(sub, sizeof(sub), "W%u L%u D%u", (unsigned)wins, (unsigned)losses, (unsigned)draws);
    if (!runIntro(engine, "TIC TAC TOE", sub)) return;
    uint8_t board[9] = {0};
    uint8_t cursor = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = gameCursorStep(engine, cursor, 9);
      if (selectTap(engine) && board[cursor] == 0) {
        board[cursor] = 1;
        engine.playSound(SOUND_SCORE);
        sparksSpawn(sparks, 40 + (cursor % 3) * 16 + 7, 10 + (cursor / 3) * 16 + 7, GAMER_ACCENT, 5);
        if (win3(board) == 0 && !full3(board)) tttCpuMove(board, diff);
      }
      engine.clear();
      drawMiniGrid(engine, 3, 3, 16, 40, 10);
      for (uint8_t i = 0; i < 9; i++) {
        drawMark(engine, 40 + (i % 3) * 16 + 1, 10 + (i / 3) * 16 + 1, board[i]);
        if (i == cursor) drawCursorBox(engine, 40 + (i % 3) * 16 + 1, 10 + (i / 3) * 16 + 1, 14, 14);
      }
      sparksUpdateDraw(sparks, engine);
      drawBestTag(engine, best, 0);
      engine.show();
      uint8_t winner = win3(board);
      if (winner || full3(board)) {
        const char* t;
        if (winner == 1) {
          t = "YOU WIN"; wins++; streak++;
          engine.playSound(SOUND_WIN);
          bool rec = bestSubmit("ttt", streak);
          if (rec) best = streak;
          char d[22];
          snprintf(d, sizeof(d), "STREAK %u", (unsigned)streak);
          if (!engine.showResult(rec ? "NEW BEST!" : "YOU WIN", d)) return;
        } else if (winner == 2) {
          t = "CPU WINS"; losses++; streak = 0;
          engine.playSound(SOUND_LOSE);
          if (!resultScreen(engine, t, 0)) return;
        } else {
          t = "DRAW"; draws++; streak = 0;
          if (!resultScreen(engine, t, 0)) return;
        }
        break;
      }
      delay(20);
    }
  }
}

// =========================================================================
// CONNECT FOUR  (Easy=random, Normal=win/block+center, Hard=minimax depth7)
// =========================================================================
void runConnectFour(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "CONNECT4", "L/R col SEL");
  if (diff == 255) return;
  uint16_t best = bestLoad("connect4");
  uint8_t streak = 0;
  SparkField sparks;

  while (true) {
    char sub[20];
    snprintf(sub, sizeof(sub), "STREAK %u", (unsigned)streak);
    if (!runIntro(engine, "CONNECT FOUR", sub)) return;
    uint8_t board[4][4] = {{0}};
    uint8_t col = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      col = gameCursorStep(engine, col, 4);
      if (selectTap(engine) && dropDisc(board, col, 1)) {
        engine.playSound(SOUND_SCORE);
        sparksSpawn(sparks, 36 + col * 15 + 6, 14, GAMER_ACCENT, 5);
        if (!winConnect(board, 1)) c4CpuMove(board, diff);
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
      // Column-drop preview: ghost disc at the landing row.
      int8_t gy = c4DropRow(board, col);
      if (gy >= 0) engine.screen().drawCircle(36 + col * 15 + 6, 8 + gy * 13 + 6, 3, GAMER_ACCENT);
      engine.screen().drawFastHLine(36 + col * 15, 61, 12, GAMER_ACCENT);
      sparksUpdateDraw(sparks, engine);
      drawBestTag(engine, best, 0);
      engine.show();
      bool pw = winConnect(board, 1), cw = winConnect(board, 2);
      if (pw || cw || c4Tie(board)) {
        if (pw) {
          streak++;
          engine.playSound(SOUND_WIN);
          bool rec = bestSubmit("connect4", streak);
          if (rec) best = streak;
          char d[20];
          snprintf(d, sizeof(d), "STREAK %u", (unsigned)streak);
          if (!engine.showResult(rec ? "NEW BEST!" : "YOU WIN", d)) return;
        } else if (cw) {
          streak = 0;
          engine.playSound(SOUND_LOSE);
          if (!resultScreen(engine, "CPU WINS", 0)) return;
        } else {
          streak = 0;
          if (!resultScreen(engine, "DRAW", 0)) return;
        }
        break;
      }
      delay(20);
    }
  }
}

// =========================================================================
// NIM  (Easy=random, Hard=perfect mod-4 strategy; teaching hint)
// =========================================================================
void runNim(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "NIM", "take 1-3");
  if (diff == 255) return;
  uint16_t best = bestLoad("nim");
  uint8_t streak = 0;
  // Hard gets a larger pile variant for more depth.
  uint8_t startPile = diff == 2 ? 21 : 15;

  while (true) {
    char sub[20];
    snprintf(sub, sizeof(sub), "WIN STREAK %u", (unsigned)streak);
    if (!runIntro(engine, "NIM", sub)) return;
    uint8_t pile = startPile;
    uint8_t take = 1;
    bool finished = false;
    while (pile > 0) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (engine.wasPressed(BTN_LEFT) && take > 1) { take--; engine.playSound(SOUND_UI_MOVE); }
      if (engine.wasPressed(BTN_RIGHT) && take < 3) { take++; engine.playSound(SOUND_UI_MOVE); }
      if (selectTap(engine)) {
        if (take > pile) take = pile;
        pile -= take;
        engine.playSound(SOUND_SCORE);
        if (pile == 0) {
          streak++;
          engine.playSound(SOUND_WIN);
          bool rec = bestSubmit("nim", streak);
          if (rec) best = streak;
          char d[20];
          snprintf(d, sizeof(d), "STREAK %u", (unsigned)streak);
          if (!engine.showResult(rec ? "NEW BEST!" : "YOU WIN", d)) return;
          finished = true;
          break;
        }
        // CPU move.
        uint8_t cpu;
        if (diff == 0) {
          cpu = random(1, (pile < 3 ? pile : 3) + 1);
        } else {
          // Perfect: leave pile at multiple of 4.
          cpu = (pile - 1) % 4;
          if (cpu == 0) cpu = 1;
          if (cpu > pile) cpu = pile;
        }
        pile -= cpu;
        if (pile == 0) {
          streak = 0;
          engine.playSound(SOUND_LOSE);
          if (!resultScreen(engine, "CPU WINS", 0)) return;
          finished = true;
          break;
        }
      }
      engine.clear();
      engine.centerText("PILE", gameY(engine, 4), engine.textScale());
      for (uint8_t i = 0; i < pile; i++) {
        engine.screen().fillRect(gameX(engine, 18 + (i % 10) * 9), gameY(engine, 18 + (i / 10) * 9),
                                 gameSize(engine, 5), gameSize(engine, 7), GAMER_WHITE);
      }
      engine.screen().setTextSize(1);
      engine.screen().setCursor(gameX(engine, 2), gameY(engine, 50));
      engine.screen().print("Take:");
      engine.screen().print(take);
      // Teaching hint: the winning move leaves pile a multiple of 4.
      int8_t winMove = pile % 4;
      engine.screen().setCursor(gameX(engine, 2), gameY(engine, 58));
      if (winMove >= 1 && winMove <= 3) { engine.screen().print("Tip take "); engine.screen().print(winMove); }
      else engine.screen().print("Tip: tough spot");
      drawBestTag(engine, best, 0);
      drawScore(engine, pile);
      engine.show();
      delay(20);
    }
    if (!finished) {
      // pile hit 0 without break (shouldn't happen) — safety.
    }
  }
}

// =========================================================================
// DOTS & BOXES (2x2 boxes, 12 edges)  player=1 cpu=2
// Easy=random, Normal=complete box / avoid 3rd side, Hard=+chain awareness
// =========================================================================
namespace {
// Edge index map for a 2x2 grid (3x3 dots).
// Horizontals 0..5 (rows 0..2, cols 0..1), Verticals 6..11 (rows 0..1, cols 0..2).
// Box b in [0..3] = (br,bc). Its 4 edges:
void boxEdges(uint8_t b, uint8_t e[4]) {
  uint8_t br = b / 2, bc = b % 2;
  e[0] = br * 2 + bc;          // top horiz
  e[1] = (br + 1) * 2 + bc;    // bottom horiz
  e[2] = 6 + br * 3 + bc;      // left vert
  e[3] = 6 + br * 3 + bc + 1;  // right vert
}

uint8_t boxSides(const bool edge[12], uint8_t b) {
  uint8_t e[4]; boxEdges(b, e);
  uint8_t c = 0;
  for (uint8_t i = 0; i < 4; i++) if (edge[e[i]]) c++;
  return c;
}

// Returns count of boxes completed by playing `m`, fills `owner` map for newly
// completed boxes set to `who`.
uint8_t playEdge(bool edge[12], uint8_t owner[4], uint8_t m, uint8_t who) {
  edge[m] = true;
  uint8_t got = 0;
  for (uint8_t b = 0; b < 4; b++) {
    if (owner[b] == 0 && boxSides(edge, b) == 4) { owner[b] = who; got++; }
  }
  return got;
}

// Choose a move for the CPU. Returns edge index (assumes at least one free).
uint8_t dotsCpuPick(const bool edge[12], const uint8_t owner[4], uint8_t diff) {
  uint8_t free[12], nf = 0;
  for (uint8_t i = 0; i < 12; i++) if (!edge[i]) free[nf++] = i;

  // 1. Complete a box if possible (Normal/Hard).
  if (diff >= 1) {
    for (uint8_t i = 0; i < nf; i++) {
      bool tmp[12]; for (uint8_t k = 0; k < 12; k++) tmp[k] = edge[k];
      uint8_t to[4]; for (uint8_t k = 0; k < 4; k++) to[k] = owner[k];
      if (playEdge(tmp, to, free[i], 2) > 0) return free[i];
    }
  }
  // 2. Prefer "safe" moves that don't make any box have 3 sides.
  if (diff >= 1) {
    uint8_t safe[12], ns = 0;
    for (uint8_t i = 0; i < nf; i++) {
      bool tmp[12]; for (uint8_t k = 0; k < 12; k++) tmp[k] = edge[k];
      tmp[free[i]] = true;
      bool gives = false;
      for (uint8_t b = 0; b < 4; b++) if (owner[b] == 0 && boxSides(tmp, b) == 3) gives = true;
      if (!gives) safe[ns++] = free[i];
    }
    if (ns > 0) {
      // Hard: among safe moves still random (chain-neutral); Normal: random safe.
      return safe[random(0, ns)];
    }
    // Hard chain awareness: if forced to give, give the move opening the
    // SMALLEST chain (fewest boxes currently at 2 sides adjacent) — approx by
    // picking the move that completes the fewest 3-side boxes downstream.
    if (diff == 2) {
      uint8_t bestM = free[0]; int8_t bestCost = 127;
      for (uint8_t i = 0; i < nf; i++) {
        bool tmp[12]; for (uint8_t k = 0; k < 12; k++) tmp[k] = edge[k];
        tmp[free[i]] = true;
        int8_t cost = 0;
        for (uint8_t b = 0; b < 4; b++) if (owner[b] == 0 && boxSides(tmp, b) == 3) cost++;
        if (cost < bestCost) { bestCost = cost; bestM = free[i]; }
      }
      return bestM;
    }
  }
  // Easy / fallback: random.
  return free[random(0, nf)];
}
}  // namespace

void runDotsBoxesLite(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "DOTS BOXES", "claim edges");
  if (diff == 255) return;
  uint16_t best = bestLoad("dotsboxes");
  SparkField sparks;

  while (true) {
    if (!runIntro(engine, "DOTS BOXES", "complete boxes")) return;
    bool edge[12] = {false};
    uint8_t owner[4] = {0};
    uint8_t cursor = 0;
    uint8_t claimed = 0;
    uint8_t pScore = 0, cScore = 0;
    bool playerTurn = true;
    uint8_t flash = 0;

    while (claimed < 4) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }

      if (playerTurn) {
        cursor = gameCursorStep(engine, cursor, 12);
        if (selectTap(engine) && !edge[cursor]) {
          uint8_t got = playEdge(edge, owner, cursor, 1);
          claimed += got;
          pScore += got;
          if (got) {
            engine.playSound(SOUND_COMBO);
            flash = 6;
            for (uint8_t b = 0; b < 4; b++) if (owner[b] == 1)
              sparksSpawn(sparks, 44 + (b % 2) * 18 + 9, 14 + (b / 2) * 18 + 9, GAMER_ACCENT, 3);
          } else {
            engine.playSound(SOUND_SCORE);
            playerTurn = false;  // no box -> turn passes
          }
        }
      } else {
        // CPU turn (small delay for readability).
        delay(220);
        uint8_t m = dotsCpuPick(edge, owner, diff);
        uint8_t got = playEdge(edge, owner, m, 2);
        claimed += got;
        cScore += got;
        if (got) { engine.playSound(SOUND_HIT); flash = 6; }
        else { engine.playSound(SOUND_UI_MOVE); playerTurn = true; }
      }

      engine.clear();
      for (uint8_t y = 0; y < 3; y++) for (uint8_t x = 0; x < 3; x++)
        engine.screen().fillCircle(44 + x * 18, 14 + y * 18, 2, GAMER_WHITE);
      // Box ownership fill.
      for (uint8_t b = 0; b < 4; b++) {
        if (owner[b] == 0) continue;
        int16_t bx = 44 + (b % 2) * 18, by = 14 + (b / 2) * 18;
        if (owner[b] == 1) engine.screen().drawLine(bx + 4, by + 9, bx + 14, by + 9, GAMER_ACCENT);
        else engine.screen().drawCircle(bx + 9, by + 9, 3, GAMER_INVERSE);
      }
      for (uint8_t i = 0; i < 12; i++) {
        bool horiz = i < 6;
        uint8_t n = horiz ? i : i - 6;
        if (horiz) {
          int16_t x = 44 + (n % 2) * 18, y = 14 + (n / 2) * 18;
          if (edge[i]) engine.screen().drawFastHLine(x, y, 18, GAMER_WHITE);
          if (playerTurn && i == cursor) engine.screen().drawRect(x + 3, y - 3, 12, 6, GAMER_ACCENT);
        } else {
          int16_t x = 44 + (n % 3) * 18, y = 14 + (n / 3) * 18;
          if (edge[i]) engine.screen().drawFastVLine(x, y, 18, GAMER_WHITE);
          if (playerTurn && i == cursor) engine.screen().drawRect(x - 3, y + 3, 6, 12, GAMER_ACCENT);
        }
      }
      if (flash > 0) { engine.screen().drawRect(0, 0, engine.width(), engine.height(), GAMER_WHITE); flash--; }
      sparksUpdateDraw(sparks, engine);
      char hud[16];
      snprintf(hud, sizeof(hud), "%u-%u", (unsigned)pScore, (unsigned)cScore);
      engine.rightText(hud, 0, GAMER_WHITE);
      drawBestTag(engine, best, 9);
      engine.show();
      delay(20);
    }

    bool rec = bestSubmit("dotsboxes", pScore);
    if (rec) best = pScore;
    const char* t = pScore > cScore ? "YOU WIN" : (pScore < cScore ? "CPU WINS" : "DRAW");
    engine.playSound(pScore > cScore ? SOUND_WIN : SOUND_LOSE);
    if (!resultScreenBest(engine, t, pScore, best, rec)) return;
  }
}

// =========================================================================
// REFLEX GAMES
// =========================================================================
void runReactionTimer(GamerEngine& engine) { runWaitGame(engine, "REACTION", false, "reaction"); }
void runQuickDraw(GamerEngine& engine) { runWaitGame(engine, "QUICK DRAW", true, "quickdraw"); }

// --- Stop Bar: visible zone, multi-round, combo for perfect stops ---------
void runStopTheBar(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "STOP BAR", "stop in zone");
  if (diff == 255) return;
  uint16_t best = bestLoad("stopbar");
  int8_t speed = diff == 0 ? 2 : (diff == 1 ? 3 : 5);
  int16_t zoneHalf = diff == 0 ? 9 : (diff == 1 ? 6 : 4);  // target zone size
  SparkField sparks;

  while (true) {
    if (!runIntro(engine, "STOP BAR", "5 rounds")) return;
    int16_t pos = 8;
    int8_t dir = speed;
    int16_t total = 0;
    uint8_t combo = 0;
    uint8_t round = 0;
    uint32_t nextFrame = 0;
    while (round < 5) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (selectTap(engine)) {
        int16_t diffPx = abs(pos - 63);
        int16_t pts;
        if (diffPx <= 1) {  // perfect
          pts = 50; combo++;
          engine.ledPulse(CRGB::Green, 160);
          engine.playSound(combo >= 2 ? SOUND_COMBO : SOUND_WIN);
          sparksSpawn(sparks, gameX(engine, pos), gameY(engine, 32), GAMER_ACCENT, 8);
        } else if (diffPx <= zoneHalf) {
          pts = 50 - diffPx * 2; combo = 0;
          engine.ledPulse(CRGB::Yellow, 120);
          engine.playSound(SOUND_SCORE);
        } else {
          pts = 0; combo = 0;
          engine.ledPulse(CRGB::Red, 140);
          engine.playSound(SOUND_ERROR);
        }
        total += pts + combo * 5;
        round++;
        pos = 8; dir = speed;
        char d[18];
        snprintf(d, sizeof(d), "+%d  x%u", pts + combo * 5, (unsigned)combo);
        engine.clear();
        engine.centerText(pts == 50 ? "PERFECT" : (pts > 0 ? "OK" : "MISS"), gameY(engine, 18),
                          GAMER_DISPLAY_IS_SH8601 ? 3 : 2);
        engine.centerText(d, gameY(engine, 44), 1);
        engine.show();
        delay(450);
        continue;
      }
      if (!frameDue(nextFrame, 22)) { delay(2); continue; }
      pos += dir;
      if (pos < 8 || pos > 117) dir = -dir;
      engine.clear();
      engine.screen().drawRect(gameX(engine, 8), gameY(engine, 28), gameX(engine, 112), gameSize(engine, 8), GAMER_WHITE);
      engine.screen().fillRect(gameX(engine, 63 - zoneHalf), gameY(engine, 26), gameSize(engine, zoneHalf * 2), gameSize(engine, 12), GAMER_ACCENT);
      engine.screen().drawFastVLine(gameX(engine, 63), gameY(engine, 24), gameSize(engine, 16), GAMER_INVERSE);
      engine.screen().fillRect(gameX(engine, pos), gameY(engine, 24), gameSize(engine, 3), gameSize(engine, 16), GAMER_INVERSE);
      sparksUpdateDraw(sparks, engine);
      drawScore(engine, total);
      drawBestTag(engine, best, 0);
      engine.show();
    }
    bool rec = bestSubmit("stopbar", total > 0 ? (uint16_t)total : 0);
    if (rec) best = total;
    if (!resultScreenBest(engine, "FINAL", total, best, rec)) return;
  }
}

// --- Stack Tower: ghost preview, precision bonus, best height -------------
void runStackTower(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "STACK", "SEL lock");
  if (diff == 255) return;
  uint16_t best = bestLoad("stacktower");
  int8_t speed = diff == 0 ? 3 : (diff == 1 ? 4 : 6);
  int16_t startW = diff == 0 ? 40 : (diff == 1 ? 34 : 26);
  SparkField sparks;

  while (true) {
    if (!runIntro(engine, "STACK TOWER", "perfect=bonus")) return;
    int16_t x = 0;
    int8_t dir = speed;
    int16_t baseX = 44;
    int16_t width = startW;
    int16_t y = 56;
    uint16_t score = 0;
    uint8_t rows = 0;
    uint32_t nextFrame = 0;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (selectTap(engine)) {
        int16_t left = x > baseX ? x : baseX;
        int16_t right = (x + width) < (baseX + width) ? (x + width) : (baseX + width);
        int16_t overlap = right - left;
        if (overlap <= 1) {
          engine.ledPulse(CRGB::Red, 220);
          engine.playSound(SOUND_LOSE);
          bool rec = bestSubmit("stacktower", rows);
          if (rec) best = rows;
          if (!resultScreenBest(engine, "TOPPLED", rows, best, rec)) return;
          break;
        }
        int16_t off = abs(x - baseX);
        if (off <= 1) {  // precision bonus for near-perfect overlap
          score += 5;
          engine.ledPulse(CRGB::Green, 150);
          engine.playSound(SOUND_COMBO);
          sparksSpawn(sparks, gameX(engine, left + overlap / 2), gameY(engine, y), GAMER_ACCENT, 8);
        } else {
          score += 1;
          engine.playSound(SOUND_SCORE);
        }
        width = overlap;
        baseX = left;
        y -= 6;
        rows++;
        x = random(0, 128 - width);
        dir = (x < baseX) ? speed : -speed;
        if (y < 12) {
          engine.playSound(SOUND_WIN);
          bool rec = bestSubmit("stacktower", rows);
          if (rec) best = rows;
          if (!resultScreenBest(engine, "TOWER!", rows, best, rec)) return;
          break;
        }
      }
      if (!frameDue(nextFrame, 28)) { delay(2); continue; }
      x += dir;
      if (x < 0 || x + width > 128) dir = -dir;
      engine.clear();
      // Stacked base.
      engine.screen().fillRect(gameX(engine, baseX), gameY(engine, y + 6), gameSize(engine, width), gameSize(engine, 5), GAMER_WHITE);
      // Ghost preview of where the moving block aligns over the base.
      engine.screen().drawRect(gameX(engine, baseX), gameY(engine, y), gameSize(engine, width), gameSize(engine, 5), GAMER_DIM);
      engine.screen().drawRect(gameX(engine, x), gameY(engine, y), gameSize(engine, width), gameSize(engine, 5), GAMER_ACCENT);
      sparksUpdateDraw(sparks, engine);
      drawScore(engine, rows);
      drawBestTag(engine, best, 0);
      engine.show();
    }
  }
}

// --- Lock Pick: visual lock w/ pins filling, forgiveness, difficulty ------
void runLockPick(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "LOCK PICK", "hit the gap");
  if (diff == 255) return;
  uint16_t best = bestLoad("lockpick");
  int8_t speed = diff == 0 ? 2 : (diff == 1 ? 3 : 5);
  int16_t tol = diff == 0 ? 8 : (diff == 1 ? 5 : 3);
  uint8_t forgive = diff == 2 ? 0 : 1;  // 1 forgiveness on Easy/Normal
  SparkField sparks;

  while (true) {
    if (!runIntro(engine, "LOCK PICK", "5 pins")) return;
    int16_t mark = 0;
    int16_t target = random(20, 108);
    int8_t dir = speed;
    uint8_t pins = 0;
    uint8_t lives = forgive;
    uint32_t nextFrame = 0;
    bool done = false;
    while (pins < 5) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (selectTap(engine)) {
        if (abs(mark - target) <= tol) {
          pins++;
          target = random(20, 108);
          engine.ledPulse(CRGB::Green, 90);
          engine.playSound(SOUND_SCORE);
          sparksSpawn(sparks, gameX(engine, mark), gameY(engine, 32), GAMER_ACCENT, 5);
        } else if (lives > 0) {
          lives--;
          engine.ledPulse(CRGB::Yellow, 120);
          engine.playSound(SOUND_ERROR);
        } else {
          engine.ledPulse(CRGB::Red, 160);
          engine.playSound(SOUND_LOSE);
          bool rec = bestSubmit("lockpick", pins);
          if (rec) best = pins;
          if (!resultScreenBest(engine, "JAMMED", pins, best, rec)) return;
          done = true;
          break;
        }
      }
      if (!frameDue(nextFrame, 24)) { delay(2); continue; }
      mark += dir;
      if (mark < 8 || mark > 120) dir = -dir;
      engine.clear();
      // Lock body with pins filling at top.
      for (uint8_t p = 0; p < 5; p++) {
        int16_t px = gameX(engine, 34 + p * 12);
        if (p < pins) engine.screen().fillRect(px, gameY(engine, 6), gameSize(engine, 8), gameSize(engine, 10), GAMER_ACCENT);
        else engine.screen().drawRect(px, gameY(engine, 6), gameSize(engine, 8), gameSize(engine, 10), GAMER_WHITE);
      }
      // Picking bar.
      engine.screen().drawRect(gameX(engine, 8), gameY(engine, 28), gameX(engine, 112), gameSize(engine, 8), GAMER_WHITE);
      engine.screen().fillRect(gameX(engine, target - tol), gameY(engine, 26), gameSize(engine, tol * 2), gameSize(engine, 12), GAMER_ACCENT);
      engine.screen().fillRect(gameX(engine, mark), gameY(engine, 24), gameSize(engine, 3), gameSize(engine, 16), GAMER_INVERSE);
      if (forgive) drawLives(engine, gameX(engine, 2), gameY(engine, 50), lives);
      sparksUpdateDraw(sparks, engine);
      drawScore(engine, pins);
      drawBestTag(engine, best, 0);
      engine.show();
    }
    if (done) continue;
    engine.playSound(SOUND_WIN);
    bool rec = bestSubmit("lockpick", pins);
    if (rec) best = pins;
    if (!resultScreenBest(engine, "OPEN!", pins, best, rec)) return;
  }
}

// --- Pixel Whack: target count by difficulty, combo, best time ------------
void runPixelWhack(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "PIXEL WHACK", "find target");
  if (diff == 255) return;
  uint16_t best = bestLoad("pixelwhack");  // best time ms (lower better)
  uint8_t goal = diff == 0 ? 10 : (diff == 1 ? 15 : 20);
  uint16_t comboWindow = diff == 0 ? 1200 : (diff == 1 ? 900 : 650);
  SparkField sparks;

  while (true) {
    if (!runIntro(engine, "PIXEL WHACK", "hit fast")) return;
    uint8_t cursor = 0;
    uint8_t target = random(0, 12);
    uint8_t score = 0;
    uint8_t combo = 0;
    uint32_t start = millis();
    uint32_t lastHit = start;
    while (score < goal) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      cursor = gameCursorStep(engine, cursor, 12);
      if (selectTap(engine)) {
        if (cursor == target) {
          score++;
          uint32_t now = millis();
          if (now - lastHit < comboWindow) combo++;
          else combo = 0;
          lastHit = now;
          uint8_t t = target;
          while (t == target) target = random(0, 12);
          engine.ledPulse(CRGB::Green, 60);
          engine.playSound(combo >= 2 ? SOUND_COMBO : SOUND_SCORE);
          int16_t cx = gameX(engine, 24 + (cursor % 4) * 20 + 7);
          int16_t cy = gameY(engine, 12 + (cursor / 4) * 15 + 6);
          sparksSpawn(sparks, cx, cy, GAMER_ACCENT, combo >= 2 ? 8 : 4);
        } else {
          combo = 0;
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
      sparksUpdateDraw(sparks, engine);
      if (combo >= 2) {
        char c[12];
        snprintf(c, sizeof(c), "x%u", (unsigned)(combo + 1));
        engine.rightText(c, 0, GAMER_ACCENT);
      } else {
        drawBestTag(engine, best, 0);
      }
      drawScore(engine, score);
      engine.show();
      delay(20);
    }
    uint16_t elapsed = (uint16_t)(millis() - start);
    bool rec = bestSubmitLow("pixelwhack", elapsed);
    if (rec) best = elapsed;
    engine.playSound(SOUND_WIN);
    char d[20];
    snprintf(d, sizeof(d), "%u ms", (unsigned)elapsed);
    if (!engine.showResult(rec ? "NEW BEST!" : "CLEARED", d)) return;
  }
}

// --- Pulse Match: accelerating speed, accuracy feedback, best -------------
void runPulseMatch(GamerEngine& engine) {
  uint8_t diff = chooseDifficulty(engine, "PULSE MATCH", "match target");
  if (diff == 255) return;
  uint16_t best = bestLoad("pulsematch");
  int8_t baseSpeed = diff == 0 ? 2 : (diff == 1 ? 3 : 4);
  int16_t tol = diff == 0 ? 8 : (diff == 1 ? 6 : 4);
  SparkField sparks;

  while (true) {
    if (!runIntro(engine, "PULSE MATCH", "10 matches")) return;
    int16_t pos = 8;
    int8_t dir = baseSpeed;
    int16_t target = random(24, 104);
    uint8_t score = 0;
    uint8_t misses = 0;
    uint32_t nextFrame = 0;
    bool done = false;
    while (score < 10) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
      if (selectTap(engine)) {
        int16_t d = abs(pos - target);
        if (d < tol) {
          score++;
          target = random(24, 104);
          // Accelerate as you progress.
          int8_t mag = baseSpeed + score / 3;
          dir = dir < 0 ? -mag : mag;
          engine.ledPulse(d <= 1 ? CRGB::Green : CRGB::Yellow, 120);
          engine.playSound(d <= 1 ? SOUND_COMBO : SOUND_SCORE);
          sparksSpawn(sparks, gameX(engine, pos), gameY(engine, 32), GAMER_ACCENT, d <= 1 ? 8 : 4);
        } else {
          misses++;
          engine.ledPulse(CRGB::Red, 120);
          engine.playSound(SOUND_ERROR);
          if (misses >= 3) {
            bool rec = bestSubmit("pulsematch", score);
            if (rec) best = score;
            if (!resultScreenBest(engine, "FAILED", score, best, rec)) return;
            done = true;
            break;
          }
        }
      }
      if (!frameDue(nextFrame, 30)) { delay(2); continue; }
      pos += dir;
      if (pos < 8 || pos > 117) dir = -dir;
      engine.clear();
      engine.screen().drawRect(gameX(engine, 8), gameY(engine, 28), gameX(engine, 112), gameSize(engine, 8), GAMER_WHITE);
      engine.screen().fillRect(gameX(engine, target - tol), gameY(engine, 26), gameSize(engine, tol * 2), gameSize(engine, 12), GAMER_ACCENT);
      engine.screen().fillRect(gameX(engine, pos), gameY(engine, 24), gameSize(engine, 3), gameSize(engine, 16), GAMER_INVERSE);
      drawLives(engine, gameX(engine, 2), gameY(engine, 50), 3 - misses);
      sparksUpdateDraw(sparks, engine);
      drawScore(engine, score);
      drawBestTag(engine, best, 0);
      engine.show();
    }
    if (done) continue;
    bool rec = bestSubmit("pulsematch", score);
    if (rec) best = score;
    engine.playSound(SOUND_WIN);
    if (!resultScreenBest(engine, "MATCHED", score, best, rec)) return;
  }
}
