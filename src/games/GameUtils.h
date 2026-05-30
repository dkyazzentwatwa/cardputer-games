#pragma once

#include "GamerEngine.h"

// ---------------------------------------------------------------------------
// Deepening toolkit
//
// Shared building blocks used by every game to add depth consistently:
// persistent high scores (microSD), difficulty selection, lives/score HUD,
// and lightweight "juice" (screen shake + particle sparks). All SD access
// degrades to a safe no-op when no card is present.
// ---------------------------------------------------------------------------

// Persistent high scores, stored on the microSD card in "/cpgames.hi".
// Keys are short stable identifiers (<= 15 chars), one per game/mode.
// bestLoad returns 0 when there is no stored record.
uint16_t bestLoad(const char* key);
// Higher-is-better. Stores value when it beats the record; returns true on a
// new record (so games can flash "NEW BEST!").
bool bestSubmit(const char* key, uint16_t value);
// Lower-is-better (reaction time, move counts, ...). A stored 0 means "unset".
bool bestSubmitLow(const char* key, uint16_t value);

// Difficulty picker layered on the intro screen. Returns 0 (Easy), 1 (Normal),
// 2 (Hard), or 255 when the player exits. `help` is an optional one-line hint.
uint8_t chooseDifficulty(GamerEngine& engine, const char* title, const char* help = nullptr);

// HUD helpers (logical 128x64 coordinates).
void drawLives(GamerEngine& engine, int16_t x, int16_t y, uint8_t lives);
void drawBestTag(GamerEngine& engine, uint16_t best, int16_t y = 0);

// Result screen that also reports the personal best and flags new records.
bool resultScreenBest(GamerEngine& engine, const char* title, int16_t score,
                      uint16_t best, bool isRecord);

// --- Juice -----------------------------------------------------------------
// Fixed-size particle burst (no heap). Spawn on hits/pickups, then update+draw
// every frame; expired sparks are skipped automatically.
struct Spark {
  int16_t x = 0;
  int16_t y = 0;
  int8_t vx = 0;
  int8_t vy = 0;
  uint8_t life = 0;
  uint16_t color = 0;
};

struct SparkField {
  static const uint8_t kMax = 14;
  Spark sparks[kMax];
};

void sparksSpawn(SparkField& field, int16_t x, int16_t y, uint16_t color, uint8_t count);
void sparksUpdateDraw(SparkField& field, GamerEngine& engine);

// Screen-shake amplitude for a decaying timer. Pass a value that you decrement
// each frame; returns a small random offset to add to draw coordinates.
int16_t shakeOffset(uint8_t intensity);

bool selectTap(GamerEngine& engine);
bool runIntro(GamerEngine& engine, const char* title, const char* help);
bool resultScreen(GamerEngine& engine, const char* title, int16_t value);
bool frameDue(uint32_t& nextFrame, uint16_t frameMs);
bool rectsOverlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                  int16_t bx, int16_t by, int16_t bw, int16_t bh);
int8_t heldAxis(GamerEngine& engine);
int8_t gameCursorStep(GamerEngine& engine, int8_t current, int8_t count);
void drawScore(GamerEngine& engine, int16_t value);
int16_t gameX(GamerEngine& engine, int16_t oledX);
int16_t gameY(GamerEngine& engine, int16_t oledY);
int16_t gameSize(GamerEngine& engine, int16_t oledSize);
void drawCursorBox(GamerEngine& engine, int16_t x, int16_t y, int16_t w, int16_t h);
void drawMiniGrid(GamerEngine& engine, uint8_t cols, uint8_t rows, uint8_t cell,
                  int16_t ox, int16_t oy);
