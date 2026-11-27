#pragma once
#include <array>
#include <cstdint>

namespace leap {
constexpr uint8_t PondLanes = 4, PondGoal = 12, PondNone = 255;
constexpr uint32_t PondSettleMs = 1200, PondTurnMs = 20000;
enum class PondPhase : uint8_t { Connecting, Lobby, Ready, Rolling, Settling, Paused, Finished, Error };
enum class PondAction : uint8_t { Start, Roll, Stop };
using PondPositions = std::array<uint8_t, PondLanes>;
using PondDice = std::array<uint8_t, 2>;

inline constexpr const char *PondNames[] = {"Mohn", "Klee", "Iris", "Mango"};
inline constexpr const char *PondColors[] = {"#ed786e", "#6bc582", "#9e92ee", "#ffbd61"};

struct PondFrog {
  char name[33]{};
  uint16_t color = 0;
  bool human = false, connected = false;
};

struct PondFrame {
  PondPhase phase = PondPhase::Connecting;
  PondPositions positions{}, previous{};
  PondDice dice{PondNone, PondNone};
  PondFrog frogs[PondLanes];
  uint8_t self = PondNone, host = PondNone, turn = PondNone, winners = 0;
  uint32_t epoch = 0, turnId = 0, revision = 0, move = 0, round = 0, remainingMs = 0, ack = 0;
  char session[33]{}, reason[32]{};
};

inline void advancePondFrogs(PondPositions &positions, PondDice dice, uint8_t &winners) {
  for (uint8_t lane : dice) {
    if (positions[lane] < PondGoal)
      ++positions[lane];
    if (positions[lane] == PondGoal)
      winners |= 1 << lane;
  }
}

class LocalPondRace {
  static constexpr uint8_t TurnSeats = 3;
  uint8_t turnsInRound = 0;
  uint32_t phaseAt = 0;
  PondDice pending{};

  void reveal(uint32_t now) {
    frame.dice = pending;
    frame.previous = frame.positions;
    advancePondFrogs(frame.positions, frame.dice, frame.winners);
    ++frame.move;
    ++turnsInRound;
    frame.phase = PondPhase::Settling;
    frame.remainingMs = PondSettleMs;
    phaseAt = now;
  }

public:
  PondFrame frame;

  void start(uint32_t now, uint8_t first) {
    frame = PondFrame{};
    frame.phase = PondPhase::Ready;
    frame.self = 0;
    frame.turn = first % TurnSeats;
    frame.round = frame.turnId = 1;
    turnsInRound = 0;
    phaseAt = now;
  }
  void roll(uint32_t now, uint32_t random) {
    pending = {uint8_t(random & 3), uint8_t((random >> 2) & 3)};
    frame.phase = PondPhase::Rolling;
    phaseAt = now;
  }
  void press(uint32_t now, uint32_t random) {
    if (frame.turn != frame.self)
      return;
    if (frame.phase == PondPhase::Ready)
      roll(now, random);
    else if (frame.phase == PondPhase::Rolling)
      reveal(now);
  }
  bool botReady(uint32_t now) const {
    return frame.phase == PondPhase::Ready && frame.turn != frame.self && uint32_t(now - phaseAt) >= 570;
  }
  void tick(uint32_t now) {
    const uint32_t age = now - phaseAt;
    if (frame.phase == PondPhase::Rolling && age >= (frame.turn == frame.self ? PondTurnMs : 430))
      reveal(now);
    else if (frame.phase == PondPhase::Settling) {
      if (age < PondSettleMs) {
        frame.remainingMs = PondSettleMs - age;
        return;
      }
      frame.remainingMs = 0;
      if (turnsInRound == TurnSeats) {
        if (frame.winners) {
          frame.phase = PondPhase::Finished;
          frame.turn = PondNone;
          return;
        }
        turnsInRound = 0;
        ++frame.round;
      }
      frame.phase = PondPhase::Ready;
      frame.turn = (frame.turn + 1) % TurnSeats;
      ++frame.turnId;
      phaseAt = now;
    }
  }
};
} // namespace leap
