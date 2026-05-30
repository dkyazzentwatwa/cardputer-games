#include "Games.h"
#include "GameUtils.h"

// ===========================================================================
// Puzzle games — deepened. Each game: difficulty tiers, persistent best
// (lowest moves/time via bestSubmitLow, or highest score via bestSubmit),
// undo/feedback where it fits, and juice (sparks/flash/sound) on solves.
// Controls: L/R = move cursor / cycle; SEL tap = act; SEL long-press =
// secondary action (undo / flag / hint, documented per game).
// Fixed arrays only — no heap in loops. Every loop calls engine.tick() and
// honors shouldExitGame() -> waitForRelease() -> return.
// ===========================================================================

namespace {

// Long-press SELECT used as a uniform "secondary action" across games.
bool selectLong(GamerEngine& engine) {
  if (engine.wasLongPressed(BTN_SELECT)) {
    engine.waitForRelease();
    return true;
  }
  return false;
}

void drawCellValue(GamerEngine& engine, int16_t x, int16_t y, uint8_t value, uint8_t size) {
  if (value == 0) engine.screen().drawRect(x, y, size - 1, size - 1, GAMER_WHITE);
  else if (value == 1) engine.screen().fillRect(x + 2, y + 2, size - 4, size - 4, GAMER_WHITE);
  else if (value == 2) {
    engine.screen().drawLine(x + 2, y + 2, x + size - 4, y + size - 4, GAMER_WHITE);
    engine.screen().drawLine(x + size - 4, y + 2, x + 2, y + size - 4, GAMER_WHITE);
  } else {
    engine.screen().fillCircle(x + size / 2, y + size / 2, size / 3, GAMER_WHITE);
  }
}

int8_t cursorStep(GamerEngine& engine, int8_t current, int8_t count) {
  const int8_t previous = current;
  if (engine.wasPressed(BTN_LEFT)) current = current == 0 ? count - 1 : current - 1;
  if (engine.wasPressed(BTN_RIGHT)) current = (current + 1) % count;
  if (current != previous) engine.playSound(SOUND_UI_MOVE);
  return current;
}

// Generic juiced solve: spark burst around centre + win cue.
void solveJuice(GamerEngine& engine, SparkField& sf) {
  engine.ledPulse(CRGB::Green, 260);
  engine.playSound(SOUND_WIN);
  for (uint8_t f = 0; f < 8; f++) {
    if ((f & 3) == 0) sparksSpawn(sf, 64, 32, GAMER_ACCENT, 8);
    engine.clear();
    engine.centerText("SOLVED!", 26, 2, GAMER_ACCENT);
    sparksUpdateDraw(sf, engine);
    engine.show();
    engine.tick();
    delay(45);
  }
}

// ---- 2048 helpers (unchanged math) ----------------------------------------
void add2048Tile(uint8_t board[4][4]) {
  uint8_t empty[16];
  uint8_t emptyCount = 0;
  for (uint8_t y = 0; y < 4; y++)
    for (uint8_t x = 0; x < 4; x++)
      if (board[y][x] == 0) empty[emptyCount++] = y * 4 + x;
  if (emptyCount == 0) return;
  const uint8_t spot = empty[random(0, emptyCount)];
  board[spot / 4][spot % 4] = random(0, 10) == 0 ? 2 : 1;
}

bool slide2048Line(uint8_t line[4], uint16_t& score) {
  uint8_t packed[4] = {0, 0, 0, 0};
  uint8_t count = 0;
  bool changed = false;
  for (uint8_t i = 0; i < 4; i++) if (line[i] != 0) packed[count++] = line[i];
  for (uint8_t i = 0; i < 3; i++) {
    if (packed[i] != 0 && packed[i] == packed[i + 1]) {
      packed[i]++;
      score += static_cast<uint16_t>(1U << packed[i]);
      for (uint8_t j = i + 1; j < 3; j++) packed[j] = packed[j + 1];
      packed[3] = 0;
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (line[i] != packed[i]) changed = true;
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
    changed = slide2048Line(line, score) || changed;
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
  for (uint8_t y = 0; y < 4; y++)
    for (uint8_t x = 0; x < 4; x++) {
      if (board[y][x] == 0) return true;
      if (x < 3 && board[y][x] == board[y][x + 1]) return true;
      if (y < 3 && board[y][x] == board[y + 1][x]) return true;
    }
  return false;
}

bool has2048Tile(uint8_t board[4][4], uint8_t goal) {
  for (uint8_t y = 0; y < 4; y++)
    for (uint8_t x = 0; x < 4; x++)
      if (board[y][x] >= goal) return true;
  return false;
}

void tile2048Label(uint8_t value, char* label, size_t labelSize) {
  if (value == 0) snprintf(label, labelSize, "%s", "");
  else if (value >= 10) snprintf(label, labelSize, "%uK", static_cast<unsigned>(1U << (value - 10)));
  else snprintf(label, labelSize, "%u", static_cast<unsigned>(1U << value));
}

void draw2048Board(GamerEngine& engine, uint8_t board[4][4], uint16_t score,
                   uint16_t moves, uint16_t best, bool canUndo) {
  engine.clear();
  engine.screen().setTextSize(1);
  engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
  engine.screen().setCursor(0, 0);
  engine.screen().print("2048");
  drawScore(engine, static_cast<int16_t>(min<uint16_t>(score, 32767)));
  char mv[12];
  snprintf(mv, sizeof(mv), "m%u%s", moves, canUndo ? "*" : "");
  engine.screen().setCursor(0, 56);
  engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
  engine.screen().print(mv);
  drawBestTag(engine, best, 56);

  const int16_t cellW = 22, cellH = 13, ox = 20, oy = 8;
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      const int16_t px = ox + x * cellW;
      const int16_t py = oy + y * cellH;
      const uint8_t value = board[y][x];
      const uint16_t border = value == 0 ? GAMER_DIM : GAMER_ACCENT;
      if (value != 0)
        engine.screen().fillRect(px + 1, py + 1, cellW - 2, cellH - 2,
                                 value >= 8 ? GAMER_INVERSE : GAMER_ACCENT);
      engine.screen().drawRect(px, py, cellW - 1, cellH - 1, border);
      if (value != 0) {
        char label[5];
        tile2048Label(value, label, sizeof(label));
        const uint8_t len = strlen(label);
        engine.screen().setTextColor(GAMER_BLACK, value >= 8 ? GAMER_INVERSE : GAMER_ACCENT);
        engine.screen().setCursor(px + max<int16_t>(2, (cellW - len * 6) / 2), py + 3);
        engine.screen().print(label);
      }
    }
  }
  engine.show();
}

}  // namespace

// ===========================================================================
// LIGHTS OUT — 3x3 / 4x4 / 5x5 tiers, limited undo, best-moves, par feedback.
// SEL tap = toggle; SEL long = undo (up to 8).
// ===========================================================================
void runLightsOut(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "LIGHTS OUT", "toggle all off");
    if (diff == 255) return;
    const uint8_t n = diff == 0 ? 3 : (diff == 1 ? 4 : 5);
    const char* key = diff == 0 ? "lightsout3" : (diff == 1 ? "lightsout4" : "lightsout5");
    const uint16_t best = bestLoad(key);
    SparkField sf;

    uint8_t cells[25];
    // Generate solvable board by applying random presses from solved state.
    for (uint8_t i = 0; i < 25; i++) cells[i] = 0;
    const uint8_t scramble = n * n;
    uint8_t par = 0;
    for (uint8_t s = 0; s < scramble; s++) {
      uint8_t c = random(0, n * n);
      int8_t cx = c % n, cy = c / n;
      int8_t dx[5] = {0, -1, 1, 0, 0}, dy[5] = {0, 0, 0, -1, 1};
      for (uint8_t i = 0; i < 5; i++) {
        int8_t nx = cx + dx[i], ny = cy + dy[i];
        if (nx >= 0 && nx < n && ny >= 0 && ny < n) cells[ny * n + nx] ^= 1;
      }
      par++;
    }
    // par is an upper bound on the optimal; clamp visually.
    uint8_t cursor = 0;
    uint16_t moves = 0;
    uint8_t undo[8];
    uint8_t undoN = 0;

    bool exit = false;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      cursor = cursorStep(engine, cursor, n * n);
      if (selectLong(engine) && undoN > 0) {
        uint8_t c = undo[--undoN];
        int8_t cx = c % n, cy = c / n;
        int8_t dx[5] = {0, -1, 1, 0, 0}, dy[5] = {0, 0, 0, -1, 1};
        for (uint8_t i = 0; i < 5; i++) {
          int8_t nx = cx + dx[i], ny = cy + dy[i];
          if (nx >= 0 && nx < n && ny >= 0 && ny < n) cells[ny * n + nx] ^= 1;
        }
        if (moves > 0) moves--;
        engine.playSound(SOUND_UI_BACK);
      } else if (selectTap(engine)) {
        int8_t x = cursor % n, y = cursor / n;
        int8_t dx[5] = {0, -1, 1, 0, 0}, dy[5] = {0, 0, 0, -1, 1};
        for (uint8_t i = 0; i < 5; i++) {
          int8_t nx = x + dx[i], ny = y + dy[i];
          if (nx >= 0 && nx < n && ny >= 0 && ny < n) cells[ny * n + nx] ^= 1;
        }
        if (undoN < 8) undo[undoN++] = cursor; else { for (uint8_t k = 1; k < 8; k++) undo[k-1]=undo[k]; undo[7]=cursor; }
        moves++;
      }

      bool won = true;
      for (uint8_t i = 0; i < n * n; i++) if (cells[i]) { won = false; break; }

      engine.clear();
      engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
      engine.screen().setCursor(0, 0); engine.screen().print("LIGHTS");
      drawScore(engine, moves);
      char pp[14]; snprintf(pp, sizeof(pp), "par%u u%u", par, undoN);
      engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
      engine.screen().setCursor(0, 56); engine.screen().print(pp);
      drawBestTag(engine, best, 56);
      const int16_t cell = n == 3 ? 14 : (n == 4 ? 11 : 9);
      const int16_t ox = 64 - (n * cell) / 2;
      const int16_t oy = 32 - (n * cell) / 2;
      for (uint8_t i = 0; i < n * n; i++) {
        int16_t px = ox + (i % n) * cell, py = oy + (i / n) * cell;
        if (cells[i]) engine.screen().fillRect(px + 1, py + 1, cell - 2, cell - 2, GAMER_ACCENT);
        else engine.screen().drawRect(px + 1, py + 1, cell - 2, cell - 2, GAMER_DIM);
        if (i == cursor) drawCursorBox(engine, px, py, cell, cell);
      }
      engine.show();

      if (won) {
        solveJuice(engine, sf);
        bool rec = bestSubmitLow(key, moves);
        if (!resultScreenBest(engine, moves <= par ? "OPTIMAL!" : "CLEARED", moves,
                              rec ? moves : best, rec)) return;
        break;
      }
      delay(20);
    }
    if (exit) return;
  }
}

// ===========================================================================
// MINEFIELD — neighbor counts, flag mode, safe first reveal, flood-open zeros,
// difficulty (mine count). SEL tap = reveal/flag; SEL long = toggle flag mode.
// ===========================================================================
void runMinefield(GamerEngine& engine) {
  const uint8_t W = 5, H = 4, N = W * H;
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "MINEFIELD", "find safe cells");
    if (diff == 255) return;
    const uint8_t mines = diff == 0 ? 3 : (diff == 1 ? 5 : 7);
    const char* key = diff == 0 ? "minefieldE" : (diff == 1 ? "minefieldN" : "minefieldH");
    const uint16_t best = bestLoad(key);
    SparkField sf;

    bool mine[20] = {false}, open[20] = {false}, flag[20] = {false};
    uint8_t around[20] = {0};
    bool placed = false;
    bool flagMode = false;
    uint8_t cursor = 0;
    uint16_t actions = 0;
    const uint8_t need = N - mines;

    bool exit = false, boom = false;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      cursor = cursorStep(engine, cursor, N);
      if (selectLong(engine)) {
        flagMode = !flagMode;
        engine.playSound(SOUND_UI_SELECT);
      } else if (selectTap(engine)) {
        if (flagMode) {
          if (!open[cursor]) { flag[cursor] = !flag[cursor]; engine.playSound(SOUND_UI_MOVE); }
        } else if (!flag[cursor] && !open[cursor]) {
          // Guaranteed-safe first reveal: place mines avoiding first cell.
          if (!placed) {
            uint8_t put = 0;
            while (put < mines) {
              uint8_t p = random(0, N);
              if (p == cursor || mine[p]) continue;
              mine[p] = true; put++;
            }
            for (uint8_t i = 0; i < N; i++) {
              if (mine[i]) continue;
              int8_t cx = i % W, cy = i / W, cnt = 0;
              for (int8_t dy = -1; dy <= 1; dy++) for (int8_t dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int8_t nx = cx + dx, ny = cy + dy;
                if (nx >= 0 && nx < W && ny >= 0 && ny < H && mine[ny * W + nx]) cnt++;
              }
              around[i] = cnt;
            }
            placed = true;
          }
          actions++;
          if (mine[cursor]) { boom = true; engine.ledPulse(CRGB::Red, 280); engine.playSound(SOUND_LOSE);
            for (uint8_t i = 0; i < N; i++) if (mine[i]) open[i] = true; break; }
          // flood-open zeros (iterative stack, fixed array).
          uint8_t stack[20]; uint8_t sp = 0; stack[sp++] = cursor;
          while (sp > 0) {
            uint8_t c = stack[--sp];
            if (open[c] || mine[c] || flag[c]) continue;
            open[c] = true;
            if (around[c] == 0) {
              int8_t cx = c % W, cy = c / W;
              for (int8_t dy = -1; dy <= 1; dy++) for (int8_t dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int8_t nx = cx + dx, ny = cy + dy;
                if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                  uint8_t ni = ny * W + nx;
                  if (!open[ni] && !mine[ni] && sp < 20) stack[sp++] = ni;
                }
              }
            }
          }
          engine.ledPulse(CRGB::Green, 50);
          engine.playSound(SOUND_SCORE);
        }
      }

      uint8_t opened = 0;
      for (uint8_t i = 0; i < N; i++) if (open[i] && !mine[i]) opened++;

      engine.clear();
      engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
      engine.screen().setCursor(0, 0); engine.screen().print("MINES ");
      engine.screen().print(mines);
      engine.screen().setTextColor(flagMode ? GAMER_INVERSE : GAMER_DIM, GAMER_BLACK);
      engine.screen().setCursor(0, 56); engine.screen().print(flagMode ? "FLAG mode" : "DIG mode");
      drawBestTag(engine, best, 56);
      const int16_t cell = 13, ox = 64 - (W * cell) / 2, oy = 10;
      for (uint8_t i = 0; i < N; i++) {
        int16_t px = ox + (i % W) * cell, py = oy + (i / W) * cell;
        if (open[i]) {
          if (mine[i]) engine.screen().fillCircle(px + 6, py + 6, 3, GAMER_INVERSE);
          else {
            engine.screen().drawRect(px, py, cell - 1, cell - 1, GAMER_DIM);
            if (around[i]) { engine.screen().setTextColor(GAMER_WHITE, GAMER_BLACK);
              engine.screen().setCursor(px + 4, py + 3); engine.screen().print(around[i]); }
          }
        } else {
          engine.screen().fillRect(px + 1, py + 1, cell - 2, cell - 2, GAMER_DIM);
          if (flag[i]) {
            engine.screen().fillRect(px + 5, py + 3, 1, 7, GAMER_INVERSE);
            engine.screen().drawTriangle(px + 5, py + 3, px + 5, py + 6, px + 9, py + 4, GAMER_INVERSE);
          }
        }
        if (i == cursor) drawCursorBox(engine, px, py, cell - 1, cell - 1);
      }
      engine.show();

      if (opened >= need) {
        solveJuice(engine, sf);
        bool rec = bestSubmitLow(key, actions);
        if (!resultScreenBest(engine, "SWEPT!", actions, rec ? actions : best, rec)) return;
        break;
      }
      if (boom) break;
      delay(20);
    }
    if (exit) return;
    if (boom) { if (!resultScreen(engine, "BOOM", 0)) return; }
  }
}

// ===========================================================================
// SOKOBAN — 3-level campaign with walls, undo, moves-vs-par, best-moves.
// Grid 8x6. tiles: 0 floor,1 wall. Player/box/target tracked separately.
// L/R = turn, SEL tap = step/push, SEL long = undo.
// ===========================================================================
namespace {
struct SokoState { int8_t px, py, bx, by; };
}
void runSokobanMicro(GamerEngine& engine) {
  // 3 levels: walls bitmap rows (8 wide), player, box, target, par.
  static const char* maps[3] = {
    "########"
    "#..@...#"
    "#..o...#"
    "#......#"
    "#....x.#"
    "########",
    "########"
    "#@.....#"
    "#.####.#"
    "#.o..#.#"
    "#...x#.#"
    "########",
    "########"
    "#.@..#.#"
    "#.o..#.#"
    "##.###.#"
    "#...x..#"
    "########"
  };
  static const uint8_t pars[3] = {6, 9, 11};

  while (true) {
    if (!runIntro(engine, "SOKOBAN", "push box to X")) return;
    const uint16_t best = bestLoad("sokoban");
    SparkField sf;
    uint16_t totalMoves = 0;
    bool quit = false;

    for (uint8_t lvl = 0; lvl < 3 && !quit; lvl++) {
      uint8_t wall[6][8];
      int8_t px = 1, py = 1, bx = 2, by = 2, tx = 5, ty = 4;
      const char* m = maps[lvl];
      for (uint8_t y = 0; y < 6; y++) for (uint8_t x = 0; x < 8; x++) {
        char c = m[y * 8 + x];
        wall[y][x] = (c == '#') ? 1 : 0;
        if (c == '@') { px = x; py = y; }
        else if (c == 'o') { bx = x; by = y; }
        else if (c == 'x') { tx = x; ty = y; }
      }
      uint8_t face = 1;
      uint16_t moves = 0;
      SokoState undo[24]; uint8_t undoN = 0;

      while (true) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); quit = true; break; }
        if (engine.wasPressed(BTN_LEFT)) { face = (face + 3) % 4; engine.playSound(SOUND_UI_MOVE); }
        if (engine.wasPressed(BTN_RIGHT)) { face = (face + 1) % 4; engine.playSound(SOUND_UI_MOVE); }
        if (selectLong(engine) && undoN > 0) {
          SokoState s = undo[--undoN];
          px = s.px; py = s.py; bx = s.bx; by = s.by;
          if (moves > 0) moves--;
          engine.playSound(SOUND_UI_BACK);
        } else if (selectTap(engine)) {
          int8_t dx = face == 1 ? 1 : (face == 3 ? -1 : 0);
          int8_t dy = face == 2 ? 1 : (face == 0 ? -1 : 0);
          int8_t nx = px + dx, ny = py + dy;
          bool moved = false;
          if (nx >= 0 && nx < 8 && ny >= 0 && ny < 6 && !wall[ny][nx]) {
            if (nx == bx && ny == by) {
              int8_t nbx = bx + dx, nby = by + dy;
              if (nbx >= 0 && nbx < 8 && nby >= 0 && nby < 6 && !wall[nby][nbx]) {
                if (undoN < 24) undo[undoN++] = {px, py, bx, by};
                bx = nbx; by = nby; px = nx; py = ny; moves++; moved = true;
              }
            } else {
              if (undoN < 24) undo[undoN++] = {px, py, bx, by};
              px = nx; py = ny; moves++; moved = true;
            }
          }
          if (!moved) engine.playSound(SOUND_ERROR);
        }

        engine.clear();
        engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
        engine.screen().setCursor(0, 0); engine.screen().print("LVL "); engine.screen().print(lvl + 1);
        char info[14]; snprintf(info, sizeof(info), "par%u m%u", pars[lvl], moves);
        engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
        engine.screen().setCursor(0, 56); engine.screen().print(info);
        drawBestTag(engine, best, 56);
        const int16_t cell = 9, ox = 64 - (8 * cell) / 2, oy = 9;
        for (uint8_t y = 0; y < 6; y++) for (uint8_t x = 0; x < 8; x++) {
          int16_t cx = ox + x * cell, cy = oy + y * cell;
          if (wall[y][x]) engine.screen().fillRect(cx, cy, cell, cell, GAMER_DIM);
        }
        engine.screen().drawRect(ox + tx * cell + 1, oy + ty * cell + 1, cell - 2, cell - 2, GAMER_INVERSE);
        bool onTarget = (bx == tx && by == ty);
        engine.screen().fillRect(ox + bx * cell + 1, oy + by * cell + 1, cell - 2, cell - 2,
                                 onTarget ? GAMER_INVERSE : GAMER_ACCENT);
        // player triangle indicates facing
        int16_t pcx = ox + px * cell + cell / 2, pcy = oy + py * cell + cell / 2;
        engine.screen().fillCircle(pcx, pcy, 2, GAMER_WHITE);
        int8_t fdx = face == 1 ? 1 : (face == 3 ? -1 : 0);
        int8_t fdy = face == 2 ? 1 : (face == 0 ? -1 : 0);
        engine.screen().drawLine(pcx, pcy, pcx + fdx * 3, pcy + fdy * 3, GAMER_WHITE);
        engine.show();

        if (onTarget) {
          totalMoves += moves;
          engine.ledPulse(CRGB::Green, 160);
          engine.playSound(SOUND_LEVELUP);
          sparksSpawn(sf, ox + tx * cell, oy + ty * cell, GAMER_ACCENT, 8);
          for (uint8_t f = 0; f < 6; f++) { engine.clear();
            engine.centerText(lvl < 2 ? "BOX SET!" : "CAMPAIGN!", 26, 2, GAMER_ACCENT);
            sparksUpdateDraw(sf, engine); engine.show(); engine.tick(); delay(40); }
          break;
        }
        delay(20);
      }
    }
    if (quit) return;
    bool rec = bestSubmitLow("sokoban", totalMoves);
    if (!resultScreenBest(engine, "CLEAR!", totalMoves, rec ? totalMoves : best, rec)) return;
  }
}

// ===========================================================================
// BOX PUSH — distinct from Sokoban: 2 boxes / 2 targets, no walls but you must
// route both boxes; pushing into another box or the wall is blocked. Tighter
// open arena makes ordering the real puzzle. SEL long = undo, R1/R3 = step.
// ===========================================================================
namespace {
struct BoxState { int8_t px, py, b0x, b0y, b1x, b1y; };
}
void runBoxPush(GamerEngine& engine) {
  const uint8_t W = 7, H = 5;
  // 3 layouts: player, two boxes, two targets.
  static const int8_t lay[3][10] = {
    //  px py  b0x b0y  b1x b1y  t0x t0y  t1x t1y
    {  3, 4,   2, 2,   4, 2,    1, 0,   5, 0 },
    {  0, 2,   2, 1,   2, 3,    5, 1,   5, 3 },
    {  3, 0,   1, 2,   5, 2,    3, 4,   3, 2 }
  };
  static const uint8_t pars[3] = {10, 12, 14};

  while (true) {
    if (!runIntro(engine, "BOX PUSH", "set BOTH boxes")) return;
    const uint16_t best = bestLoad("boxpush");
    SparkField sf;
    uint16_t totalMoves = 0;
    bool quit = false;

    for (uint8_t lvl = 0; lvl < 3 && !quit; lvl++) {
      int8_t px = lay[lvl][0], py = lay[lvl][1];
      int8_t b0x = lay[lvl][2], b0y = lay[lvl][3], b1x = lay[lvl][4], b1y = lay[lvl][5];
      const int8_t t0x = lay[lvl][6], t0y = lay[lvl][7], t1x = lay[lvl][8], t1y = lay[lvl][9];
      uint8_t face = 1;
      uint16_t moves = 0;
      BoxState undo[28]; uint8_t undoN = 0;

      while (true) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); quit = true; break; }
        if (engine.wasPressed(BTN_LEFT)) { face = (face + 3) % 4; engine.playSound(SOUND_UI_MOVE); }
        if (engine.wasPressed(BTN_RIGHT)) { face = (face + 1) % 4; engine.playSound(SOUND_UI_MOVE); }
        if (selectLong(engine) && undoN > 0) {
          BoxState s = undo[--undoN];
          px = s.px; py = s.py; b0x = s.b0x; b0y = s.b0y; b1x = s.b1x; b1y = s.b1y;
          if (moves > 0) moves--;
          engine.playSound(SOUND_UI_BACK);
        } else if (selectTap(engine)) {
          int8_t dx = face == 1 ? 1 : (face == 3 ? -1 : 0);
          int8_t dy = face == 2 ? 1 : (face == 0 ? -1 : 0);
          int8_t nx = px + dx, ny = py + dy;
          bool moved = false;
          if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
            int8_t* hit = nullptr;
            if (nx == b0x && ny == b0y) hit = &b0x;
            else if (nx == b1x && ny == b1y) hit = &b1x;
            if (hit) {
              int8_t nbx = nx + dx, nby = ny + dy;
              bool freeCell = nbx >= 0 && nbx < W && nby >= 0 && nby < H &&
                              !(nbx == b0x && nby == b0y) && !(nbx == b1x && nby == b1y);
              if (freeCell) {
                if (undoN < 28) undo[undoN++] = {px, py, b0x, b0y, b1x, b1y};
                if (hit == &b0x) { b0x = nbx; b0y = nby; } else { b1x = nbx; b1y = nby; }
                px = nx; py = ny; moves++; moved = true;
              }
            } else {
              if (undoN < 28) undo[undoN++] = {px, py, b0x, b0y, b1x, b1y};
              px = nx; py = ny; moves++; moved = true;
            }
          }
          if (!moved) engine.playSound(SOUND_ERROR);
        }

        bool s0 = (b0x == t0x && b0y == t0y) || (b0x == t1x && b0y == t1y);
        bool s1 = (b1x == t0x && b1y == t0y) || (b1x == t1x && b1y == t1y);
        // ensure both targets covered (not same target twice)
        bool done = ((b0x==t0x&&b0y==t0y)&&(b1x==t1x&&b1y==t1y)) ||
                    ((b0x==t1x&&b0y==t1y)&&(b1x==t0x&&b1y==t0y));

        engine.clear();
        engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
        engine.screen().setCursor(0, 0); engine.screen().print("PUSH "); engine.screen().print(lvl + 1);
        char info[14]; snprintf(info, sizeof(info), "par%u m%u", pars[lvl], moves);
        engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
        engine.screen().setCursor(0, 56); engine.screen().print(info);
        drawBestTag(engine, best, 56);
        const int16_t cell = 10, ox = 64 - (W * cell) / 2, oy = 9;
        engine.screen().drawRect(ox - 1, oy - 1, W * cell + 2, H * cell + 2, GAMER_DIM);
        engine.screen().drawRect(ox + t0x * cell + 1, oy + t0y * cell + 1, cell - 2, cell - 2, GAMER_INVERSE);
        engine.screen().drawRect(ox + t1x * cell + 1, oy + t1y * cell + 1, cell - 2, cell - 2, GAMER_INVERSE);
        engine.screen().fillRect(ox + b0x * cell + 2, oy + b0y * cell + 2, cell - 4, cell - 4, s0 ? GAMER_INVERSE : GAMER_ACCENT);
        engine.screen().fillRect(ox + b1x * cell + 2, oy + b1y * cell + 2, cell - 4, cell - 4, s1 ? GAMER_INVERSE : GAMER_ACCENT);
        int16_t pcx = ox + px * cell + cell / 2, pcy = oy + py * cell + cell / 2;
        engine.screen().fillCircle(pcx, pcy, 2, GAMER_WHITE);
        int8_t fdx = face == 1 ? 1 : (face == 3 ? -1 : 0);
        int8_t fdy = face == 2 ? 1 : (face == 0 ? -1 : 0);
        engine.screen().drawLine(pcx, pcy, pcx + fdx * 3, pcy + fdy * 3, GAMER_WHITE);
        engine.show();

        if (done) {
          totalMoves += moves;
          engine.ledPulse(CRGB::Green, 160);
          engine.playSound(SOUND_LEVELUP);
          sparksSpawn(sf, pcx, pcy, GAMER_ACCENT, 10);
          for (uint8_t f = 0; f < 6; f++) { engine.clear();
            engine.centerText(lvl < 2 ? "BOTH SET!" : "ALL CLEAR!", 26, 2, GAMER_ACCENT);
            sparksUpdateDraw(sf, engine); engine.show(); engine.tick(); delay(40); }
          break;
        }
        delay(20);
      }
    }
    if (quit) return;
    bool rec = bestSubmitLow("boxpush", totalMoves);
    if (!resultScreenBest(engine, "PUSHED!", totalMoves, rec ? totalMoves : best, rec)) return;
  }
}

// ===========================================================================
// SLIDING — solvable-only shuffle (inversion parity), shuffle-depth tiers,
// best-moves. L/R cursor, SEL tap = slide adjacent tile into blank.
// ===========================================================================
namespace {
bool slidingSolvable(const uint8_t t[9]) {
  // 3x3 with blank=0: solvable iff inversions are even.
  uint8_t inv = 0;
  for (uint8_t i = 0; i < 9; i++) for (uint8_t j = i + 1; j < 9; j++)
    if (t[i] && t[j] && t[i] > t[j]) inv++;
  return (inv % 2) == 0;
}
}
void runSlidingPuzzle(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "SLIDING", "order 1-8");
    if (diff == 255) return;
    const uint16_t depth = diff == 0 ? 30 : (diff == 1 ? 80 : 200);
    const char* key = diff == 0 ? "slidingE" : (diff == 1 ? "slidingN" : "slidingH");
    const uint16_t best = bestLoad(key);
    SparkField sf;

    uint8_t tiles[9] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
    // Shuffle by valid blank moves -> always solvable, depth controls hardness.
    uint8_t blank = 8;
    for (uint16_t s = 0; s < depth; s++) {
      int8_t bx = blank % 3, by = blank / 3;
      int8_t dirs[4] = {0, 1, 2, 3};
      uint8_t nb = blank;
      for (uint8_t tryi = 0; tryi < 4; tryi++) {
        uint8_t d = random(0, 4);
        int8_t nx = bx + (d == 2 ? -1 : d == 3 ? 1 : 0);
        int8_t ny = by + (d == 0 ? -1 : d == 1 ? 1 : 0);
        if (nx >= 0 && nx < 3 && ny >= 0 && ny < 3) { nb = ny * 3 + nx; break; }
      }
      if (nb != blank) { tiles[blank] = tiles[nb]; tiles[nb] = 0; blank = nb; }
    }
    if (!slidingSolvable(tiles)) { /* safety: swap two non-blank tiles */
      uint8_t a = tiles[0] ? 0 : 2, b = tiles[1] ? 1 : 3;
      uint8_t tmp = tiles[a]; tiles[a] = tiles[b]; tiles[b] = tmp;
    }

    uint8_t cursor = 0;
    uint16_t moves = 0;
    bool exit = false;
    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      cursor = cursorStep(engine, cursor, 9);
      if (selectTap(engine)) {
        for (uint8_t i = 0; i < 9; i++) if (tiles[i] == 0) blank = i;
        int8_t dx = abs((int8_t)(cursor % 3) - (int8_t)(blank % 3));
        int8_t dy = abs((int8_t)(cursor / 3) - (int8_t)(blank / 3));
        if (dx + dy == 1) { tiles[blank] = tiles[cursor]; tiles[cursor] = 0; moves++; }
        else engine.playSound(SOUND_ERROR);
      }
      bool won = true;
      for (uint8_t i = 0; i < 8; i++) if (tiles[i] != i + 1) { won = false; break; }

      engine.clear();
      engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
      engine.screen().setCursor(0, 0); engine.screen().print("SLIDE");
      drawScore(engine, moves);
      drawBestTag(engine, best, 56);
      const int16_t cell = 16, ox = 40, oy = 8;
      drawMiniGrid(engine, 3, 3, 16, 40, 8);
      for (uint8_t i = 0; i < 9; i++) {
        int16_t x = ox + (i % 3) * cell, y = oy + (i / 3) * cell;
        if (tiles[i] != 0) {
          engine.screen().fillRect(x + 2, y + 2, cell - 4, cell - 4, GAMER_DIM);
          engine.screen().setTextColor(GAMER_WHITE, GAMER_DIM);
          engine.screen().setCursor(x + 5, y + 4); engine.screen().print(tiles[i]);
        }
        if (i == cursor) drawCursorBox(engine, x + 1, y + 1, cell - 2, cell - 2);
      }
      engine.show();

      if (won) {
        solveJuice(engine, sf);
        bool rec = bestSubmitLow(key, moves);
        if (!resultScreenBest(engine, "SOLVED!", moves, rec ? moves : best, rec)) return;
        break;
      }
      delay(20);
    }
    if (exit) return;
  }
}

// ===========================================================================
// MEMORY — 4/6/8 pair tiers, combo bonus for consecutive matches, best=moves.
// L/R cursor, SEL tap = flip.
// ===========================================================================
void runMemoryMatch(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "MEMORY", "match pairs");
    if (diff == 255) return;
    const uint8_t pairsN = diff == 0 ? 4 : (diff == 1 ? 6 : 8);
    const uint8_t cardsN = pairsN * 2;        // up to 16
    const uint8_t cols = pairsN == 4 ? 4 : (pairsN == 6 ? 4 : 4);
    const char* key = diff == 0 ? "memory4" : (diff == 1 ? "memory6" : "memory8");
    const uint16_t best = bestLoad(key);
    SparkField sf;

    uint8_t cards[16];
    for (uint8_t i = 0; i < pairsN; i++) { cards[i * 2] = i + 1; cards[i * 2 + 1] = i + 1; }
    for (uint8_t i = 0; i < 60; i++) {
      uint8_t a = random(0, cardsN), b = random(0, cardsN);
      uint8_t t = cards[a]; cards[a] = cards[b]; cards[b] = t;
    }
    bool matched[16] = {false}, shown[16] = {false};
    int8_t first = -1;
    uint8_t cursor = 0, pairs = 0, combo = 0;
    uint16_t moves = 0, bonus = 0;
    bool exit = false;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      cursor = cursorStep(engine, cursor, cardsN);
      if (selectTap(engine) && !matched[cursor] && !(first == cursor)) {
        shown[cursor] = true;
        if (first < 0) first = cursor;
        else {
          moves++;
          if (cards[first] == cards[cursor]) {
            matched[first] = matched[cursor] = true; pairs++; combo++;
            bonus += combo;  // combo bonus (display only)
            engine.ledPulse(CRGB::Green, 80);
            engine.playSound(combo >= 2 ? SOUND_COMBO : SOUND_SCORE);
            int16_t cx = 24 + (cursor % cols) * 20 + 8, cy = 12 + (cursor / cols) * 13 + 6;
            sparksSpawn(sf, cx, cy, GAMER_ACCENT, combo >= 2 ? 8 : 4);
          } else {
            combo = 0;
            engine.playSound(SOUND_ERROR);
            engine.show(); delay(320);
            shown[first] = shown[cursor] = false;
          }
          first = -1;
        }
      }

      engine.clear();
      engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
      engine.screen().setCursor(0, 0); engine.screen().print("MEM ");
      char hud[16]; snprintf(hud, sizeof(hud), "x%u +%u", combo, bonus);
      engine.screen().setTextColor(combo >= 2 ? GAMER_INVERSE : GAMER_DIM, GAMER_BLACK);
      engine.screen().setCursor(0, 56); engine.screen().print(hud);
      drawScore(engine, moves);
      drawBestTag(engine, best, 56);
      const int16_t cw = 18, ch = 11, ox = 64 - (cols * 20) / 2;
      const uint8_t rows = (cardsN + cols - 1) / cols;
      for (uint8_t i = 0; i < cardsN; i++) {
        int16_t x = ox + (i % cols) * 20, y = 10 + (i / cols) * 13;
        engine.screen().drawRect(x, y, cw, ch, matched[i] ? GAMER_DIM : GAMER_WHITE);
        if (shown[i] || matched[i]) {
          engine.screen().setTextColor(matched[i] ? GAMER_DIM : GAMER_ACCENT, GAMER_BLACK);
          engine.screen().setCursor(x + 6, y + 2); engine.screen().print(cards[i]);
        }
        if (i == cursor) drawCursorBox(engine, x, y, cw, ch);
      }
      sparksUpdateDraw(sf, engine);
      engine.show();

      if (pairs == pairsN) {
        solveJuice(engine, sf);
        bool rec = bestSubmitLow(key, moves);
        if (!resultScreenBest(engine, "MATCHED!", moves, rec ? moves : best, rec)) return;
        break;
      }
      delay(20);
    }
    if (exit) return;
  }
}

// ===========================================================================
// SIMON — accelerating tempo per level, 3 lives, cap 24, best level (high).
// ===========================================================================
void runSimon(GamerEngine& engine) {
  while (true) {
    if (!runIntro(engine, "SIMON", "L/R/SEL repeat")) return;
    const uint16_t best = bestLoad("simon");
    uint8_t seq[24];
    for (uint8_t i = 0; i < 24; i++) seq[i] = random(0, 3);
    uint8_t level = 1, lives = 3;

    while (lives > 0 && level <= 24) {
      // playback tempo accelerates with level.
      uint16_t lit = level < 8 ? 380 : (level < 16 ? 280 : 200);
      uint16_t gap = level < 8 ? 120 : (level < 16 ? 90 : 60);
      bool aborted = false;
      for (uint8_t i = 0; i < level; i++) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
        engine.clear();
        const char* label = seq[i] == 0 ? "LEFT" : (seq[i] == 1 ? "RIGHT" : "SELECT");
        engine.centerText("SIMON", 6, 1, GAMER_DIM);
        engine.centerText(label, 26, 2, GAMER_ACCENT);
        char lv[12]; snprintf(lv, sizeof(lv), "Lv%u", level);
        engine.centerText(lv, 50, 1, GAMER_DIM);
        drawLives(engine, 2, 1, lives);
        engine.show();
        engine.ledPulse(seq[i] == 0 ? CRGB::Blue : (seq[i] == 1 ? CRGB::Green : CRGB::Purple), lit);
        engine.playSound(seq[i] == 0 ? SOUND_SIMON_LEFT : (seq[i] == 1 ? SOUND_SIMON_RIGHT : SOUND_SIMON_SELECT));
        delay(lit);
        engine.clear(); drawLives(engine, 2, 1, lives); engine.show();
        delay(gap);
      }
      bool failed = false;
      for (uint8_t i = 0; i < level && !failed; i++) {
        uint8_t input = 9;
        while (input == 9) {
          engine.tick();
          if (engine.shouldExitGame()) { engine.waitForRelease(); return; }
          if (engine.wasPressed(BTN_LEFT)) { input = 0; engine.playSound(SOUND_SIMON_LEFT); }
          if (engine.wasPressed(BTN_RIGHT)) { input = 1; engine.playSound(SOUND_SIMON_RIGHT); }
          if (selectTap(engine)) { input = 2; engine.playSound(SOUND_SIMON_SELECT); }
          delay(8);
        }
        if (input != seq[i]) failed = true;
      }
      if (failed) {
        lives--;
        engine.ledPulse(CRGB::Red, 220);
        engine.playSound(SOUND_LOSE);
        engine.clear();
        engine.centerText(lives ? "MISS!" : "GAME OVER", 24, 2, GAMER_INVERSE);
        drawLives(engine, 2, 1, lives);
        engine.show(); delay(700);
      } else {
        level++;
        engine.ledPulse(CRGB::Green, 180);
        engine.playSound(SOUND_LEVELUP);
      }
      (void)aborted;
    }
    uint16_t reached = level - 1;
    bool rec = bestSubmit("simon", reached);
    engine.playSound(rec ? SOUND_WIN : SOUND_LOSE);
    if (!resultScreenBest(engine, "REACHED", reached, rec ? reached : best, rec)) return;
  }
}

// ===========================================================================
// MASTERMIND — exact + misplaced feedback, 3/4-digit tiers, one hint, best=
// fewest attempts. L/R change digit, SEL tap = next/submit, SEL long = hint.
// ===========================================================================
void runMastermind(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "MASTERMIND", "crack the code");
    if (diff == 255) return;
    const uint8_t len = diff == 0 ? 3 : (diff == 1 ? 3 : 4);
    const uint8_t range = diff == 0 ? 4 : (diff == 1 ? 6 : 6);
    const uint8_t maxTry = diff == 2 ? 12 : 10;
    const char* key = diff == 0 ? "mmindE" : (diff == 1 ? "mmindN" : "mmindH");
    const uint16_t best = bestLoad(key);
    SparkField sf;

    uint8_t secret[4], guess[4] = {0, 0, 0, 0};
    for (uint8_t i = 0; i < len; i++) secret[i] = random(0, range);
    uint8_t pos = 0, attempts = 0, lastExact = 0, lastColor = 0;
    bool hintUsed = false;
    bool exit = false, won = false;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      if (engine.wasPressed(BTN_LEFT)) { guess[pos] = guess[pos] == 0 ? range - 1 : guess[pos] - 1; engine.playSound(SOUND_UI_MOVE); }
      if (engine.wasPressed(BTN_RIGHT)) { guess[pos] = (guess[pos] + 1) % range; engine.playSound(SOUND_UI_MOVE); }
      if (selectLong(engine) && !hintUsed) {
        guess[pos] = secret[pos]; hintUsed = true;
        engine.playSound(SOUND_POWERUP);
      } else if (selectTap(engine)) {
        pos++;
        if (pos >= len) {
          pos = 0; attempts++;
          lastExact = 0; lastColor = 0;
          uint8_t sUsed[4] = {0}, gUsed[4] = {0};
          for (uint8_t i = 0; i < len; i++) if (guess[i] == secret[i]) { lastExact++; sUsed[i] = gUsed[i] = 1; }
          for (uint8_t i = 0; i < len; i++) {
            if (gUsed[i]) continue;
            for (uint8_t j = 0; j < len; j++) {
              if (!sUsed[j] && guess[i] == secret[j]) { lastColor++; sUsed[j] = 1; break; }
            }
          }
          if (lastExact == len) { won = true; break; }
          if (attempts >= maxTry) { break; }
        }
      }

      engine.clear();
      engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
      engine.screen().setCursor(0, 0); engine.screen().print("CODE 0-"); engine.screen().print(range - 1);
      for (uint8_t i = 0; i < len; i++) {
        int16_t x = 64 - (len * 14) / 2 + i * 14;
        engine.screen().setTextColor(GAMER_WHITE, GAMER_BLACK);
        engine.screen().setCursor(x + 3, 24); engine.screen().print(guess[i]);
        if (i == pos) drawCursorBox(engine, x, 20, 12, 14);
      }
      char fb[20]; snprintf(fb, sizeof(fb), "exact %u  col %u", lastExact, lastColor);
      engine.centerText(fb, 42, 1, GAMER_INVERSE);
      char at[14]; snprintf(at, sizeof(at), "try %u/%u%s", attempts, maxTry, hintUsed ? "" : " H");
      engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
      engine.screen().setCursor(0, 56); engine.screen().print(at);
      drawBestTag(engine, best, 56);
      engine.show();
      delay(20);
    }
    if (exit) return;
    if (won) {
      solveJuice(engine, sf);
      bool rec = bestSubmitLow(key, attempts);
      if (!resultScreenBest(engine, "CRACKED!", attempts, rec ? attempts : best, rec)) return;
    } else {
      engine.ledPulse(CRGB::Red, 220); engine.playSound(SOUND_LOSE);
      if (!resultScreen(engine, "LOCKED", lastExact)) return;
    }
  }
}

// ===========================================================================
// NUMBER GUESS — range tiers, optimal-tries indicator, best = fewest tries.
// L/R set value (hold to repeat via wasPressed bursts), SEL tap = submit.
// ===========================================================================
void runNumberGuess(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "NUMBER", "binary search");
    if (diff == 255) return;
    const uint8_t maxV = diff == 0 ? 15 : (diff == 1 ? 31 : 127);
    const char* key = diff == 0 ? "numguessE" : (diff == 1 ? "numguessN" : "numguessH");
    const uint16_t best = bestLoad(key);
    SparkField sf;
    // optimal worst-case tries = ceil(log2(maxV+1))
    uint8_t opt = 0; for (uint16_t v = maxV + 1; v > 1; v = (v + 1) / 2) opt++;

    uint8_t target = random(0, maxV + 1);
    uint8_t guess = (maxV + 1) / 2, tries = 0;
    int8_t hint = 0;
    uint8_t lo = 0, hi = maxV;  // tracked range for the player
    bool exit = false;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      bool step = false;
      if (engine.wasPressed(BTN_LEFT) && guess > 0) { guess--; step = true; }
      if (engine.wasPressed(BTN_RIGHT) && guess < maxV) { guess++; step = true; }
      // accelerate when held
      if (engine.isHeld(BTN_LEFT) && guess > 0) { static uint32_t t=0; if(millis()-t>70){guess--;step=true;t=millis();} }
      if (engine.isHeld(BTN_RIGHT) && guess < maxV) { static uint32_t t=0; if(millis()-t>70){guess++;step=true;t=millis();} }
      if (step) engine.playSound(SOUND_UI_MOVE);
      if (selectTap(engine)) {
        tries++;
        if (guess == target) {
          solveJuice(engine, sf);
          bool rec = bestSubmitLow(key, tries);
          if (!resultScreenBest(engine, tries <= opt ? "SHARP!" : "FOUND!", tries, rec ? tries : best, rec)) return;
          break;
        }
        if (guess < target) { hint = 1; if (guess + 1 > lo) lo = guess + 1; }
        else { hint = -1; if (guess >= 1 && guess - 1 < hi) hi = guess - 1; }
        engine.playSound(SOUND_ERROR);
      }

      engine.clear();
      char hdr[16]; snprintf(hdr, sizeof(hdr), "0-%u", maxV);
      engine.centerText(hdr, 4, 1, GAMER_DIM);
      char text[8]; snprintf(text, sizeof(text), "%u", guess);
      engine.centerText(text, 22, 2, GAMER_ACCENT);
      engine.centerText(hint == 0 ? "SEL guess" : (hint > 0 ? "GO HIGHER" : "GO LOWER"), 44, 1, GAMER_INVERSE);
      char info[18]; snprintf(info, sizeof(info), "try %u  opt %u", tries, opt);
      engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
      engine.screen().setCursor(0, 56); engine.screen().print(info);
      drawBestTag(engine, best, 56);
      // range bar
      engine.screen().drawRect(14, 36, 100, 4, GAMER_DIM);
      int16_t a = 14 + (int32_t)lo * 100 / maxV, b = 14 + (int32_t)hi * 100 / maxV;
      engine.screen().fillRect(a, 37, max<int16_t>(1, b - a), 2, GAMER_ACCENT);
      engine.show();
      delay(20);
    }
    if (exit) return;
  }
}

// ===========================================================================
// 2048 — move counter, limited 1-step undo, 2048/4096 hard mode, best score.
// Arrows = move, SEL long = undo last move.
// ===========================================================================
void runTwentyFortyEight(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "2048", "merge to goal");
    if (diff == 255) return;
    const uint8_t goal = diff == 2 ? 12 : 11;  // 4096 vs 2048 (exponent)
    const char* key = diff == 2 ? "g2048hard" : "g2048";
    const uint16_t best = bestLoad(key);

    uint8_t board[4][4] = {};
    uint8_t prev[4][4] = {};
    uint16_t prevScore = 0;
    bool canUndo = false;
    uint16_t score = 0, moves = 0;
    bool won = false, exit = false;
    add2048Tile(board); add2048Tile(board);

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }

      if (engine.wasLongPressed(BTN_SELECT)) {
        engine.waitForRelease();
        if (canUndo) {
          for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++) board[y][x] = prev[y][x];
          score = prevScore; canUndo = false;
          if (moves > 0) moves--;
          engine.playSound(SOUND_UI_BACK);
        }
      } else {
        GamerDirection dir = GAMER_DIR_UP;
        bool hasMove = true;
        if (engine.wasDirectionPressed(GAMER_DIR_UP)) dir = GAMER_DIR_UP;
        else if (engine.wasDirectionPressed(GAMER_DIR_DOWN)) dir = GAMER_DIR_DOWN;
        else if (engine.wasDirectionPressed(GAMER_DIR_LEFT)) dir = GAMER_DIR_LEFT;
        else if (engine.wasDirectionPressed(GAMER_DIR_RIGHT)) dir = GAMER_DIR_RIGHT;
        else hasMove = false;
        if (hasMove) {
          uint8_t snap[4][4]; uint16_t snapScore = score;
          for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++) snap[y][x] = board[y][x];
          if (move2048(board, dir, score)) {
            for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++) prev[y][x] = snap[y][x];
            prevScore = snapScore; canUndo = true;
            add2048Tile(board); moves++;
            engine.ledPulse(CRGB::Aqua, 50); engine.playSound(SOUND_UI_MOVE);
          } else engine.playSound(SOUND_ERROR);
        }
      }

      draw2048Board(engine, board, score, moves, best, canUndo);

      if (!won && has2048Tile(board, goal)) {
        won = true;
        engine.ledPulse(CRGB::Green, 260); engine.playSound(SOUND_WIN);
        bool rec = bestSubmit(key, min<uint16_t>(score, 65535));
        if (!resultScreenBest(engine, goal == 12 ? "4096!" : "2048!",
                              min<uint16_t>(score, 32767), rec ? score : best, rec)) return;
        break;
      }
      if (!canMove2048(board)) {
        engine.ledPulse(CRGB::Red, 220); engine.playSound(SOUND_LOSE);
        bool rec = bestSubmit(key, min<uint16_t>(score, 65535));
        if (!resultScreenBest(engine, "NO MOVES", min<uint16_t>(score, 32767), rec ? score : best, rec)) return;
        break;
      }
      delay(20);
    }
    if (exit) return;
  }
}

// ===========================================================================
// FLOOD FILL — move-limit vs par, size+color tiers, best moves.
// L/R cycle color, SEL tap = flood from top-left.
// ===========================================================================
namespace {
void floodN(uint8_t* b, uint8_t n, int8_t x, int8_t y, uint8_t from, uint8_t to) {
  if (x < 0 || x >= n || y < 0 || y >= n || b[y * n + x] != from || from == to) return;
  b[y * n + x] = to;
  floodN(b, n, x - 1, y, from, to); floodN(b, n, x + 1, y, from, to);
  floodN(b, n, x, y - 1, from, to); floodN(b, n, x, y + 1, from, to);
}
}
void runFloodFill(GamerEngine& engine) {
  while (true) {
    const uint8_t diff = chooseDifficulty(engine, "FLOOD", "fill one color");
    if (diff == 255) return;
    const uint8_t n = diff == 0 ? 5 : (diff == 1 ? 6 : 7);
    const uint8_t colors = diff == 0 ? 3 : (diff == 1 ? 4 : 4);
    const char* key = diff == 0 ? "floodfillE" : (diff == 1 ? "floodfillN" : "floodfillH");
    const uint16_t best = bestLoad(key);
    SparkField sf;

    uint8_t board[49];
    for (uint8_t i = 0; i < n * n; i++) board[i] = random(0, colors);
    // par/limit heuristic scales with size & colors.
    const uint8_t limit = (uint8_t)(n + colors + (diff == 2 ? 6 : 4));
    uint8_t moves = 0;
    uint8_t pick = 0;
    bool exit = false, won = false, lost = false;

    while (true) {
      engine.tick();
      if (engine.shouldExitGame()) { engine.waitForRelease(); exit = true; break; }
      if (engine.wasPressed(BTN_LEFT)) { pick = pick == 0 ? colors - 1 : pick - 1; engine.playSound(SOUND_UI_MOVE); }
      if (engine.wasPressed(BTN_RIGHT)) { pick = (pick + 1) % colors; engine.playSound(SOUND_UI_MOVE); }
      if (selectTap(engine)) {
        uint8_t from = board[0];
        if (from != pick) { floodN(board, n, 0, 0, from, pick); moves++; engine.playSound(SOUND_ACTION); }
        else engine.playSound(SOUND_ERROR);
      }
      bool same = true;
      for (uint8_t i = 1; i < n * n; i++) if (board[i] != board[0]) { same = false; break; }
      if (same) won = true;
      else if (moves >= limit) lost = true;

      engine.clear();
      engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
      engine.screen().setCursor(0, 0); engine.screen().print("FLOOD");
      char mh[14]; snprintf(mh, sizeof(mh), "%u/%u", moves, limit);
      engine.rightText(mh);
      drawBestTag(engine, best, 56);
      const int16_t cell = n <= 5 ? 10 : (n == 6 ? 8 : 7);
      const int16_t ox = 64 - (n * cell) / 2, oy = 10;
      static const uint16_t pal[4] = {GAMER_WHITE, GAMER_ACCENT, GAMER_INVERSE, GAMER_DIM};
      for (uint8_t y = 0; y < n; y++) for (uint8_t x = 0; x < n; x++)
        engine.screen().fillRect(ox + x * cell, oy + y * cell, cell - 1, cell - 1, pal[board[y * n + x] % 4]);
      // color picker
      for (uint8_t c = 0; c < colors; c++) {
        int16_t bx = 4 + c * 12;
        engine.screen().fillRect(bx, 54, 9, 8, pal[c]);
        if (c == pick) engine.screen().drawRect(bx - 1, 53, 11, 10, GAMER_WHITE);
      }
      engine.show();

      if (won) {
        solveJuice(engine, sf);
        bool rec = bestSubmitLow(key, moves);
        if (!resultScreenBest(engine, moves <= limit - 2 ? "PERFECT!" : "FILLED!", moves, rec ? moves : best, rec)) return;
        break;
      }
      if (lost) break;
      delay(20);
    }
    if (exit) return;
    if (lost) { engine.ledPulse(CRGB::Red, 220); engine.playSound(SOUND_LOSE);
      if (!resultScreen(engine, "OUT OF MOVES", moves)) return; }
  }
}

// ===========================================================================
// LASER MIRROR — multi-level layouts, draws the routed laser path, moves-vs-par,
// best. L/R cursor, SEL tap = rotate mirror at cursor.
// Grid 5 wide x 3 tall. mirror: 0 '/'  1 '\'  2 empty. Laser enters left mid.
// ===========================================================================
void runLaserMirror(GamerEngine& engine) {
  const uint8_t GW = 5, GH = 3, GN = GW * GH;
  // 3 layouts: initial mirror states + goal exit edge cell (row), par.
  // values: 0='/',1='\',2=empty
  static const uint8_t init[3][15] = {
    { 2,2,2,2,2,  2,2,2,2,2,  2,2,2,2,2 },
    { 1,2,2,2,0,  2,2,1,2,2,  0,2,2,2,1 },
    { 2,1,2,0,2,  1,2,2,2,1,  2,0,2,1,2 }
  };
  static const uint8_t goalRow[3] = {1, 0, 2};   // which right-edge row must be hit
  static const uint8_t pars[3] = {2, 4, 6};

  while (true) {
    if (!runIntro(engine, "LASER", "route to X")) return;
    const uint16_t best = bestLoad("laser");
    SparkField sf;
    uint16_t totalMoves = 0;
    bool quit = false;

    for (uint8_t lvl = 0; lvl < 3 && !quit; lvl++) {
      uint8_t mir[15];
      for (uint8_t i = 0; i < GN; i++) mir[i] = init[lvl][i];
      uint8_t cursor = 0, moves = 0;
      const uint8_t gr = goalRow[lvl];

      while (true) {
        engine.tick();
        if (engine.shouldExitGame()) { engine.waitForRelease(); quit = true; break; }
        cursor = cursorStep(engine, cursor, GN);
        if (selectTap(engine)) {
          mir[cursor] = (mir[cursor] + 1) % 3;  // / -> \ -> empty -> /
          moves++;
        }

        // trace laser: enter at left of row gr going right.
        // store path cells visited for drawing.
        int8_t px = -1, py = gr, dx = 1, dy = 0;
        uint8_t pathx[40], pathy[40]; uint8_t pn = 0;
        bool hit = false;
        for (uint8_t step = 0; step < 40; step++) {
          int8_t nx = px + dx, ny = py + dy;
          if (pn < 40) { pathx[pn] = nx; pathy[pn] = ny; pn++; }
          if (nx < 0 || ny < 0 || ny >= GH) break;       // off top/bottom/left
          if (nx >= GW) { hit = (ny == gr); break; }      // exited right edge
          uint8_t m = mir[ny * GW + nx];
          if (m == 0) { int8_t t = dx; dx = -dy; dy = -t; }       // '/'
          else if (m == 1) { int8_t t = dx; dx = dy; dy = t; }    // '\'
          // m==2 pass straight
          px = nx; py = ny;
        }

        engine.clear();
        engine.screen().setTextColor(GAMER_ACCENT, GAMER_BLACK);
        engine.screen().setCursor(0, 0); engine.screen().print("LASER "); engine.screen().print(lvl + 1);
        char info[14]; snprintf(info, sizeof(info), "par%u m%u", pars[lvl], moves);
        engine.screen().setTextColor(GAMER_DIM, GAMER_BLACK);
        engine.screen().setCursor(0, 56); engine.screen().print(info);
        drawBestTag(engine, best, 56);
        const int16_t cell = 16, ox = 64 - (GW * cell) / 2, oy = 12;
        // draw path first (under mirrors)
        int16_t lx = ox - 4, ly = oy + gr * cell + cell / 2;
        for (uint8_t i = 0; i < pn; i++) {
          int16_t cx = ox + pathx[i] * cell + cell / 2;
          int16_t cy = oy + pathy[i] * cell + cell / 2;
          if (pathx[i] >= GW) cx = ox + GW * cell + 4;  // clamp exit draw
          engine.screen().drawLine(lx, ly, cx, cy, GAMER_INVERSE);
          lx = cx; ly = cy;
        }
        // grid + mirrors
        for (uint8_t i = 0; i < GN; i++) {
          int16_t cx = ox + (i % GW) * cell, cy = oy + (i / GW) * cell;
          engine.screen().drawRect(cx, cy, cell, cell, GAMER_DIM);
          uint8_t m = mir[i];
          if (m == 0) engine.screen().drawLine(cx + 3, cy + cell - 3, cx + cell - 3, cy + 3, GAMER_WHITE);
          else if (m == 1) engine.screen().drawLine(cx + 3, cy + 3, cx + cell - 3, cy + cell - 3, GAMER_WHITE);
          if (i == cursor) drawCursorBox(engine, cx, cy, cell, cell);
        }
        // source & goal markers
        engine.screen().setTextColor(GAMER_INVERSE, GAMER_BLACK);
        engine.screen().setCursor(ox - 9, oy + gr * cell + 4); engine.screen().print(">");
        engine.screen().setCursor(ox + GW * cell + 2, oy + gr * cell + 4); engine.screen().print("X");
        engine.show();

        if (hit) {
          totalMoves += moves;
          engine.ledPulse(CRGB::Green, 160); engine.playSound(SOUND_LEVELUP);
          sparksSpawn(sf, ox + GW * cell, oy + gr * cell + cell / 2, GAMER_INVERSE, 10);
          for (uint8_t f = 0; f < 6; f++) { engine.clear();
            engine.centerText(lvl < 2 ? "ALIGNED!" : "ALL LIT!", 26, 2, GAMER_ACCENT);
            sparksUpdateDraw(sf, engine); engine.show(); engine.tick(); delay(40); }
          break;
        }
        delay(20);
      }
    }
    if (quit) return;
    bool rec = bestSubmitLow("laser", totalMoves);
    if (!resultScreenBest(engine, "ROUTED!", totalMoves, rec ? totalMoves : best, rec)) return;
  }
}
