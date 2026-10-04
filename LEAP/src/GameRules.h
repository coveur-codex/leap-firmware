#pragma once
#include <algorithm>
#include <cstdint>
namespace leap {
// Values are wellbeing, not penalties. No decay while the device is powered off.
struct PetState {
  static constexpr uint32_t DecayIntervalMs = 30000;
  uint8_t version = 1, food = 85, joy = 85, clean = 85, energy = 85;
  bool valid() const {
    return version == 1 && food >= 35 && food <= 100 && joy >= 35 && joy <= 100 && clean >= 35 &&
           clean <= 100 && energy >= 35 && energy <= 100;
  }
  void decay(unsigned ticks) {
    auto lower = [ticks](uint8_t &v, unsigned rate) {
      v = uint8_t(std::max(35, int(v) - int(std::min(ticks, 100u) * rate)));
    };
    lower(food, 2);
    lower(joy, 2);
    lower(clean, 1);
    lower(energy, 2);
  }
  void care(int action) {
    if (action == 0)
      food = 100;
    if (action == 1)
      joy = 100;
    if (action == 2)
      clean = 100;
    if (action == 3)
      energy = 100;
  }
  const char *mood() const {
    // Show the most urgent need, with a stable tie break to avoid flicker.
    uint8_t lowest = std::min(std::min(food, energy), std::min(clean, joy));
    if (lowest < 55) {
      if (food == lowest)
        return "hungry";
      if (energy == lowest)
        return "tired";
      if (clean == lowest)
        return "dirty";
      return "sad";
    }
    if (food >= 80 && joy >= 80 && clean >= 80 && energy >= 80)
      return "happy";
    return "idle";
  }
};
inline bool petNight(bool clockKnown, int hour) {
  return clockKnown && (hour < 7 || hour >= 20);
}
class SnakeState {
public:
  static constexpr int Columns = 40, Rows = 13, Capacity = Columns * Rows;
  struct Cell {
    int x = 0, y = 0;
    bool operator==(Cell b) const {
      return x == b.x && y == b.y;
    }
  };
  static constexpr int FoodSlots = 3;
  Cell body[Capacity]{}, foods[FoodSlots]{};
  int foodCount = 0;
  int length = 3, direction = 3, queued = 3, score = 0;
  bool alive = true, won = false, turned = false;
  void placeFood(uint32_t random) {
    foodCount = std::min(foodCount, Capacity - length);
    if (length == Capacity) {
      alive = false;
      won = true;
      foodCount = 0;
      return;
    }
    int target = std::min(FoodSlots, Capacity - length);
    while (foodCount < target) {
      int choice = random % (Capacity - length - foodCount);
      bool placed = false;
      for (int y = 0; y < Rows && !placed; ++y)
        for (int x = 0; x < Columns && !placed; ++x) {
          Cell cell{x, y};
          bool occupied = false;
          for (int i = 0; i < length; ++i)
            occupied |= body[i] == cell;
          for (int i = 0; i < foodCount; ++i)
            occupied |= foods[i] == cell;
          if (!occupied && choice-- == 0) {
            foods[foodCount++] = cell;
            placed = true;
          }
        }
      random = random * 1664525u + 1013904223u;
    }
  }
  void start(uint32_t random) {
    length = 3;
    score = 0;
    direction = queued = 3;
    alive = true;
    won = turned = false;
    for (int i = 0; i < length; ++i)
      body[i] = {Columns / 2 - i, Rows / 2};
    foodCount = 0;
    placeFood(random);
  }
  void turn(int next) {
    // Same order as Key: up, down, left, right. One buffered turn per move.
    if (!alive || turned || next < 0 || next > 3 || next == direction || next == (direction ^ 1))
      return;
    queued = next;
    turned = true;
  }
  bool move(uint32_t random) {
    if (!alive)
      return false;
    direction = queued;
    turned = false;
    Cell head = body[0];
    head.x += (direction == 3) - (direction == 2);
    head.y += (direction == 1) - (direction == 0);
    int eaten = -1;
    for (int i = 0; i < foodCount; ++i)
      if (head == foods[i]) eaten = i;
    bool eat = eaten >= 0;
    if (head.x < 0 || head.x >= Columns || head.y < 0 || head.y >= Rows) {
      alive = false;
      return false;
    }
    // Moving into the departing tail is legal when no food is eaten.
    for (int i = 0; i < length - (eat ? 0 : 1); ++i)
      if (head == body[i]) {
        alive = false;
        return false;
      }
    if (eat) {
      ++length;
      ++score;
      foods[eaten] = foods[--foodCount];
    }
    for (int i = length - 1; i > 0; --i)
      body[i] = body[i - 1];
    body[0] = head;
    if (eat)
      placeFood(random);
    return eat;
  }
};
} // namespace leap
