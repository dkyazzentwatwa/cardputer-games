#include "GameScreen.h"

#include <string.h>

#include "Games.h"

namespace {
constexpr uint16_t kPanel = 0x0841;
constexpr uint16_t kPanelAlt = 0x10A2;
constexpr uint16_t kBorder = 0x31A6;
constexpr uint16_t kMuted = 0x9CF3;

uint8_t clippedLength(const char* text, uint8_t maxChars) {
  const size_t rawLen = text ? strlen(text) : 0;
  const uint8_t len = rawLen > 255 ? 255 : static_cast<uint8_t>(rawLen);
  return len > maxChars && maxChars > 3 ? maxChars : len;
}

template <typename GfxT>
void printClipped(GfxT& gfx, const char* text, uint8_t maxChars) {
  if (!text || maxChars == 0) {
    return;
  }
  const size_t rawLen = strlen(text);
  const uint8_t len = rawLen > 255 ? 255 : static_cast<uint8_t>(rawLen);
  if (len <= maxChars || maxChars <= 3) {
    for (uint8_t i = 0; i < min<uint8_t>(len, maxChars); ++i) {
      gfx.print(text[i]);
    }
    return;
  }
  for (uint8_t i = 0; i < maxChars - 3; ++i) {
    gfx.print(text[i]);
  }
  gfx.print("...");
}
}

void GameScreen::begin(GamerEngine::ServiceCallback serviceCallback) {
  _engine.begin(serviceCallback);
  randomSeed(micros());
}

void GameScreen::enter() {
  _active = true;
  _gameRunning = false;
  _dirty = true;
  _engine.resetForLaunch();
  drawLauncher(true);
}

void GameScreen::exit() {
  _active = false;
  _gameRunning = false;
  _dirty = true;
  _engine.resetForLaunch();
}

bool GameScreen::isActive() const {
  return _active;
}

bool GameScreen::isGameRunning() const {
  return _gameRunning;
}

bool GameScreen::handleInput(const GameInputEvent& event) {
  if (!_active || !event.pressed) {
    return false;
  }

  _engine.queueInput(event);
  if (textHasAny(event.text, "mM")) {
    _dirty = true;
    drawLauncher(true);
    return false;
  }
  if (event.back || event.del || event.home || event.tab || textHasAny(event.text, "qQ")) {
    _engine.playSound(SOUND_UI_BACK);
    return true;
  }

  if (event.left || event.up || textHasAny(event.text, "aAhHkKwW")) {
    selectPrevious();
    return false;
  }
  if (event.right || event.down || textHasAny(event.text, "dDjJlLsS")) {
    selectNext();
    return false;
  }
  if (event.enter || event.space || event.btnA || event.text.indexOf(' ') >= 0) {
    launchSelected();
    return false;
  }

  drawLauncher(false);
  return false;
}

void GameScreen::selectPrevious() {
  if (GAME_COUNT == 0) {
    return;
  }
  _selectedGame = (_selectedGame == 0) ? GAME_COUNT - 1 : _selectedGame - 1;
  _dirty = true;
  _engine.ledPulse(CRGB::Blue, 55);
  _engine.playSound(SOUND_UI_MOVE);
  drawLauncher(false);
}

void GameScreen::selectNext() {
  if (GAME_COUNT == 0) {
    return;
  }
  _selectedGame = (_selectedGame + 1) % GAME_COUNT;
  _dirty = true;
  _engine.ledPulse(CRGB::Blue, 55);
  _engine.playSound(SOUND_UI_MOVE);
  drawLauncher(false);
}

void GameScreen::launchSelected() {
  if (_gameRunning || GAME_COUNT == 0 || _selectedGame >= GAME_COUNT) {
    return;
  }

  _gameRunning = true;
  _engine.resetForLaunch();
  _engine.ledPulse(CRGB::Green, 90);
  _engine.playSound(SOUND_GAME_START);
  Serial.printf("game launch index=%u title=\"%s\" category=\"%s\"\n",
                static_cast<unsigned>(_selectedGame + 1), GAME_LIBRARY[_selectedGame].title,
                GAME_LIBRARY[_selectedGame].category);
  GAME_LIBRARY[_selectedGame].run(_engine);
  Serial.printf("game returned index=%u title=\"%s\" heap=%u\n",
                static_cast<unsigned>(_selectedGame + 1), GAME_LIBRARY[_selectedGame].title,
                ESP.getFreeHeap());
  _engine.waitForRelease();
  _engine.resetForLaunch();
  _engine.playSound(SOUND_GAME_EXIT);
  _gameRunning = false;
  _dirty = true;
  drawLauncher(true);
}

void GameScreen::drawLauncher(bool force) {
  if (!_active || !_engine.displayReady()) {
    return;
  }
  if (!force && !_dirty && _lastRenderedGame == _selectedGame) {
    return;
  }

  auto& gfx = M5Cardputer.Display;
  gfx.fillScreen(GAMER_BLACK);
  gfx.setTextDatum(top_left);
  gfx.setTextWrap(false);

  const int16_t width = CARDPUTER_GAME_DISPLAY_WIDTH;
  const int16_t height = CARDPUTER_GAME_DISPLAY_HEIGHT;
  const int16_t margin = 8;
  const int16_t headerH = 20;
  const int16_t footerH = 19;
  const int16_t rowH = 14;

  gfx.fillRect(0, 0, width, headerH, TFT_NAVY);
  gfx.setTextSize(1);
  gfx.setTextColor(GAMER_WHITE, TFT_NAVY);
  gfx.setCursor(margin, 6);
  gfx.print("CARDPUTER GAMES");

  char countText[12];
  snprintf(countText, sizeof(countText), "%u/%u", static_cast<unsigned>(_selectedGame + 1),
           static_cast<unsigned>(GAME_COUNT));
  gfx.setTextColor(GAMER_WHITE, TFT_NAVY);
  gfx.setCursor(width - margin - strlen(countText) * 6, 6);
  gfx.print(countText);
  gfx.drawFastHLine(0, headerH - 1, width, GAMER_ACCENT);

  const int visibleRows = max(4, (height - headerH - footerH - 5) / rowH);
  int first = _selectedGame >= visibleRows ? _selectedGame - visibleRows + 1 : 0;
  if (first + visibleRows > GAME_COUNT) {
    first = GAME_COUNT > visibleRows ? GAME_COUNT - visibleRows : 0;
  }

  for (int row = 0; row < visibleRows; ++row) {
    const int index = first + row;
    if (index >= GAME_COUNT) {
      break;
    }
    const int16_t y = headerH + 4 + row * rowH;
    const bool selected = index == _selectedGame;
    const uint16_t bg = selected ? GAMER_ACCENT : ((row % 2 == 0) ? kPanel : kPanelAlt);
    const uint16_t fg = selected ? GAMER_BLACK : GAMER_WHITE;
    const uint16_t cat = selected ? GAMER_BLACK : kMuted;

    gfx.fillRoundRect(5, y - 2, width - 10, rowH - 2, 3, bg);
    gfx.drawRoundRect(5, y - 2, width - 10, rowH - 2, 3, selected ? GAMER_WHITE : kBorder);
    gfx.setTextColor(fg, bg);
    gfx.setCursor(margin + 4, y + 1);
    printClipped(gfx, GAME_LIBRARY[index].title, 22);
    gfx.setTextColor(cat, bg);
    const uint8_t categoryChars = clippedLength(GAME_LIBRARY[index].category, 11);
    gfx.setCursor(width - margin - 4 - categoryChars * 6, y + 1);
    printClipped(gfx, GAME_LIBRARY[index].category, 11);
  }

  const int16_t footerY = height - footerH;
  gfx.fillRect(0, footerY, width, footerH, GAMER_BLACK);
  gfx.drawFastHLine(0, footerY, width, kBorder);
  gfx.setTextColor(kMuted, GAMER_BLACK);
  gfx.setCursor(margin, footerY + 5);
  gfx.print(_engine.soundMuted() ? "WASD Ent M sound Del/Q" : "WASD Ent M mute Del/Q");

  _dirty = false;
  _lastRenderedGame = _selectedGame;
}

bool GameScreen::textHasAny(const String& text, const char* chars) const {
  for (const char* c = chars; c && *c; ++c) {
    if (text.indexOf(*c) >= 0) {
      return true;
    }
  }
  return false;
}
