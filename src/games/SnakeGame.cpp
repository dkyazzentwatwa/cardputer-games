#include "Games.h"
#include "GameUtils.h"

// ---------------------------------------------------------------------------
// SNAKE (deepened)
//
// Difficulty sets starting speed + speed-up rate + wall behaviour. Eating food
// speeds the snake up progressively; every few foods is a "level up" (flash +
// sound) that, on the harder modes, drops a fair obstacle onto the board.
//
// Depth layers on top of classic Snake:
//   - Combo chain: eat foods in quick succession to build a x2/x3/x4 score
//     multiplier (SOUND_COMBO). The chain decays if you dawdle, so it rewards
//     planning an efficient route. The HUD shows the live combo + a draining bar.
//   - Two special foods that spawn occasionally on a timer:
//       * BONUS (orange, blinking): extra points, scaled by combo + level.
//       * SHIELD (magenta ring): grants one free pass through a wall/obstacle/
//         self-hit. Survive a fatal move and the shield is spent with a flash.
//   - Pause (Enter/Space/BtnA) freezes the sim with an overlay.
//
// Easy wraps around the edges; Normal/Hard use solid walls. All state lives in
// fixed-size arrays (no heap in the loop). SD best key: "snake".
// ---------------------------------------------------------------------------

namespace {
constexpr uint8_t CELL = GAMER_DISPLAY_IS_SH8601 ? 24 : 8;
constexpr uint8_t GRID_W = SCREEN_WIDTH / CELL;
// Reserve the top row for the HUD so score/best never overlaps the field.
constexpr int16_t HUD_H = 8;
constexpr uint8_t GRID_H = (SCREEN_HEIGHT - HUD_H - (GAMER_DISPLAY_IS_SH8601 ? 44 : 0)) / CELL;
constexpr uint8_t MAX_SNAKE = GRID_W * GRID_H;
constexpr uint8_t MAX_WALLS = 16;
constexpr uint8_t FOODS_PER_LEVEL = 4;       // level-up cadence
constexpr uint16_t BONUS_LIFE_MS = 6000;     // bonus / shield food timeout
constexpr uint16_t COMBO_WINDOW_MS = 2600;   // time to chain the next food
constexpr uint8_t MAX_COMBO = 4;             // multiplier cap (x4)

const char* kSnakeKey = "snake";

enum Direction : uint8_t { DIR_UP = 0, DIR_RIGHT = 1, DIR_DOWN = 2, DIR_LEFT = 3 };
enum SpecialKind : uint8_t { SPECIAL_NONE = 0, SPECIAL_BONUS = 1, SPECIAL_SHIELD = 2 };

struct Cell {
  int8_t x;
  int8_t y;
};

bool sameCell(const Cell& a, const Cell& b) { return a.x == b.x && a.y == b.y; }

bool snakeContains(const Cell snake[], uint8_t length, const Cell& cell) {
  for (uint8_t i = 0; i < length; i++) {
    if (sameCell(snake[i], cell)) return true;
  }
  return false;
}

bool wallsContain(const Cell walls[], uint8_t wallCount, const Cell& cell) {
  for (uint8_t i = 0; i < wallCount; i++) {
    if (sameCell(walls[i], cell)) return true;
  }
  return false;
}

bool opposite(Direction a, Direction b) { return (a + 2) % 4 == b; }

// Pick a random open cell not occupied by snake or walls. avoid lets us keep
// the regular food cell free when placing a special food.
Cell placeOpen(const Cell snake[], uint8_t length, const Cell walls[], uint8_t wallCount,
               const Cell* avoid) {
  Cell open[MAX_SNAKE];
  uint8_t openCount = 0;
  for (uint8_t y = 0; y < GRID_H; y++) {
    for (uint8_t x = 0; x < GRID_W; x++) {
      Cell c = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
      if (snakeContains(snake, length, c)) continue;
      if (wallsContain(walls, wallCount, c)) continue;
      if (avoid && sameCell(c, *avoid)) continue;
      open[openCount++] = c;
    }
  }
  if (openCount == 0) return {-1, -1};
  return open[random(0, openCount)];
}

void drawCellRect(GamerEngine& engine, int16_t ox, int16_t oy, const Cell& c, uint16_t color,
                  bool fill) {
  const int16_t px = ox + c.x * CELL;
  const int16_t py = oy + c.y * CELL;
  if (fill) {
    engine.screen().fillRect(px + 1, py + 1, CELL - 2, CELL - 2, color);
  } else {
    engine.screen().drawRect(px, py, CELL, CELL, color);
  }
}

struct World {
  Cell snake[MAX_SNAKE];
  uint8_t length;
  Direction dir;
  Direction pendingDir;
  Cell food;
  Cell walls[MAX_WALLS];
  uint8_t wallCount;
  // One special food slot at a time (bonus OR shield).
  SpecialKind special;
  Cell specialCell;
  uint32_t specialUntil;
  uint16_t score;
  uint8_t level;
  uint8_t foodSinceLevel;
  uint16_t frameDelay;
  bool wrap;
  uint8_t speedStep;   // ms shaved per food
  uint16_t minDelay;   // speed cap
  // Depth state.
  uint8_t combo;       // current multiplier 1..MAX_COMBO
  uint32_t comboUntil; // when the chain expires
  bool shield;         // one free fatal-hit pass
};

uint8_t comboMult(uint8_t combo) { return combo < 1 ? 1 : combo; }

void drawWorld(GamerEngine& engine, const World& w, uint16_t best, int16_t shake) {
  engine.clear();
  const int16_t ox = (engine.width() - GRID_W * CELL) / 2 + shake;
  const int16_t oy = HUD_H + shake;

  // Playfield border (visible solid wall on no-wrap modes).
  engine.screen().drawRect(ox - 1, oy - 1, GRID_W * CELL + 2, GRID_H * CELL + 2,
                           w.wrap ? GAMER_DIM : GAMER_WHITE);

  // Obstacles.
  for (uint8_t i = 0; i < w.wallCount; i++) {
    drawCellRect(engine, ox, oy, w.walls[i], GAMER_DIM, true);
  }

  // Regular food.
  drawCellRect(engine, ox, oy, w.food, GAMER_ACCENT, true);

  // Special food: blink as it nears expiry. Bonus = filled orange, Shield =
  // hollow magenta ring so the two read differently at a glance.
  if (w.special != SPECIAL_NONE) {
    bool show = true;
    uint32_t left = (w.specialUntil > millis()) ? (w.specialUntil - millis()) : 0;
    if (left < 2000) show = (millis() / 150) & 1;
    if (show) {
      if (w.special == SPECIAL_BONUS) {
        drawCellRect(engine, ox, oy, w.specialCell, TFT_ORANGE, true);
        drawCellRect(engine, ox, oy, w.specialCell, GAMER_WHITE, false);
      } else {
        drawCellRect(engine, ox, oy, w.specialCell, TFT_MAGENTA, false);
        const int16_t px = ox + w.specialCell.x * CELL;
        const int16_t py = oy + w.specialCell.y * CELL;
        engine.screen().drawPixel(px + CELL / 2, py + CELL / 2, GAMER_WHITE);
      }
    }
  }

  // Snake: head brighter than body; magenta outline while shielded.
  for (uint8_t i = 0; i < w.length; i++) {
    bool head = (i == w.length - 1);
    drawCellRect(engine, ox, oy, w.snake[i], head ? GAMER_ACCENT : GAMER_WHITE, !head);
    if (head) {
      drawCellRect(engine, ox, oy, w.snake[i], w.shield ? TFT_MAGENTA : GAMER_WHITE, false);
    }
  }

  // HUD: score (left of centre), level (far left), best (right). When a combo
  // is live, show its multiplier and a draining bar under the score.
  char buf[16];
  snprintf(buf, sizeof(buf), "%u", w.score);
  engine.centerText(buf, 0, 1, GAMER_WHITE);

  engine.screen().setTextColor(GAMER_WHITE, GAMER_BLACK);
  engine.screen().setTextSize(1);
  engine.screen().setCursor(1, 0);
  char lvl[10];
  snprintf(lvl, sizeof(lvl), "L%u", w.level);
  engine.screen().print(lvl);

  if (w.combo >= 2 && millis() < w.comboUntil) {
    // Combo tag just right of the level, plus a tiny draining bar.
    engine.screen().setTextColor(TFT_ORANGE, GAMER_BLACK);
    engine.screen().setCursor(18, 0);
    char cb[6];
    snprintf(cb, sizeof(cb), "x%u", comboMult(w.combo));
    engine.screen().print(cb);
    uint32_t left = w.comboUntil - millis();
    int16_t barW = (int16_t)((left * 12) / COMBO_WINDOW_MS);
    if (barW > 12) barW = 12;
    engine.screen().drawFastHLine(18, 7, 12, GAMER_DIM);
    if (barW > 0) engine.screen().drawFastHLine(18, 7, barW, TFT_ORANGE);
  }

  // Shield indicator (small magenta cell) tucked left of the best tag.
  if (w.shield) {
    engine.screen().fillRect(engine.width() - 36, 1, 5, 5, TFT_MAGENTA);
  }

  drawBestTag(engine, best, 0);
}

// Try to add one obstacle that is not on the snake, food, or directly in front
// of the head (so a level-up never feels like an instant-death trap).
void addWall(World& w) {
  if (w.wallCount >= MAX_WALLS) return;
  Cell head = w.snake[w.length - 1];
  for (uint8_t attempt = 0; attempt < 24; attempt++) {
    Cell avoid = w.food;
    Cell c = placeOpen(w.snake, w.length, w.walls, w.wallCount, &avoid);
    if (c.x < 0) return;
    // Keep a little breathing room around the head.
    int dx = c.x - head.x, dy = c.y - head.y;
    if (dx * dx + dy * dy < 9) continue;
    w.walls[w.wallCount++] = c;
    return;
  }
}

// Draw a centred pause overlay; caller pushes the frame.
void drawPauseOverlay(GamerEngine& engine) {
  int16_t w = engine.width(), h = engine.height();
  engine.screen().fillRect(w / 2 - 30, h / 2 - 10, 60, 20, GAMER_BLACK);
  engine.screen().drawRect(w / 2 - 30, h / 2 - 10, 60, 20, GAMER_ACCENT);
  engine.centerText("PAUSED", h / 2 - 7, 1, GAMER_ACCENT);
  engine.centerText("Del:quit", h / 2 + 2, 1, GAMER_DIM);
}
}  // namespace

void runSnake(GamerEngine& engine) {
  uint16_t best = bestLoad(kSnakeKey);

  while (true) {
    uint8_t diff = chooseDifficulty(engine, "SNAKE",
                                    "Chain eats for combos; grab shields.");
    if (diff == 255) return;

    World w;
    w.length = 3;
    w.snake[0] = {static_cast<int8_t>(GRID_W / 2 - 1), static_cast<int8_t>(GRID_H / 2)};
    w.snake[1] = {static_cast<int8_t>(GRID_W / 2), static_cast<int8_t>(GRID_H / 2)};
    w.snake[2] = {static_cast<int8_t>(GRID_W / 2 + 1), static_cast<int8_t>(GRID_H / 2)};
    w.dir = DIR_RIGHT;
    w.pendingDir = DIR_RIGHT;
    w.wallCount = 0;
    w.special = SPECIAL_NONE;
    w.specialCell = {-1, -1};
    w.specialUntil = 0;
    w.score = 0;
    w.level = 1;
    w.foodSinceLevel = 0;
    w.wrap = (diff == 0);  // Easy wraps; Normal/Hard solid walls.
    w.combo = 1;
    w.comboUntil = 0;
    w.shield = false;

    // Difficulty: starting speed + speed-up rate + speed cap.
    if (diff == 0) { w.frameDelay = 190; w.speedStep = 4; w.minDelay = 95; }
    else if (diff == 1) { w.frameDelay = 150; w.speedStep = 6; w.minDelay = 75; }
    else { w.frameDelay = 115; w.speedStep = 8; w.minDelay = 55; }

    Cell avoidNone = {-1, -1};
    w.food = placeOpen(w.snake, w.length, w.walls, w.wallCount, &avoidNone);

    SparkField sparks;
    uint8_t shake = 0;
    uint32_t nextFrame = 0;
    uint32_t nextSpecialTry = millis() + random(7000, 12000);
    bool alive = true;
    bool paused = false;
    uint8_t levelFlash = 0;   // frames remaining for level-up flash
    uint8_t shieldFlash = 0;  // frames remaining for shield-save flash

    engine.waitForRelease();
    drawWorld(engine, w, best, 0);
    engine.show();

    while (alive) {
      engine.tick();
      if (engine.shouldExitGame()) {
        engine.waitForRelease();
        return;
      }

      // Pause toggle on select (Enter/Space/BtnA).
      if (engine.wasPressed(BTN_SELECT)) {
        paused = !paused;
        engine.playSound(SOUND_UI_SELECT);
        if (paused) {
          drawWorld(engine, w, best, 0);
          drawPauseOverlay(engine);
          engine.show();
        }
      }
      if (paused) {
        // Hold the chain timer steady while paused (push it forward).
        delay(8);
        continue;
      }

      // Absolute direction input (arrows/WASD/HJKL). Buffer the turn and reject
      // 180-degree reversals against the *current* heading.
      Direction want = w.pendingDir;
      if (engine.wasDirectionPressed(GAMER_DIR_UP)) want = DIR_UP;
      else if (engine.wasDirectionPressed(GAMER_DIR_DOWN)) want = DIR_DOWN;
      else if (engine.wasDirectionPressed(GAMER_DIR_LEFT)) want = DIR_LEFT;
      else if (engine.wasDirectionPressed(GAMER_DIR_RIGHT)) want = DIR_RIGHT;
      if (want != w.dir && !opposite(want, w.dir)) {
        if (want != w.pendingDir) engine.playSound(SOUND_UI_MOVE);
        w.pendingDir = want;
      }

      // Combo decay: if the window lapses, drop back to x1.
      if (w.combo >= 2 && millis() >= w.comboUntil) {
        w.combo = 1;
      }

      // Special food timeout.
      if (w.special != SPECIAL_NONE && millis() >= w.specialUntil) {
        w.special = SPECIAL_NONE;
      }

      if (!frameDue(nextFrame, w.frameDelay)) {
        // Keep juice/animation lively between simulation steps.
        if (shake > 0 || levelFlash > 0 || shieldFlash > 0) {
          drawWorld(engine, w, best, shake ? shakeOffset(shake) : 0);
          if (levelFlash > 0) {
            engine.screen().drawRect(0, 0, engine.width(), engine.height(), GAMER_ACCENT);
            levelFlash--;
          }
          if (shieldFlash > 0) {
            engine.screen().drawRect(0, 0, engine.width(), engine.height(), TFT_MAGENTA);
            shieldFlash--;
          }
          sparksUpdateDraw(sparks, engine);
          engine.show();
          if (shake > 0) shake--;
        }
        delay(2);
        continue;
      }

      // Commit buffered turn, advance head.
      w.dir = w.pendingDir;
      Cell head = w.snake[w.length - 1];
      if (w.dir == DIR_UP) head.y--;
      else if (w.dir == DIR_DOWN) head.y++;
      else if (w.dir == DIR_LEFT) head.x--;
      else head.x++;

      bool fatalWall = false;
      if (w.wrap) {
        if (head.x < 0) head.x = GRID_W - 1;
        else if (head.x >= GRID_W) head.x = 0;
        if (head.y < 0) head.y = GRID_H - 1;
        else if (head.y >= GRID_H) head.y = 0;
      } else if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H) {
        fatalWall = true;
      }

      // Self / obstacle collision. The tail cell will move this step (when not
      // eating), so colliding with the current tail is allowed.
      bool willGrow = !fatalWall && (sameCell(head, w.food) ||
                      (w.special != SPECIAL_NONE && sameCell(head, w.specialCell)));
      bool fatalBody = false;
      if (!fatalWall) {
        for (uint8_t i = 0; i < w.length; i++) {
          if (i == 0 && !willGrow) continue;  // tail vacates
          if (sameCell(w.snake[i], head)) { fatalBody = true; break; }
        }
      }
      bool fatalObstacle = !fatalWall && !fatalBody &&
                           wallsContain(w.walls, w.wallCount, head);

      if (fatalWall || fatalBody || fatalObstacle) {
        // Shield absorbs one fatal move. On a wall hit while wrapping is off we
        // can't continue (no valid cell), so the shield only saves body/obstacle
        // collisions; wall deaths still end the run for fairness/consistency.
        if (w.shield && (fatalBody || fatalObstacle)) {
          w.shield = false;
          shieldFlash = 4;
          shake = 4;
          w.combo = 1;
          const int16_t ox = (engine.width() - GRID_W * CELL) / 2;
          const int16_t oy = HUD_H;
          sparksSpawn(sparks, ox + head.x * CELL + CELL / 2, oy + head.y * CELL + CELL / 2,
                      TFT_MAGENTA, 12);
          engine.ledPulse(CRGB::Magenta, 200);
          engine.playSound(SOUND_HIT);
          // Don't move into the offending cell; re-draw and skip this step.
          drawWorld(engine, w, best, 0);
          sparksUpdateDraw(sparks, engine);
          engine.show();
          continue;
        }
        alive = false;
        break;
      }

      const int16_t ox = (engine.width() - GRID_W * CELL) / 2;
      const int16_t oy = HUD_H;

      bool ateSpecial = w.special != SPECIAL_NONE && sameCell(head, w.specialCell);
      bool ateBonus = ateSpecial && w.special == SPECIAL_BONUS;
      bool ateShield = ateSpecial && w.special == SPECIAL_SHIELD;
      bool ateFood = sameCell(head, w.food);

      if (ateBonus) {
        if (w.length < MAX_SNAKE) w.snake[w.length++] = head;
        w.special = SPECIAL_NONE;
        // Bonus scales with current combo and level, rewarding aggressive play.
        uint16_t gain = (uint16_t)(3 + w.level) * comboMult(w.combo);
        w.score += gain;
        // A grab keeps the combo alive too.
        w.combo = (w.combo < MAX_COMBO) ? w.combo + 1 : MAX_COMBO;
        w.comboUntil = millis() + COMBO_WINDOW_MS;
        sparksSpawn(sparks, ox + head.x * CELL + CELL / 2, oy + head.y * CELL + CELL / 2,
                    TFT_ORANGE, 12);
        engine.ledPulse(CRGB::Magenta, 120);
        engine.playSound(SOUND_POWERUP);
      } else if (ateShield) {
        if (w.length < MAX_SNAKE) w.snake[w.length++] = head;
        w.special = SPECIAL_NONE;
        w.shield = true;
        shieldFlash = 3;
        sparksSpawn(sparks, ox + head.x * CELL + CELL / 2, oy + head.y * CELL + CELL / 2,
                    TFT_MAGENTA, 12);
        engine.ledPulse(CRGB::Purple, 160);
        engine.playSound(SOUND_POWERUP);
      } else if (ateFood) {
        if (w.length < MAX_SNAKE) w.snake[w.length++] = head;
        // Combo: chained within the window bumps the multiplier.
        if (millis() < w.comboUntil && w.combo < MAX_COMBO) {
          w.combo++;
          if (w.combo >= 2) engine.playSound(SOUND_COMBO);
        } else if (millis() >= w.comboUntil) {
          w.combo = 1;
        }
        w.comboUntil = millis() + COMBO_WINDOW_MS;
        w.score += comboMult(w.combo);
        w.foodSinceLevel++;
        // Progressive speed-up.
        w.frameDelay = (w.frameDelay > w.minDelay + w.speedStep)
                           ? w.frameDelay - w.speedStep : w.minDelay;
        w.food = placeOpen(w.snake, w.length, w.walls, w.wallCount, &avoidNone);
        sparksSpawn(sparks, ox + head.x * CELL + CELL / 2, oy + head.y * CELL + CELL / 2,
                    GAMER_ACCENT, 6);
        engine.ledPulse(CRGB::Green, 80);
        engine.playSound(SOUND_SCORE);

        // Level up every N foods: flash + sound, and a fair obstacle on
        // Normal/Hard.
        if (w.foodSinceLevel >= FOODS_PER_LEVEL) {
          w.foodSinceLevel = 0;
          w.level++;
          levelFlash = 4;
          engine.ledPulse(CRGB::Cyan, 160);
          engine.playSound(SOUND_LEVELUP);
          if (diff >= 1) {
            addWall(w);
            if (diff == 2) addWall(w);  // Hard packs more obstacles
          }
        }
      } else {
        // Move: shift body toward head.
        for (uint8_t i = 1; i < w.length; i++) w.snake[i - 1] = w.snake[i];
        w.snake[w.length - 1] = head;
      }

      // Occasionally spawn a timed special when none is active. Shields are
      // rarer than bonuses and slightly more likely on harder modes.
      if (w.special == SPECIAL_NONE && millis() >= nextSpecialTry) {
        Cell avoid = w.food;
        Cell c = placeOpen(w.snake, w.length, w.walls, w.wallCount, &avoid);
        if (c.x >= 0) {
          // ~30% shield, else bonus; never offer a shield if one is held.
          bool wantShield = !w.shield && random(0, 100) < 30;
          w.specialCell = c;
          w.special = wantShield ? SPECIAL_SHIELD : SPECIAL_BONUS;
          w.specialUntil = millis() + BONUS_LIFE_MS;
          engine.playSound(SOUND_COMBO);
        }
        nextSpecialTry = millis() + random(8000, 15000);
      }

      drawWorld(engine, w, best, 0);
      if (levelFlash > 0) {
        engine.screen().drawRect(0, 0, engine.width(), engine.height(), GAMER_ACCENT);
        levelFlash--;
      }
      if (shieldFlash > 0) {
        engine.screen().drawRect(0, 0, engine.width(), engine.height(), TFT_MAGENTA);
        shieldFlash--;
      }
      sparksUpdateDraw(sparks, engine);
      engine.show();
    }

    // Death: shake + red flash + sound.
    engine.ledPulse(CRGB::Red, 280);
    engine.playSound(SOUND_LOSE);
    for (uint8_t f = 0; f < 8; f++) {
      drawWorld(engine, w, best, shakeOffset(6));
      sparksUpdateDraw(sparks, engine);
      engine.show();
      delay(28);
    }

    bool record = bestSubmit(kSnakeKey, w.score);
    if (record) best = w.score;
    if (!resultScreenBest(engine, "GAME OVER", w.score, best, record)) return;
  }
}
