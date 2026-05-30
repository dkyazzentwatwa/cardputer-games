#include "GameUtils.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

// ---------------------------------------------------------------------------
// Persistent high scores on microSD ("/cpgames.hi").
//
// The file is a tiny line-based store: "<key> <value>\n". With ~53 keys it
// stays well under 1KB, so we just rewrite the whole file on submit. Every
// entry point lazily initializes the card and silently no-ops when no card is
// present, so games behave identically with or without storage.
// ---------------------------------------------------------------------------
namespace {
constexpr const char* kHiPath = "/cpgames.hi";
constexpr uint8_t kMaxRecords = 96;
constexpr uint8_t kKeyLen = 16;

struct HiRecord {
  char key[kKeyLen];
  uint16_t value;
};

int8_t g_sdState = -1;  // -1 unknown, 0 unavailable, 1 ready

bool sdReady() {
  if (g_sdState >= 0) {
    return g_sdState == 1;
  }
  // M5Cardputer microSD shares the SPI bus; these are the documented pins.
  SPI.begin(40, 39, 14, 12);
  g_sdState = SD.begin(12, SPI, 25000000) ? 1 : 0;
  return g_sdState == 1;
}

uint8_t loadAll(HiRecord* out) {
  uint8_t count = 0;
  if (!sdReady() || !SD.exists(kHiPath)) {
    return 0;
  }
  File f = SD.open(kHiPath, FILE_READ);
  if (!f) {
    return 0;
  }
  while (f.available() && count < kMaxRecords) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    int sep = line.indexOf(' ');
    if (sep <= 0) {
      continue;
    }
    String k = line.substring(0, sep);
    long v = line.substring(sep + 1).toInt();
    if (k.length() == 0 || k.length() >= kKeyLen) {
      continue;
    }
    strncpy(out[count].key, k.c_str(), kKeyLen - 1);
    out[count].key[kKeyLen - 1] = '\0';
    out[count].value = static_cast<uint16_t>(constrain(v, 0, 65535));
    ++count;
  }
  f.close();
  return count;
}

bool saveAll(const HiRecord* records, uint8_t count) {
  if (!sdReady()) {
    return false;
  }
  File f = SD.open(kHiPath, FILE_WRITE);
  if (!f) {
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    f.printf("%s %u\n", records[i].key, static_cast<unsigned>(records[i].value));
  }
  f.close();
  return true;
}

// Returns index of key, or -1. Used by both load and submit paths.
int16_t findKey(const HiRecord* records, uint8_t count, const char* key) {
  for (uint8_t i = 0; i < count; ++i) {
    if (strncmp(records[i].key, key, kKeyLen) == 0) {
      return i;
    }
  }
  return -1;
}

bool submitRecord(const char* key, uint16_t value, bool lowerBetter) {
  if (!key || !sdReady()) {
    return false;
  }
  static HiRecord records[kMaxRecords];
  uint8_t count = loadAll(records);
  int16_t idx = findKey(records, count, key);

  bool isRecord = false;
  if (idx < 0) {
    // First-ever entry. For lower-is-better, a value of 0 still counts as a
    // record so the slot gets created.
    if (count >= kMaxRecords) {
      return false;
    }
    strncpy(records[count].key, key, kKeyLen - 1);
    records[count].key[kKeyLen - 1] = '\0';
    records[count].value = value;
    ++count;
    isRecord = true;
  } else {
    const uint16_t cur = records[idx].value;
    isRecord = lowerBetter ? (value < cur) : (value > cur);
    if (isRecord) {
      records[idx].value = value;
    }
  }

  if (isRecord) {
    saveAll(records, count);
  }
  return isRecord;
}
}  // namespace

uint16_t bestLoad(const char* key) {
  if (!key) {
    return 0;
  }
  static HiRecord records[kMaxRecords];
  uint8_t count = loadAll(records);
  int16_t idx = findKey(records, count, key);
  return idx < 0 ? 0 : records[idx].value;
}

bool bestSubmit(const char* key, uint16_t value) {
  return submitRecord(key, value, false);
}

bool bestSubmitLow(const char* key, uint16_t value) {
  return submitRecord(key, value, true);
}

uint8_t chooseDifficulty(GamerEngine& engine, const char* title, const char* help) {
  static const char* kNames[3] = {"EASY", "NORMAL", "HARD"};
  static const uint16_t kColors[3] = {TFT_GREEN, GAMER_ACCENT, TFT_ORANGE};
  int8_t pick = 1;
  engine.waitForRelease();

  while (true) {
    engine.tick();
    if (engine.shouldExitGame()) {
      engine.waitForRelease();
      engine.playSound(SOUND_UI_BACK);
      return 255;
    }
    pick = gameCursorStep(engine, pick, 3);
    if (selectTap(engine)) {
      engine.waitForRelease();
      engine.playSound(SOUND_UI_SELECT);
      return static_cast<uint8_t>(pick);
    }

    engine.clear();
    engine.screen().drawRect(0, 0, engine.width(), engine.height(), GAMER_DIM);
    engine.centerText(title, 6, strlen(title) > 10 ? 1 : 2, GAMER_WHITE);
    if (help) {
      engine.centerText(help, 26, 1, GAMER_DIM);
    }
    const int16_t boxW = 32;
    const int16_t gap = 4;
    const int16_t totalW = boxW * 3 + gap * 2;
    int16_t bx = (engine.width() - totalW) / 2;
    const int16_t by = 38;
    for (uint8_t i = 0; i < 3; ++i) {
      const bool sel = i == pick;
      engine.screen().drawRoundRect(bx, by, boxW, 16, 3, sel ? GAMER_WHITE : GAMER_DIM);
      if (sel) {
        engine.screen().fillRoundRect(bx + 1, by + 1, boxW - 2, 14, 2, kColors[i]);
      }
      engine.screen().setTextColor(sel ? GAMER_BLACK : kColors[i], GAMER_BLACK);
      engine.screen().setTextSize(1);
      const int16_t tw = strlen(kNames[i]) * 6;
      engine.screen().setCursor(bx + (boxW - tw) / 2, by + 5);
      engine.screen().print(kNames[i]);
      bx += boxW + gap;
    }
    engine.centerText("SEL choose  Del exit", engine.height() - 9, 1, GAMER_DIM);
    engine.show();
    delay(10);
  }
}

void drawLives(GamerEngine& engine, int16_t x, int16_t y, uint8_t lives) {
  for (uint8_t i = 0; i < lives; ++i) {
    const int16_t lx = x + i * 7;
    // Tiny heart: two top pixels and a triangle body.
    engine.screen().fillRect(lx, y + 1, 2, 2, TFT_RED);
    engine.screen().fillRect(lx + 3, y + 1, 2, 2, TFT_RED);
    engine.screen().fillRect(lx, y + 2, 5, 2, TFT_RED);
    engine.screen().fillRect(lx + 1, y + 4, 3, 1, TFT_RED);
    engine.screen().drawPixel(lx + 2, y + 5, TFT_RED);
  }
}

void drawBestTag(GamerEngine& engine, uint16_t best, int16_t y) {
  if (best == 0) {
    return;
  }
  char tag[16];
  snprintf(tag, sizeof(tag), "HI %u", static_cast<unsigned>(best));
  engine.rightText(tag, y, GAMER_DIM);
}

bool resultScreenBest(GamerEngine& engine, const char* title, int16_t score,
                      uint16_t best, bool isRecord) {
  char detail[26];
  if (isRecord) {
    snprintf(detail, sizeof(detail), "NEW BEST %d", score);
  } else if (best > 0) {
    snprintf(detail, sizeof(detail), "%d  (HI %u)", score, static_cast<unsigned>(best));
  } else {
    snprintf(detail, sizeof(detail), "Score %d", score);
  }
  if (isRecord) {
    engine.playSound(SOUND_LEVELUP);
  }
  return engine.showResult(title, detail);
}

// --- Juice -----------------------------------------------------------------
void sparksSpawn(SparkField& field, int16_t x, int16_t y, uint16_t color, uint8_t count) {
  uint8_t spawned = 0;
  for (uint8_t i = 0; i < SparkField::kMax && spawned < count; ++i) {
    Spark& s = field.sparks[i];
    if (s.life != 0) {
      continue;
    }
    s.x = x;
    s.y = y;
    s.vx = static_cast<int8_t>(random(-3, 4));
    s.vy = static_cast<int8_t>(random(-4, 2));
    s.life = static_cast<uint8_t>(random(5, 11));
    s.color = color;
    ++spawned;
  }
}

void sparksUpdateDraw(SparkField& field, GamerEngine& engine) {
  for (uint8_t i = 0; i < SparkField::kMax; ++i) {
    Spark& s = field.sparks[i];
    if (s.life == 0) {
      continue;
    }
    s.x += s.vx;
    s.y += s.vy;
    ++s.vy;  // gravity
    --s.life;
    if (s.x < 0 || s.x >= static_cast<int16_t>(engine.width()) || s.y >= static_cast<int16_t>(engine.height())) {
      s.life = 0;
      continue;
    }
    engine.screen().drawPixel(s.x, s.y, s.color);
    if (s.life > 6) {
      engine.screen().drawPixel(s.x + 1, s.y, s.color);
    }
  }
}

int16_t shakeOffset(uint8_t intensity) {
  if (intensity == 0) {
    return 0;
  }
  return static_cast<int16_t>(random(-static_cast<int>(intensity), intensity + 1));
}

bool selectTap(GamerEngine& engine) {
  const bool tapped = engine.wasReleased(BTN_SELECT) &&
                      engine.releasedDuration(BTN_SELECT) < BUTTON_LONGPRESS_MS;
  if (tapped) {
    engine.playSound(SOUND_ACTION);
  }
  return tapped;
}

bool checkExit(GamerEngine& engine) {
  if (!engine.shouldExitGame()) return false;
  engine.waitForRelease();
  return true;
}

bool runIntro(GamerEngine& engine, const char* title, const char* help) {
  return engine.waitForSelectOrExit(title, help, "SEL start");
}

bool resultScreen(GamerEngine& engine, const char* title, int16_t value) {
  char detail[18];
  snprintf(detail, sizeof(detail), "Score %d", value);
  return engine.showResult(title, detail);
}

bool frameDue(uint32_t& nextFrame, uint16_t frameMs) {
  uint32_t now = millis();
  if (now < nextFrame) return false;
  nextFrame = now + frameMs;
  return true;
}

bool rectsOverlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                  int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

int8_t heldAxis(GamerEngine& engine) {
  if (engine.isHeld(BTN_LEFT)) return -1;
  if (engine.isHeld(BTN_RIGHT)) return 1;
  return 0;
}

int8_t gameCursorStep(GamerEngine& engine, int8_t current, int8_t count) {
  const int8_t previous = current;
  if (engine.wasPressed(BTN_LEFT)) current = current == 0 ? count - 1 : current - 1;
  if (engine.wasPressed(BTN_RIGHT)) current = (current + 1) % count;
  if (current != previous) {
    engine.playSound(SOUND_UI_MOVE);
  }
  return current;
}

void drawScore(GamerEngine& engine, int16_t value) {
  char scoreText[10];
  snprintf(scoreText, sizeof(scoreText), "%d", value);
  engine.rightText(scoreText);
}

void drawStat(GamerEngine& engine, const char* label, int16_t value) {
  engine.screen().setTextSize(engine.textScale());
  engine.screen().setTextColor(GAMER_WHITE, GAMER_BLACK);
  engine.screen().setCursor(0, 0);
  engine.screen().print(label);
  engine.screen().print(value);
}

int16_t gameX(GamerEngine& engine, int16_t oledX) {
  return static_cast<int32_t>(oledX) * engine.width() / 128;
}

int16_t gameY(GamerEngine& engine, int16_t oledY) {
  return static_cast<int32_t>(oledY) * engine.height() / 64;
}

int16_t gameSize(GamerEngine& engine, int16_t oledSize) {
  const int16_t sx = max<int16_t>(1, static_cast<int32_t>(oledSize) * engine.width() / 128);
  const int16_t sy = max<int16_t>(1, static_cast<int32_t>(oledSize) * engine.height() / 64);
  return min(sx, sy);
}

void drawCursorBox(GamerEngine& engine, int16_t x, int16_t y, int16_t w, int16_t h) {
  engine.screen().drawRect(x - 1, y - 1, w + 2, h + 2, GAMER_WHITE);
}

void drawMiniGrid(GamerEngine& engine, uint8_t cols, uint8_t rows, uint8_t cell,
                  int16_t ox, int16_t oy) {
  const int16_t scaledCell = gameSize(engine, cell);
  const int16_t scaledOx = gameX(engine, ox);
  const int16_t scaledOy = gameY(engine, oy);
  for (uint8_t x = 0; x <= cols; x++) {
    engine.screen().drawFastVLine(scaledOx + x * scaledCell, scaledOy, rows * scaledCell,
                                  GAMER_WHITE);
  }
  for (uint8_t y = 0; y <= rows; y++) {
    engine.screen().drawFastHLine(scaledOx, scaledOy + y * scaledCell, cols * scaledCell,
                                  GAMER_WHITE);
  }
}
