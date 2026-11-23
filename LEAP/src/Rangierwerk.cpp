#include "Rangierwerk.h"
#include "Audio.h"
#include <algorithm>

namespace leap {
using namespace rangierwerk;

void Rangierwerk::open() {
  screen = Screen::Setup;
  onlineMode = false;
  field = mode = pause = held = 0;
  level = duration = 1;
  waitRelease = true;
}
void Rangierwerk::close() {
  if (onlineMode)
    rangierwerkMultiplayer.close();
  onlineMode = false;
  screen = Screen::Setup;
  held = 0;
}
void Rangierwerk::enterTrack() {
  screen = Screen::Track;
  animation.fill(MotionFrame{});
  openedAt = millis();
  deliveryAt = 0;
  lastDelivered = delivered();
  lastSwitches = onlineMode ? remote.switches : yard.thrown;
  switched = 0;
  const auto depot = yard.layout.depots[self()].turnout;
  turnout = yard.layout.nodes[depot].turnout;
  order = 0;
  waitRelease = true;
}
void Rangierwerk::startLocal(uint32_t now) {
  onlineMode = false;
  if (!yard.reset(esp_random(), level, false) || !yard.join(0))
    return;
  local.start(duration, now);
  movedAt = now - StepMs;
  enterTrack();
}
void Rangierwerk::startOnline() {
  onlineMode = true;
  screen = Screen::Track;
  remote = RangierFrame{};
  card = 0;
  waitRelease = true;
  if (!rangierwerkMultiplayer.start())
    remote.phase = RangierPhase::Error;
}
bool Rangierwerk::playing() const { return !onlineMode || remote.phase == RangierPhase::Playing; }
bool Rangierwerk::summary() const { return onlineMode ? remote.summary : local.finished; }

void Rangierwerk::directions(uint8_t mask) {
  const bool up = mask & (1u << int(Key::Up)), down = mask & (1u << int(Key::Down));
  held = int(up) - int(down);
  if (!up && !down)
    waitRelease = false;
}

Consist Rangierwerk::consist() const { return inspectConsist(vehicles(), self()); }

void Rangierwerk::discrete(RangierAction type, uint32_t a, uint32_t b) {
  if (onlineMode) {
    rangierwerkMultiplayer.action(type, card, a, b);
  } else if (type == RangierAction::Turnout) {
    if (yard.toggle(static_cast<Index>(a)))
      audio.tone(420, 35);
  } else if (type == RangierAction::Split) {
    if (yard.split(0, a, b))
      audio.tone(480, 45);
  } else if (yard.recover(0)) {
    audio.tone(480, 60);
  }
}

bool Rangierwerk::input(const InputEvent &event) {
  const auto key = event.key;
  const int vertical = int(key == Key::Down) - int(key == Key::Up);
  const int horizontal = int(key == Key::Right) - int(key == Key::Left);
  if (event.longPress) {
    if (!event.right && key == Key::Center && event.heldMs < Input::ExtendedCenterHoldMs) {
      if (screen == Screen::Setup || !playing())
        return true;
      if (screen != Screen::Pause && !summary()) {
        screen = Screen::Pause;
        pause = 0;
        waitRelease = true;
        if (onlineMode) rangierwerkMultiplayer.throttle(0);
      }
    }
    return false;
  }
  if (screen == Screen::Setup) {
    if (!event.right && key == Key::Center)
      return true;
    if (!event.right)
      return false;
    field = std::clamp(field + vertical, 0, mode ? 0 : 2);
    if (field == 0) mode = std::clamp(mode + horizontal, 0, 1);
    if (field == 1) level = std::clamp(level + horizontal, 1, 4);
    if (field == 2) duration = std::clamp(duration + horizontal, 0, 4);
    if (key == Key::Center) {
      if (mode) startOnline();
      else startLocal(millis());
    }
    return false;
  }

  if (screen == Screen::Players) {
    if ((!event.right && key == Key::Center) || (event.right && (key == Key::Down || key == Key::Center)))
      screen = Screen::Pause;
    return false;
  }
  if (screen == Screen::Pause) {
    if (!event.right && key == Key::Center) {
      screen = Screen::Track;
      waitRelease = true;
    } else if (event.right) {
      pause = std::clamp(pause + vertical, 0, 3);
      if (key == Key::Center) {
        if (pause == 3)
          return true;
        if (onlineMode && pause == 2) {
          screen = Screen::Players;
        } else if (!onlineMode && pause == 1) {
          startLocal(millis());
        } else {
          if (pause == (onlineMode ? 1 : 2))
            discrete(RangierAction::Recover);
          screen = Screen::Track;
          waitRelease = true;
        }
      }
    }
    return false;
  }

  if (!playing() || summary() || (onlineMode && !rangierwerkMultiplayer.online)) {
    if (!event.right && key == Key::Center) {
      if (onlineMode) rangierwerkMultiplayer.close();
      open();
    } else if (!onlineMode && event.right && key == Key::Center && summary()) {
      startLocal(millis());
    }
    return false;
  }

  if (!event.right) {
    if (horizontal)
      turnout = (turnout + horizontal + yard.layout.switchCount) % yard.layout.switchCount;
    if (vertical) {
      int count = 0;
      for (const auto &o : orders())
        if (o.open()) ++count;
      if (count)
        order = (order % count + vertical + count) % count;
    }
    if (key == Key::Center && !event.repeated)
      discrete(RangierAction::Turnout, turnout);
    return false;
  }

  if (screen == Screen::Couplings) {
    const auto train = consist();
    cut = std::clamp(cut + horizontal, 0, std::max(0, int(train.size) - 2));
    if (key == Key::Center && train.size >= 2) {
      const auto &fleet = vehicles();
      discrete(RangierAction::Split, fleet[train.vehicles[cut]].id, fleet[train.vehicles[cut + 1]].id);
    }
    if (key == Key::Center || key == Key::Down) {
      screen = Screen::Track;
      waitRelease = true;
    }
  } else if (key == Key::Center && !held) {
    screen = Screen::Couplings;
    cut = 0;
    if (onlineMode) rangierwerkMultiplayer.throttle(0);
  }
  return false;
}

void Rangierwerk::animate(uint32_t now) {
  for (Index i = 0; i < MaxVehicles; ++i) {
    const auto &v = vehicles()[i];
    auto &sprite = animation[i];
    if (!v.id) {
      sprite = MotionFrame{};
      continue;
    }
    if (sprite.id != v.id) {
      sprite = {v.id, now, now, now - 500, v.node, v.node, v.links[0], v.links[1]};
    } else {
      if (v.node != sprite.at) {
        sprite.from = yard.layout.adjacent(sprite.at, v.node) ? sprite.at : v.node;
        sprite.at = v.node;
        sprite.movedAt = now;
      }
      if (sprite.a != v.links[0] || sprite.b != v.links[1]) {
        sprite.a = v.links[0]; sprite.b = v.links[1]; sprite.linkAt = now;
      }
    }
  }
  const uint32_t switches = onlineMode ? remote.switches : yard.thrown;
  if (switches != lastSwitches) {
    switched = switches ^ lastSwitches;
    switchedAt = now;
    lastSwitches = switches;
  }
  if (delivered() > lastDelivered) {
    deliveryAt = now;
    audio.tone(780, 90);
  }
  lastDelivered = delivered();
}

void Rangierwerk::tick(uint32_t now) {
  if (screen == Screen::Setup)
    return;
  if (onlineMode) {
    if (rangierwerkMultiplayer.poll(remote) && playing()) {
      if (remote.card != card) {
        if (!yard.layout.generate(remote.seed, remote.difficulty, true)) {
          remote.phase = RangierPhase::Error;
          rangierwerkMultiplayer.close();
          return;
        }
        card = remote.card;
        enterTrack();
      }
    }
    if (!rangierwerkMultiplayer.online)
      waitRelease = true;
    const bool drive = playing() && !summary() && screen == Screen::Track && !waitRelease;
    rangierwerkMultiplayer.throttle(drive ? held : 0);
    if (playing()) animate(now);
    return;
  }
  local.advance(yard, now, screen == Screen::Pause);
  if (!summary() && screen == Screen::Track && held && !waitRelease && elapsed(now, movedAt, StepMs)) {
    movedAt = now;
    if (yard.drive(0, held) == Move::Coupled)
      audio.tone(550, 45);
  }
  animate(now);
}
} // namespace leap
