#include <Arduino.h>
#include <M5Cardputer.h>
#include <CypherPuterReturn.h>
#include <esp_system.h>

#include "src/games/GameScreen.h"

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t INTRO_SPLASH_MS = 4000;

GameScreen gameScreen;

GameInputEvent readGameInput();
void drawIntroSplash();
void serviceGameRuntime();
const char* resetReasonName(esp_reset_reason_t reason);

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setBrightness(180);

  randomSeed(esp_random() ^ micros());

  Serial.println();
  Serial.println("cardputer-games boot");
  Serial.printf("reset reason=%s heap=%u\n", resetReasonName(esp_reset_reason()),
                ESP.getFreeHeap());
  Serial.println("controls: arrows/WASD/HJKL move, Enter/Space/BtnA act, M mute, Del/Tab/Q return to launcher");

  drawIntroSplash();

  gameScreen.begin(serviceGameRuntime);
  gameScreen.enter();
}

void loop() {
  M5Cardputer.update();

  GameInputEvent event = readGameInput();
  if (event.pressed) {
    if (gameScreen.handleInput(event)) {
      cypherPuterReturnToLauncher();
    }
  }

  delay(8);
}

GameInputEvent readGameInput() {
  GameInputEvent event;
  event.btnA = M5Cardputer.BtnA.wasClicked();

  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    event.pressed = event.btnA;
    return event;
  }

  event.pressed = true;
  Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
  event.enter = keys.enter;
  event.del = keys.del;
  event.back = keys.del;
  event.tab = keys.tab;
  event.home = keys.tab;
  event.space = keys.space;

  if (keys.space) {
    event.text += ' ';
  }

  for (auto c : keys.word) {
    if (c == 'w' || c == 'W' || c == 'k' || c == 'K') event.up = true;
    else if (c == 's' || c == 'S' || c == 'j' || c == 'J') event.down = true;
    else if (c == 'a' || c == 'A' || c == 'h' || c == 'H') event.left = true;
    else if (c == 'd' || c == 'D' || c == 'l' || c == 'L') event.right = true;
    event.text += c;
  }

  for (const auto& key : M5Cardputer.Keyboard.keyList()) {
    if (key.y == 3 && key.x == 10) event.left = true;
    if (key.y == 3 && key.x == 12) event.right = true;
    if (key.y == 2 && key.x == 11) event.up = true;
    if (key.y == 3 && key.x == 11) event.down = true;
  }

  return event;
}

template <typename GfxT>
void drawIntroSplashFrame(GfxT& gfx, int16_t width, int16_t height, uint32_t elapsed) {
  static const uint8_t starX[] = {14, 32, 49, 78, 103, 127, 151, 174, 199, 221};
  static const uint8_t starY[] = {18, 96, 37, 112, 23, 88, 48, 15, 104, 69};
  static const uint8_t starSpeed[] = {1, 2, 1, 3, 2, 1, 2, 1, 3, 2};
  const int16_t sweep = (elapsed / 16) % height;
  const int16_t pulse = (elapsed / 90) % 18;
  const int16_t progress = min<int16_t>(width - 44,
                                        ((width - 44) * elapsed) / INTRO_SPLASH_MS);

  gfx.fillScreen(TFT_BLACK);
  gfx.fillRect(0, 0, width, height, 0x0008);

  for (int16_t y = 10; y < height; y += 16) {
    gfx.drawFastHLine(12, y, width - 24, 0x0841);
  }
  for (int16_t x = 18; x < width; x += 24) {
    gfx.drawFastVLine(x, 8, height - 16, 0x0821);
  }

  for (uint8_t i = 0; i < sizeof(starX); ++i) {
    const int16_t x = (starX[i] + (elapsed / (40 / starSpeed[i]))) % width;
    const int16_t y = starY[i];
    const uint16_t color = (i % 3 == 0) ? TFT_CYAN : ((i % 3 == 1) ? TFT_DARKCYAN : TFT_BLUE);
    gfx.drawPixel(x, y, color);
    if (i % 4 == 0) {
      gfx.drawPixel((x + 1) % width, y, color);
    }
  }

  gfx.drawRect(5, 5, width - 10, height - 10, TFT_DARKCYAN);
  gfx.drawRect(8, 8, width - 16, height - 16, TFT_BLUE);
  gfx.drawFastHLine(12, 12, 34 + pulse, TFT_CYAN);
  gfx.drawFastHLine(width - 46 - pulse, 12, 34 + pulse, TFT_CYAN);
  gfx.drawFastHLine(12, height - 13, 34 + pulse, TFT_CYAN);
  gfx.drawFastHLine(width - 46 - pulse, height - 13, 34 + pulse, TFT_CYAN);
  gfx.drawFastVLine(12, 12, 24, TFT_CYAN);
  gfx.drawFastVLine(width - 13, 12, 24, TFT_CYAN);
  gfx.drawFastVLine(12, height - 36, 24, TFT_CYAN);
  gfx.drawFastVLine(width - 13, height - 36, 24, TFT_CYAN);

  gfx.fillRect(10, sweep, width - 20, 2, 0x03FF);
  if (sweep > 0) {
    gfx.drawFastHLine(14, sweep - 1, width - 28, 0x01D7);
  }

  gfx.setTextSize(2);
  gfx.setTextColor(TFT_BLACK, TFT_BLACK);
  gfx.setCursor(42, 44);
  gfx.print("Cypher-Gamer");
  gfx.setTextColor(TFT_CYAN, TFT_BLACK);
  gfx.setCursor(40, 42);
  gfx.print("Cypher-Gamer");

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  gfx.setCursor(84, 67);
  gfx.print("by littlehakr");
  gfx.setTextColor(TFT_DARKCYAN, TFT_BLACK);
  gfx.setCursor(75, 82);
  gfx.print("ARCADE CORE ONLINE");

  gfx.drawRect(21, height - 25, width - 42, 8, TFT_DARKCYAN);
  gfx.fillRect(23, height - 23, progress, 4, TFT_CYAN);
  gfx.fillRect(23 + progress, height - 23, 3, 4, TFT_WHITE);
}

void drawIntroSplash() {
  auto& display = M5Cardputer.Display;
  const int16_t width = display.width();
  const int16_t height = display.height();
  const uint32_t started = millis();
  M5Canvas frame;
  frame.setColorDepth(16);
  const bool buffered = frame.createSprite(width, height) != nullptr;

  Serial.println("intro splash start");
  display.setRotation(1);
  display.setTextDatum(top_left);
  display.setTextWrap(false);
  if (buffered) {
    frame.setTextDatum(top_left);
    frame.setTextWrap(false);
  }

  while (millis() - started < INTRO_SPLASH_MS) {
    const uint32_t elapsed = millis() - started;
    M5Cardputer.update();
    if (buffered) {
      drawIntroSplashFrame(frame, width, height, elapsed);
      frame.pushSprite(&display, 0, 0);
    } else {
      drawIntroSplashFrame(display, width, height, elapsed);
    }

    delay(33);
    yield();
  }

  if (buffered) {
    frame.deleteSprite();
  }
  display.fillScreen(TFT_BLACK);
  Serial.println("intro splash done");
}

void serviceGameRuntime() {
  M5Cardputer.update();
}

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}
