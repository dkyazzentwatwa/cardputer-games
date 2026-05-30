#pragma once

#include <Arduino.h>

enum GameSoundCue : uint8_t {
  SOUND_UI_MOVE = 0,
  SOUND_UI_SELECT,
  SOUND_UI_BACK,
  SOUND_GAME_START,
  SOUND_GAME_EXIT,
  SOUND_ACTION,
  SOUND_JUMP,
  SOUND_THRUST,
  SOUND_SHOOT,
  SOUND_HIT,
  SOUND_SCORE,
  SOUND_WIN,
  SOUND_LOSE,
  SOUND_ERROR,
  SOUND_TIMER_GO,
  SOUND_SIMON_LEFT,
  SOUND_SIMON_RIGHT,
  SOUND_SIMON_SELECT,
  SOUND_MUTE_ON,
  SOUND_MUTE_OFF,
  SOUND_POWERUP,
  SOUND_LEVELUP,
  SOUND_COMBO
};

class GamerSound {
public:
  struct Note {
    uint16_t frequency;
    uint16_t durationMs;
    uint8_t gapMs;
  };

  struct Cue {
    const Note* notes;
    uint8_t count;
    uint8_t channel;
  };

  void begin();
  void tick();
  void play(GameSoundCue cue);
  bool toggleMute();
  void setMuted(bool muted);
  bool muted() const;

private:
  struct ChannelState {
    const Note* notes = nullptr;
    uint8_t count = 0;
    uint8_t index = 0;
    uint8_t channel = 0;
    uint32_t nextAt = 0;
  };

  void playNow(ChannelState& state);
  const Cue& cueFor(GameSoundCue cue) const;

  ChannelState _channels[3];
  bool _muted = false;
  bool _begun = false;
};
