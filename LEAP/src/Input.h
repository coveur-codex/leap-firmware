#pragma once
#include "Core.h"
#include "Hardware.h"
#include <Arduino.h>
#include <freertos/queue.h>
#include <freertos/task.h>
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
  QueueHandle_t events = nullptr;
  static void sampleTask(void *context) {
    auto &input = *static_cast<Input *>(context);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
      uint32_t now = millis();
      for (int i = 0; i < 10; i++) {
        int result = input.keys[i].poll(digitalRead(input.pins[i]) == LOW, now, i % 5 != 4);
        if (!result)
          continue;
        InputEvent event{i >= 5, Key(i % 5), result == 3};
        // Preserve queued presses; drop new events if the consumer is stalled.
        xQueueSend(input.events, &event, 0);
      }
      vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
    }
  }

public:
  void begin() {
    for (int i = 0; i < 5; i++) {
      pins[i] = hw::LeftKeys[i];
      pins[i + 5] = hw::RightKeys[i];
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
  bool poll(InputEvent &e) {
    if (events)
      return xQueueReceive(events, &e, 0) == pdTRUE;
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
