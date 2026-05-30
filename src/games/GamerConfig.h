#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#define GAMER_DISPLAY_IS_SSD1306 0
#define GAMER_DISPLAY_IS_SH8601 0
#define GAMER_HAS_TOUCH 0
#define GAMER_HAS_BOOT_BUTTON 0

constexpr const char* GAMER_BOARD_NAME = "cardputer-adv";
constexpr uint16_t SCREEN_WIDTH = 128;
constexpr uint16_t SCREEN_HEIGHT = 64;
constexpr uint16_t CARDPUTER_GAME_DISPLAY_WIDTH = 240;
constexpr uint16_t CARDPUTER_GAME_DISPLAY_HEIGHT = 135;
constexpr uint16_t CARDPUTER_GAME_VIEWPORT_WIDTH = 240;
constexpr uint16_t CARDPUTER_GAME_VIEWPORT_HEIGHT = 120;
constexpr int16_t CARDPUTER_GAME_VIEWPORT_X = 0;
constexpr int16_t CARDPUTER_GAME_VIEWPORT_Y = 7;
constexpr float CARDPUTER_GAME_SCALE = 240.0f / 128.0f;
constexpr uint16_t BUTTON_DEBOUNCE_MS = 35;
constexpr uint16_t BUTTON_LONGPRESS_MS = 850;

constexpr uint16_t GAMER_BLACK = TFT_BLACK;
constexpr uint16_t GAMER_WHITE = TFT_WHITE;
constexpr uint16_t GAMER_ACCENT = TFT_CYAN;
constexpr uint16_t GAMER_DIM = TFT_DARKGREY;
constexpr uint16_t GAMER_INVERSE = TFT_ORANGE;

struct CRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  constexpr CRGB(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
      : r(red), g(green), b(blue) {}

  static const CRGB Black;
  static const CRGB Red;
  static const CRGB Green;
  static const CRGB Blue;
  static const CRGB Purple;
  static const CRGB Aqua;
  static const CRGB Orange;
  static const CRGB Cyan;
  static const CRGB Magenta;
  static const CRGB Yellow;
  static const CRGB White;
};
