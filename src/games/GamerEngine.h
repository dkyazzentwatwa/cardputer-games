#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5GFX.h>

#include "GamerConfig.h"
#include "GamerSound.h"

enum GamerButton : uint8_t {
  BTN_LEFT = 0,
  BTN_RIGHT = 1,
  BTN_SELECT = 2
};

struct ButtonRuntime {
  bool lastRawPressed = false;
  bool stablePressed = false;
  uint32_t lastRawChange = 0;
  uint32_t pressedAt = 0;
  uint16_t releaseDuration = 0;
  bool pressEvent = false;
  bool releaseEvent = false;
  bool longEvent = false;
  bool longFired = false;
};

struct GameInputEvent {
  bool pressed = false;
  bool enter = false;
  bool back = false;
  bool home = false;
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool del = false;
  bool tab = false;
  bool btnA = false;
  bool space = false;
  String text;
};

class CardputerGameDisplay : public Print {
public:
  void begin();
  bool ready() const;
  void clearPhysical();
  void pushFrame();

  void setTextSize(uint8_t size);
  void setTextColor(uint16_t color);
  void setTextColor(uint16_t color, uint16_t bg);
  void setCursor(int16_t x, int16_t y);
  void setTextWrap(bool wrap);
  void getTextBounds(const char* text, int16_t x, int16_t y, int16_t* x1, int16_t* y1,
                     uint16_t* w, uint16_t* h);
  void getTextBounds(const String& text, int16_t x, int16_t y, int16_t* x1, int16_t* y1,
                     uint16_t* w, uint16_t* h);

  void fillScreen(uint16_t color);
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2, uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color);

  size_t write(uint8_t value) override;

private:
  int16_t dx(int16_t x) const;
  int16_t dy(int16_t y) const;

  M5Canvas _canvas;
  uint8_t _textSize = 1;
  bool _useCanvas = false;
};

class GamerEngine {
public:
  typedef void (*ServiceCallback)();

  void begin(ServiceCallback serviceCallback = nullptr);
  void resetForLaunch();
  void tick();
  bool displayReady() const;

  bool isHeld(GamerButton button) const;
  bool wasPressed(GamerButton button) const;
  bool wasReleased(GamerButton button) const;
  bool wasLongPressed(GamerButton button) const;
  uint16_t releasedDuration(GamerButton button) const;
  bool shouldExitGame() const;
  void waitForRelease();

  void queueInput(const GameInputEvent& event);

  CardputerGameDisplay& screen();
  uint16_t width() const;
  uint16_t height() const;
  uint8_t textScale() const;
  String powerLabel();

  void clear();
  void show();
  void centerText(const char* text, int16_t y, uint8_t size = 1, uint16_t color = GAMER_WHITE);
  void rightText(const char* text, int16_t y = 0, uint16_t color = GAMER_WHITE);
  void drawTitle(const char* title, const char* line1, const char* line2);
  bool waitForSelectOrExit(const char* title, const char* line1, const char* line2);
  bool showResult(const char* title, const char* detail);
  void drawHeader(const char* title, int16_t value);

  void ledOff();
  void ledSet(const CRGB& color);
  void ledPulse(const CRGB& color, uint16_t durationMs);
  void playSound(GameSoundCue cue);
  bool toggleSoundMuted();
  bool soundMuted() const;

private:
  void clearEvents();
  void pollKeyboard();
  void updateButton(ButtonRuntime& button, bool rawPressed);
  void drawFeedbackBorder();
  bool textHasAny(const String& text, const char* chars) const;
  uint16_t toColor565(const CRGB& color) const;

  CardputerGameDisplay _display;
  GamerSound _sound;
  ButtonRuntime _buttons[3];
  ServiceCallback _serviceCallback = nullptr;
  bool _exitRequested = false;
  bool _muteKeyHeld = false;
  uint32_t _feedbackUntil = 0;
  uint16_t _feedbackColor = GAMER_ACCENT;
};
