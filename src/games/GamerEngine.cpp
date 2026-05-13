#include "GamerEngine.h"

#include <string.h>

const CRGB CRGB::Black = CRGB(0, 0, 0);
const CRGB CRGB::Red = CRGB(255, 0, 0);
const CRGB CRGB::Green = CRGB(0, 255, 0);
const CRGB CRGB::Blue = CRGB(0, 0, 255);
const CRGB CRGB::Purple = CRGB(160, 0, 255);
const CRGB CRGB::Aqua = CRGB(0, 220, 255);
const CRGB CRGB::Orange = CRGB(255, 120, 0);

void CardputerGameDisplay::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextDatum(top_left);
  M5Cardputer.Display.setTextWrap(false);
  _canvas.setColorDepth(16);
  _useCanvas = _canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT) != nullptr;
  if (_useCanvas) {
    _canvas.setTextDatum(top_left);
    _canvas.setTextWrap(false);
  }
  setTextSize(1);
  setTextColor(GAMER_WHITE, GAMER_BLACK);
}

bool CardputerGameDisplay::ready() const {
  return true;
}

void CardputerGameDisplay::clearPhysical() {
  M5Cardputer.Display.fillScreen(GAMER_BLACK);
}

void CardputerGameDisplay::pushFrame() {
  if (_useCanvas) {
    _canvas.pushSprite(&M5Cardputer.Display, CARDPUTER_GAME_VIEWPORT_X,
                       CARDPUTER_GAME_VIEWPORT_Y);
  }
}

void CardputerGameDisplay::setTextSize(uint8_t size) {
  _textSize = max<uint8_t>(1, size);
  if (_useCanvas) {
    _canvas.setTextSize(_textSize);
  } else {
    M5Cardputer.Display.setTextSize(_textSize);
  }
}

void CardputerGameDisplay::setTextColor(uint16_t color) {
  if (_useCanvas) {
    _canvas.setTextColor(color);
  } else {
    M5Cardputer.Display.setTextColor(color);
  }
}

void CardputerGameDisplay::setTextColor(uint16_t color, uint16_t bg) {
  if (_useCanvas) {
    _canvas.setTextColor(color, bg);
  } else {
    M5Cardputer.Display.setTextColor(color, bg);
  }
}

void CardputerGameDisplay::setCursor(int16_t x, int16_t y) {
  if (_useCanvas) {
    _canvas.setCursor(x, y);
  } else {
    M5Cardputer.Display.setCursor(dx(x), dy(y));
  }
}

void CardputerGameDisplay::setTextWrap(bool wrap) {
  if (_useCanvas) {
    _canvas.setTextWrap(wrap);
  } else {
    M5Cardputer.Display.setTextWrap(wrap);
  }
}

void CardputerGameDisplay::getTextBounds(const char* text, int16_t x, int16_t y, int16_t* x1,
                                         int16_t* y1, uint16_t* w, uint16_t* h) {
  size_t len = text ? strlen(text) : 0;
  if (x1) *x1 = x;
  if (y1) *y1 = y;
  if (w) *w = len * 6 * _textSize;
  if (h) *h = 8 * _textSize;
}

void CardputerGameDisplay::getTextBounds(const String& text, int16_t x, int16_t y, int16_t* x1,
                                         int16_t* y1, uint16_t* w, uint16_t* h) {
  getTextBounds(text.c_str(), x, y, x1, y1, w, h);
}

void CardputerGameDisplay::fillScreen(uint16_t color) {
  if (_useCanvas) {
    _canvas.fillScreen(color);
  } else {
    M5Cardputer.Display.fillRect(CARDPUTER_GAME_VIEWPORT_X, CARDPUTER_GAME_VIEWPORT_Y,
                                 SCREEN_WIDTH, SCREEN_HEIGHT, color);
  }
}

void CardputerGameDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (_useCanvas) {
    _canvas.drawPixel(x, y, color);
  } else {
    M5Cardputer.Display.drawPixel(dx(x), dy(y), color);
  }
}

void CardputerGameDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                    uint16_t color) {
  if (_useCanvas) {
    _canvas.drawLine(x0, y0, x1, y1, color);
  } else {
    M5Cardputer.Display.drawLine(dx(x0), dy(y0), dx(x1), dy(y1), color);
  }
}

void CardputerGameDisplay::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_useCanvas) {
    _canvas.drawFastHLine(x, y, w, color);
  } else {
    M5Cardputer.Display.drawFastHLine(dx(x), dy(y), w, color);
  }
}

void CardputerGameDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_useCanvas) {
    _canvas.drawFastVLine(x, y, h, color);
  } else {
    M5Cardputer.Display.drawFastVLine(dx(x), dy(y), h, color);
  }
}

void CardputerGameDisplay::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                    uint16_t color) {
  if (_useCanvas) {
    _canvas.drawRect(x, y, w, h, color);
  } else {
    M5Cardputer.Display.drawRect(dx(x), dy(y), w, h, color);
  }
}

void CardputerGameDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                    uint16_t color) {
  if (_useCanvas) {
    _canvas.fillRect(x, y, w, h, color);
  } else {
    M5Cardputer.Display.fillRect(dx(x), dy(y), w, h, color);
  }
}

void CardputerGameDisplay::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (_useCanvas) {
    _canvas.drawCircle(x, y, r, color);
  } else {
    M5Cardputer.Display.drawCircle(dx(x), dy(y), r, color);
  }
}

void CardputerGameDisplay::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (_useCanvas) {
    _canvas.fillCircle(x, y, r, color);
  } else {
    M5Cardputer.Display.fillCircle(dx(x), dy(y), r, color);
  }
}

void CardputerGameDisplay::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                        int16_t x2, int16_t y2, uint16_t color) {
  if (_useCanvas) {
    _canvas.drawTriangle(x0, y0, x1, y1, x2, y2, color);
  } else {
    M5Cardputer.Display.drawTriangle(dx(x0), dy(y0), dx(x1), dy(y1), dx(x2), dy(y2),
                                     color);
  }
}

void CardputerGameDisplay::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                         int16_t r, uint16_t color) {
  if (_useCanvas) {
    _canvas.fillRoundRect(x, y, w, h, r, color);
  } else {
    M5Cardputer.Display.fillRoundRect(dx(x), dy(y), w, h, r, color);
  }
}

void CardputerGameDisplay::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                         int16_t r, uint16_t color) {
  if (_useCanvas) {
    _canvas.drawRoundRect(x, y, w, h, r, color);
  } else {
    M5Cardputer.Display.drawRoundRect(dx(x), dy(y), w, h, r, color);
  }
}

size_t CardputerGameDisplay::write(uint8_t value) {
  return _useCanvas ? _canvas.write(value) : M5Cardputer.Display.write(value);
}

int16_t CardputerGameDisplay::dx(int16_t x) const {
  return x + CARDPUTER_GAME_VIEWPORT_X;
}

int16_t CardputerGameDisplay::dy(int16_t y) const {
  return y + CARDPUTER_GAME_VIEWPORT_Y;
}

void GamerEngine::begin(ServiceCallback serviceCallback) {
  _serviceCallback = serviceCallback;
  _display.begin();
  _sound.begin();
  resetForLaunch();
}

void GamerEngine::resetForLaunch() {
  memset(_buttons, 0, sizeof(_buttons));
  _exitRequested = false;
  _feedbackUntil = 0;
  _display.clearPhysical();
}

void GamerEngine::clearEvents() {
  for (uint8_t i = 0; i < 3; ++i) {
    _buttons[i].pressEvent = false;
    _buttons[i].releaseEvent = false;
    _buttons[i].longEvent = false;
  }
}

void GamerEngine::tick() {
  clearEvents();
  if (_serviceCallback) {
    _serviceCallback();
  } else {
    M5Cardputer.update();
  }
  pollKeyboard();
  _sound.tick();
  yield();
}

bool GamerEngine::displayReady() const {
  return _display.ready();
}

bool GamerEngine::isHeld(GamerButton button) const {
  return _buttons[button].stablePressed;
}

bool GamerEngine::wasPressed(GamerButton button) const {
  return _buttons[button].pressEvent;
}

bool GamerEngine::wasReleased(GamerButton button) const {
  return _buttons[button].releaseEvent;
}

bool GamerEngine::wasLongPressed(GamerButton button) const {
  return _buttons[button].longEvent;
}

uint16_t GamerEngine::releasedDuration(GamerButton button) const {
  return _buttons[button].releaseDuration;
}

bool GamerEngine::shouldExitGame() const {
  return _exitRequested || _buttons[BTN_SELECT].longEvent;
}

void GamerEngine::waitForRelease() {
  while (isHeld(BTN_LEFT) || isHeld(BTN_RIGHT) || isHeld(BTN_SELECT)) {
    tick();
    delay(10);
  }
}

void GamerEngine::queueInput(const GameInputEvent& event) {
  if (textHasAny(event.text, "mM")) {
    toggleSoundMuted();
  }

  if (event.back || event.del || event.home || event.tab || textHasAny(event.text, "qQ")) {
    _exitRequested = true;
  }

  if (event.up || event.left || textHasAny(event.text, "aAhHkKwW")) {
    updateButton(_buttons[BTN_LEFT], true);
  }
  if (event.down || event.right || textHasAny(event.text, "dDjJlLsS")) {
    updateButton(_buttons[BTN_RIGHT], true);
  }
  if (event.enter || event.space || event.btnA || event.text.indexOf(' ') >= 0) {
    updateButton(_buttons[BTN_SELECT], true);
  }
}

CardputerGameDisplay& GamerEngine::screen() {
  return _display;
}

uint16_t GamerEngine::width() const {
  return SCREEN_WIDTH;
}

uint16_t GamerEngine::height() const {
  return SCREEN_HEIGHT;
}

uint8_t GamerEngine::textScale() const {
  return 1;
}

String GamerEngine::powerLabel() {
  return String();
}

void GamerEngine::clear() {
  _display.fillScreen(GAMER_BLACK);
  _display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GAMER_DIM);
  _display.setTextWrap(false);
  _display.setTextSize(textScale());
  _display.setTextColor(GAMER_WHITE, GAMER_BLACK);
}

void GamerEngine::show() {
  drawFeedbackBorder();
  _display.pushFrame();
}

void GamerEngine::centerText(const char* text, int16_t y, uint8_t size, uint16_t color) {
  _display.setTextSize(size);
  _display.setTextColor(color, GAMER_BLACK);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textW = 0;
  uint16_t textH = 0;
  _display.getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);
  int16_t x = (static_cast<int16_t>(width()) - static_cast<int16_t>(textW)) / 2 - x1;
  if (x < 0) x = 0;
  _display.setCursor(x, y);
  _display.print(text);
  _display.setTextSize(textScale());
  _display.setTextColor(GAMER_WHITE, GAMER_BLACK);
}

void GamerEngine::rightText(const char* text, int16_t y, uint16_t color) {
  _display.setTextSize(textScale());
  _display.setTextColor(color, GAMER_BLACK);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textW = 0;
  uint16_t textH = 0;
  _display.getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);
  int16_t x = static_cast<int16_t>(width()) - static_cast<int16_t>(textW) - 4 - x1;
  if (x < 0) x = 0;
  _display.setCursor(x, y);
  _display.print(text);
  _display.setTextColor(GAMER_WHITE, GAMER_BLACK);
}

void GamerEngine::drawTitle(const char* title, const char* line1, const char* line2) {
  clear();
  _display.drawRect(0, 0, width(), height(), GAMER_DIM);
  const uint8_t titleSize = strlen(title ? title : "") > 10 ? 1 : 2;
  centerText(title, titleSize == 2 ? 12 : 15, titleSize, GAMER_WHITE);
  centerText(line1, 37, textScale(), GAMER_ACCENT);
  centerText(line2, 51, textScale(), GAMER_DIM);
  show();
}

bool GamerEngine::waitForSelectOrExit(const char* title, const char* line1, const char* line2) {
  waitForRelease();
  drawTitle(title, line1, line2);

  while (true) {
    tick();
    if (shouldExitGame()) {
      waitForRelease();
      ledPulse(CRGB::Red, 120);
      playSound(SOUND_UI_BACK);
      return false;
    }
    if (wasReleased(BTN_SELECT) && releasedDuration(BTN_SELECT) < BUTTON_LONGPRESS_MS) {
      ledPulse(CRGB::Green, 90);
      playSound(SOUND_UI_SELECT);
      return true;
    }
    delay(10);
  }
}

bool GamerEngine::showResult(const char* title, const char* detail) {
  waitForRelease();
  if (title && (strstr(title, "OVER") || strstr(title, "CRASH") || strstr(title, "BOOM") ||
                strstr(title, "MISSED") || strstr(title, "LOST") || strstr(title, "LOCKED") ||
                strstr(title, "SOON") || strstr(title, "TOPPLED") || strstr(title, "CPU"))) {
    playSound(SOUND_LOSE);
  } else {
    playSound(SOUND_WIN);
  }
  clear();
  const int16_t bandH = 34;
  const int16_t bandY = (height() - bandH) / 2;
  _display.fillRect(0, bandY, width(), bandH, GAMER_ACCENT);
  centerText(title, bandY + 9, 1, GAMER_BLACK);
  centerText(detail, height() - 23, 1, GAMER_WHITE);
  show();

  while (true) {
    tick();
    if (shouldExitGame()) {
      waitForRelease();
      ledPulse(CRGB::Red, 160);
      playSound(SOUND_GAME_EXIT);
      return false;
    }
    if (wasReleased(BTN_SELECT) && releasedDuration(BTN_SELECT) < BUTTON_LONGPRESS_MS) {
      ledPulse(CRGB::Green, 90);
      playSound(SOUND_UI_SELECT);
      return true;
    }
    delay(10);
  }
}

void GamerEngine::drawHeader(const char* title, int16_t value) {
  _display.setTextSize(textScale());
  _display.setTextColor(GAMER_WHITE, GAMER_BLACK);
  _display.setCursor(0, 0);
  _display.print(title);
  char valueText[10];
  snprintf(valueText, sizeof(valueText), "%d", value);
  rightText(valueText, 0);
}

void GamerEngine::ledOff() {
  _feedbackUntil = 0;
}

void GamerEngine::ledSet(const CRGB& color) {
  ledPulse(color, 120);
}

void GamerEngine::ledPulse(const CRGB& color, uint16_t durationMs) {
  _feedbackColor = toColor565(color);
  _feedbackUntil = millis() + min<uint16_t>(durationMs, 180);
}

void GamerEngine::playSound(GameSoundCue cue) {
  _sound.play(cue);
}

bool GamerEngine::toggleSoundMuted() {
  const bool muted = _sound.toggleMute();
  Serial.printf("sound muted=%s\n", muted ? "true" : "false");
  return muted;
}

bool GamerEngine::soundMuted() const {
  return _sound.muted();
}

void GamerEngine::pollKeyboard() {
  bool rawLeft = false;
  bool rawRight = false;
  bool rawSelect = M5Cardputer.BtnA.isPressed();
  bool rawExit = false;
  bool mutePressed = false;

  if (M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
    rawSelect = rawSelect || keys.enter || keys.space;
    rawExit = keys.del || keys.tab;

    for (auto c : keys.word) {
      if (c == 'a' || c == 'A' || c == 'h' || c == 'H' || c == 'k' || c == 'K' ||
          c == 'w' || c == 'W') {
        rawLeft = true;
      }
      if (c == 'd' || c == 'D' || c == 'j' || c == 'J' || c == 'l' || c == 'L' ||
          c == 's' || c == 'S') {
        rawRight = true;
      }
      if (c == 'q' || c == 'Q') {
        rawExit = true;
      }
      if (c == 'm' || c == 'M') {
        mutePressed = true;
      }
    }

    for (const auto& key : M5Cardputer.Keyboard.keyList()) {
      if (key.y == 3 && key.x == 10) rawLeft = true;
      if (key.y == 3 && key.x == 12) rawRight = true;
      if (key.y == 2 && key.x == 11) rawLeft = true;
      if (key.y == 3 && key.x == 11) rawRight = true;
    }
  }

  updateButton(_buttons[BTN_LEFT], rawLeft);
  updateButton(_buttons[BTN_RIGHT], rawRight);
  updateButton(_buttons[BTN_SELECT], rawSelect);
  if (mutePressed && !_muteKeyHeld) {
    toggleSoundMuted();
  }
  _muteKeyHeld = mutePressed;
  if (rawExit) {
    _exitRequested = true;
  }
}

void GamerEngine::updateButton(ButtonRuntime& button, bool rawPressed) {
  const uint32_t now = millis();

  if (rawPressed != button.lastRawPressed) {
    button.lastRawPressed = rawPressed;
    button.lastRawChange = now;
  }

  if ((now - button.lastRawChange) < BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (rawPressed != button.stablePressed) {
    button.stablePressed = rawPressed;
    if (button.stablePressed) {
      button.pressEvent = true;
      button.longFired = false;
      button.pressedAt = now;
      button.releaseDuration = 0;
    } else {
      const uint32_t heldFor = min<uint32_t>(now - button.pressedAt, 65535);
      button.releaseDuration = static_cast<uint16_t>(heldFor);
      button.releaseEvent = true;
    }
  }

  if (button.stablePressed && !button.longFired && (now - button.pressedAt) >= BUTTON_LONGPRESS_MS) {
    button.longFired = true;
    button.longEvent = true;
  }
}

void GamerEngine::drawFeedbackBorder() {
  if (_feedbackUntil == 0 || static_cast<int32_t>(millis() - _feedbackUntil) >= 0) {
    _feedbackUntil = 0;
    return;
  }
  _display.drawRect(0, 0, width(), height(), _feedbackColor);
  _display.drawRect(1, 1, width() - 2, height() - 2, _feedbackColor);
}

bool GamerEngine::textHasAny(const String& text, const char* chars) const {
  for (const char* c = chars; c && *c; ++c) {
    if (text.indexOf(*c) >= 0) {
      return true;
    }
  }
  return false;
}

uint16_t GamerEngine::toColor565(const CRGB& color) const {
  return ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
}
