#include "PondPost.h"
#include "AccentColor.h"
#include "Audio.h"
#include <cstring>

namespace leap {
void PondPost::open() {
  menu = true;
  chooseOnline = onlineMode = false;
  rollSent = stopSent = false;
  brakeStart = 1;
}

void PondPost::close() {
  if (onlineMode)
    pondMultiplayer.close();
  open();
}

void PondPost::startSolo(uint32_t now) {
  solo.start(now, esp_random() % 3);
  for (uint8_t i = 0; i < PondLanes; ++i) {
    auto &frog = solo.frame.frogs[i];
    std::strcpy(frog.name, i == 0 ? "Du" : PondNames[i]);
    frog.human = i == 0;
    frog.connected = i < 3;
    frog.color = accentColor(PondColors[i]);
  }
}

PondPhase PondPost::phase() const {
  if (!onlineMode)
    return solo.frame.phase;
  if (remote.phase != PondPhase::Connecting && remote.phase != PondPhase::Error &&
      remote.phase != PondPhase::Finished && !pondMultiplayer.online)
    return PondPhase::Paused;
  return remote.phase == PondPhase::Ready && rollSent ? PondPhase::Rolling : remote.phase;
}

void PondPost::input(Key key, uint32_t now) {
  if (menu) {
    if (key == Key::Up || key == Key::Left)
      chooseOnline = false;
    else if (key == Key::Down || key == Key::Right)
      chooseOnline = true;
    else if (key == Key::Center) {
      menu = false;
      onlineMode = chooseOnline;
      if (!onlineMode) {
        startSolo(now);
      } else {
        remote = PondFrame{};
        if (!pondMultiplayer.start())
          remote.phase = PondPhase::Error;
      }
    }
    return;
  }
  if (key != Key::Center)
    return;
  const auto &frame = state();
  switch (phase()) {
  case PondPhase::Error:
  case PondPhase::Finished:
    close();
    return;
  case PondPhase::Lobby:
    if (frame.self == frame.host)
      pondMultiplayer.action(PondAction::Start, frame.turnId);
    return;
  case PondPhase::Ready:
    if (frame.turn == frame.self) {
      if (onlineMode)
        rollSent = pondMultiplayer.action(PondAction::Roll, frame.turnId);
      else
        solo.press(now, esp_random());
      rollingAt = now;
    }
    return;
  case PondPhase::Rolling:
    if (frame.turn == frame.self) {
      if (onlineMode) {
        if (!stopSent)
          stopSent = pondMultiplayer.action(PondAction::Stop, frame.turnId);
      } else {
        brakeStart = 1 + (now - rollingAt) / 4;
        solo.press(now, 0);
        audio.tone(630, 60);
      }
    }
    return;
  default:
    return;
  }
}

void PondPost::tick(uint32_t now) {
  if (menu)
    return;
  if (onlineMode) {
    PondFrame next;
    if (pondMultiplayer.poll(next)) {
      if (next.phase == PondPhase::Rolling && phase() != PondPhase::Rolling)
        rollingAt = now;
      if (next.phase == PondPhase::Settling && next.move > remote.move) {
        brakeStart = phase() == PondPhase::Rolling ? 1 + (now - rollingAt) / 4 : 1;
        audio.tone(630, 60);
      }
      if (next.phase == PondPhase::Finished && next.winners && remote.phase != PondPhase::Finished)
        audio.tone(990, 160);
      if (next.phase == PondPhase::Finished || next.phase == PondPhase::Error)
        pondMultiplayer.close();
      if (next.turnId != remote.turnId || next.phase != PondPhase::Rolling)
        rollSent = stopSent = false;
      remote = next;
      receivedAt = now;
    }
    if (!pondMultiplayer.online)
      rollSent = stopSent = false;
    return;
  }
  const PondPhase before = solo.frame.phase;
  if (solo.botReady(now)) {
    rollingAt = now;
    solo.roll(now, esp_random());
  }
  solo.tick(now);
  if (solo.frame.phase != before) {
    if (solo.frame.phase == PondPhase::Settling) {
      brakeStart = 1 + (now - rollingAt) / 4;
      audio.tone(630, 60);
    }
    else if (solo.frame.phase == PondPhase::Finished)
      audio.tone(990, 160);
  }
}
} // namespace leap
