#pragma once

#include <Arduino.h>

#include "GamerEngine.h"

class GameScreen {
public:
  void begin(GamerEngine::ServiceCallback serviceCallback = nullptr);
  void enter();
  void exit();
  bool isActive() const;
  bool isGameRunning() const;

  void drawLauncher(bool force = false);
  bool handleInput(const GameInputEvent& event);

  template <typename InputEventT>
  bool handleInput(const InputEventT& event) {
    GameInputEvent gameEvent;
    gameEvent.pressed = event.pressed;
    gameEvent.enter = event.enter;
    gameEvent.back = event.back;
    gameEvent.home = event.home;
    gameEvent.up = event.up;
    gameEvent.down = event.down;
    gameEvent.left = event.left;
    gameEvent.right = event.right;
    gameEvent.del = event.del;
    gameEvent.tab = event.tab;
    gameEvent.btnA = event.btnA;
    gameEvent.space = event.text.indexOf(' ') >= 0;
    gameEvent.text = event.text;
    return handleInput(gameEvent);
  }

  void selectPrevious();
  void selectNext();
  void launchSelected();

private:
  bool textHasAny(const String& text, const char* chars) const;

  GamerEngine _engine;
  bool _active = false;
  bool _gameRunning = false;
  bool _dirty = true;
  uint8_t _selectedGame = 0;
  uint8_t _lastRenderedGame = 255;
};
