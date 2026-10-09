#pragma once
#include "Core.h"
#include "Hardware.h"
#include <Arduino.h>
#include <atomic>
#include <freertos/queue.h>
#include <freertos/task.h>
namespace leap {
enum class Key { Up, Down, Left, Right, Center };
struct InputEvent {
  bool right;
  Key key;
  bool longPress;
  uint32_t heldMs = 0; // Zero for a short press or direction repeat.
  uint32_t atMs = 0;
  bool timestamped = false;
};
class Input {
  Debouncer keys[10];
  std::atomic<uint8_t> rightDirections{0};
  void publishDirections() {
    uint8_t mask = 0;
    for (int i = 0; i < 4; ++i)
      if (keys[i + 5].stable)
        mask |= uint8_t(1u << i);
    rightDirections.store(mask);
  }
  int pins[10];
  QueueHandle_t events = nullptr;
  static void sampleTask(void *context) {
    auto &input = *static_cast<Input *>(context);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
      uint32_t now = millis();
      for (int i = 0; i < 10; i++) {
        bool center = i % 5 == 4;
        int result = input.keys[i].poll(digitalRead(input.pins[i]) == LOW, now, !center,
                                      center ? ExtendedCenterHoldMs : 0);
        if (!result)
          continue;
        InputEvent event{i >= 5, Key(i % 5), result >= 3,
                         result == 4 ? ExtendedCenterHoldMs : result == 3 ? Debouncer::LongPressMs : 0};
        event.atMs = now;
        event.timestamped = true;
        // Preserve queued presses; drop new events if the consumer is stalled.
        xQueueSend(input.events, &event, 0);
      }
      input.publishDirections();
      vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
    }
  }

public:
  static constexpr uint32_t ExtendedCenterHoldMs = 2000;
  void begin(const uint8_t *left = nullptr, const uint8_t *right = nullptr) {
    for (int i = 0; i < 5; i++) {
      pins[i] = left ? left[i] : hw::LeftKeys[i];
      pins[i + 5] = right ? right[i] : hw::RightKeys[i];
    }
    for (int p : pins)
      pinMode(p, INPUT_PULLUP);
  }
  bool start() {
    if (events)
      return true;
    events = xQueueCreate(32, sizeof(InputEvent));
    if (!events)
      return false;
    if (xTaskCreatePinnedToCore(sampleTask, "leap-input", 2048, this, 2, nullptr, 1) != pdPASS) {
      vQueueDelete(events);
      events = nullptr;
      return false;
    }
    return true;
  }
  uint8_t heldRightDirections() const {
    return rightDirections.load();
  }
  bool poll(InputEvent &e) {
    if (events)
      return xQueueReceive(events, &e, 0) == pdTRUE;
    for (int i = 0; i < 10; i++) {
      bool center = i % 5 == 4;
      int result = keys[i].poll(digitalRead(pins[i]) == LOW, millis(), !center,
                                center ? ExtendedCenterHoldMs : 0);
      if (result) {
        e = {i >= 5, Key(i % 5), result >= 3,
             result == 4 ? ExtendedCenterHoldMs : result == 3 ? Debouncer::LongPressMs : 0};
        e.atMs = millis();
        e.timestamped = true;
        publishDirections();
        return true;
      }
    }
    publishDirections();
    return false;
  }
};
} // namespace leap
