#include "GamerSound.h"

#include <M5Cardputer.h>

namespace {
constexpr uint8_t kMasterVolume = 56;
constexpr uint8_t kUiChannel = 0;
constexpr uint8_t kActionChannel = 1;
constexpr uint8_t kResultChannel = 2;

const GamerSound::Note kUiMove[] = {{1760, 18, 0}};
const GamerSound::Note kUiSelect[] = {{1320, 24, 8}, {1760, 30, 0}};
const GamerSound::Note kUiBack[] = {{740, 35, 8}, {440, 45, 0}};
const GamerSound::Note kGameStart[] = {{880, 35, 8}, {1175, 35, 8}, {1568, 55, 0}};
const GamerSound::Note kGameExit[] = {{1047, 35, 8}, {784, 45, 0}};
const GamerSound::Note kAction[] = {{988, 22, 0}};
const GamerSound::Note kJump[] = {{740, 24, 6}, {1175, 34, 0}};
const GamerSound::Note kThrust[] = {{220, 30, 0}};
const GamerSound::Note kShoot[] = {{1976, 20, 5}, {1568, 20, 0}};
const GamerSound::Note kHit[] = {{523, 28, 6}, {392, 42, 0}};
const GamerSound::Note kScore[] = {{1175, 26, 6}, {1568, 34, 0}};
const GamerSound::Note kWin[] = {{784, 45, 8}, {1047, 45, 8}, {1568, 75, 0}};
const GamerSound::Note kLose[] = {{392, 55, 8}, {294, 70, 0}};
const GamerSound::Note kError[] = {{196, 45, 8}, {147, 45, 0}};
const GamerSound::Note kTimerGo[] = {{1568, 50, 6}, {2093, 70, 0}};
const GamerSound::Note kSimonLeft[] = {{659, 150, 0}};
const GamerSound::Note kSimonRight[] = {{880, 150, 0}};
const GamerSound::Note kSimonSelect[] = {{1175, 150, 0}};
const GamerSound::Note kMuteOn[] = {{330, 25, 5}, {220, 35, 0}};
const GamerSound::Note kMuteOff[] = {{220, 25, 5}, {440, 35, 0}};
const GamerSound::Note kPowerup[] = {{1047, 26, 4}, {1319, 26, 4}, {1568, 38, 0}};
const GamerSound::Note kLevelup[] = {{784, 30, 6}, {988, 30, 6}, {1319, 30, 6}, {1976, 60, 0}};
const GamerSound::Note kCombo[] = {{1319, 18, 4}, {1760, 26, 0}};

const GamerSound::Cue kCues[] = {
    {kUiMove, 1, kUiChannel},
    {kUiSelect, 2, kUiChannel},
    {kUiBack, 2, kUiChannel},
    {kGameStart, 3, kResultChannel},
    {kGameExit, 2, kResultChannel},
    {kAction, 1, kActionChannel},
    {kJump, 2, kActionChannel},
    {kThrust, 1, kActionChannel},
    {kShoot, 2, kActionChannel},
    {kHit, 2, kActionChannel},
    {kScore, 2, kActionChannel},
    {kWin, 3, kResultChannel},
    {kLose, 2, kResultChannel},
    {kError, 2, kActionChannel},
    {kTimerGo, 2, kResultChannel},
    {kSimonLeft, 1, kActionChannel},
    {kSimonRight, 1, kActionChannel},
    {kSimonSelect, 1, kActionChannel},
    {kMuteOn, 2, kUiChannel},
    {kMuteOff, 2, kUiChannel},
    {kPowerup, 3, kActionChannel},
    {kLevelup, 4, kResultChannel},
    {kCombo, 2, kActionChannel},
};
}

void GamerSound::begin() {
  if (_begun) {
    return;
  }
  M5Cardputer.Speaker.setVolume(kMasterVolume);
  M5Cardputer.Speaker.setAllChannelVolume(220);
  _begun = true;
}

void GamerSound::tick() {
  if (_muted) {
    return;
  }
  for (uint8_t i = 0; i < 3; ++i) {
    ChannelState& state = _channels[i];
    if (!state.notes || state.index >= state.count) {
      continue;
    }
    if (static_cast<int32_t>(millis() - state.nextAt) >= 0) {
      playNow(state);
    }
  }
}

void GamerSound::play(GameSoundCue cue) {
  begin();
  const Cue& selected = cueFor(cue);
  if (_muted && cue != SOUND_MUTE_OFF) {
    return;
  }
  ChannelState& state = _channels[selected.channel];
  state.notes = selected.notes;
  state.count = selected.count;
  state.index = 0;
  state.channel = selected.channel;
  state.nextAt = millis();
  playNow(state);
}

bool GamerSound::toggleMute() {
  setMuted(!_muted);
  return _muted;
}

void GamerSound::setMuted(bool muted) {
  if (_muted == muted) {
    return;
  }
  _muted = muted;
  if (_muted) {
    M5Cardputer.Speaker.stop();
    M5Cardputer.Speaker.tone(220, 35, kUiChannel, true);
    for (uint8_t i = 0; i < 3; ++i) {
      _channels[i] = ChannelState();
    }
  } else {
    play(SOUND_MUTE_OFF);
  }
}

bool GamerSound::muted() const {
  return _muted;
}

void GamerSound::playNow(ChannelState& state) {
  if (!state.notes || state.index >= state.count) {
    state.notes = nullptr;
    return;
  }

  const Note& note = state.notes[state.index++];
  M5Cardputer.Speaker.tone(note.frequency, note.durationMs, state.channel, true);
  state.nextAt = millis() + note.durationMs + note.gapMs;
  if (state.index >= state.count) {
    state.notes = nullptr;
  }
}

const GamerSound::Cue& GamerSound::cueFor(GameSoundCue cue) const {
  const uint8_t index = static_cast<uint8_t>(cue);
  if (index >= (sizeof(kCues) / sizeof(kCues[0]))) {
    return kCues[SOUND_ERROR];
  }
  return kCues[index];
}
