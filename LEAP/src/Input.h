#pragma once
#include "Core.h"
#include "Hardware.h"
#include <Arduino.h>
namespace leap {
enum class Key { Up, Down, Left, Right, Center };
struct InputEvent {
  bool right;
  Key key;
  bool longPress;
};
class Input {
  Debouncer keys[10];
  int pins[10];

public:
  void begin() {
    for (int i = 0; i < 5; i++) {
      pins[i] = hw::LeftKeys[i];
      pins[i + 5] = hw::RightKeys[i];
    }
    for (int p : pins)
      pinMode(p, INPUT_PULLUP);
  }
  bool poll(InputEvent &e) {
    for (int i = 0; i < 10; i++) {
      int result = keys[i].poll(digitalRead(pins[i]) == LOW, millis(), i % 5 != 4);
      if (result) {
        e = {i >= 5, Key(i % 5), result == 3};
        return true;
      }
    }
    return false;
  }
};
} // namespace leap
