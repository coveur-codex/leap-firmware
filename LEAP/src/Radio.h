#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
namespace leap {
enum class RadioStatus : uint8_t { Stopped, Connecting, Buffering, Playing, NoWifi, InvalidUrl, Unsupported, ConnectionError, Timeout, MemoryError };
class Radio {
  struct Command { uint32_t generation; char url[1001]; };
  struct Pcm;
  QueueHandle_t commands = nullptr;
  SemaphoreHandle_t mutex = nullptr;
  Pcm *pcm = nullptr; // Only accessed under mutex; producer owns its lifetime.
  std::atomic<uint32_t> generation{0};
  std::atomic<uint32_t> requestedEpoch{0};
  std::atomic<bool> suspended{false};
  std::atomic<RadioStatus> state{RadioStatus::Stopped};
  bool current(uint32_t id) const { return requestedEpoch == id && !suspended && generation == id; }
  void status(uint32_t id, RadioStatus value) { if (generation == id) state = value; }
  void run();
  void stream(const Command &command);
public:
  bool begin();
  void play(const String &url);
  void stop();
  void suspend() { suspended = true; stop(); }
  void resume() { suspended = false; }
  uint32_t epoch() const { return generation.load(); }
  bool isSuspended() const { return suspended.load(); }
  bool active() const { return requestedEpoch != 0 && !suspended; }
  RadioStatus status() const { return state.load(); }
  // Audio task: a bounded copy, no network or decoding on the I2S task.
  size_t read(int16_t *stereo, size_t frames, uint32_t &rate);
  static const char *label(RadioStatus value);
};
extern Radio radio;
} // namespace leap
