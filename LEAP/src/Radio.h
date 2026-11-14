#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
namespace leap {
enum class RadioStatus : uint8_t { Stopped, Paused, Ended, Connecting, Buffering, Playing, NoWifi, InvalidUrl, Unsupported, ConnectionError, Timeout, MemoryError, DnsError, TlsError, HttpError };
class Radio {
  struct Command { uint32_t generation; char url[1001]; uint32_t position, duration; bool finite, seeked; };
  struct Pcm;
  QueueHandle_t commands = nullptr;
  SemaphoreHandle_t mutex = nullptr;
  Pcm *pcm = nullptr; // Only accessed under mutex; producer owns its lifetime.
  std::atomic<uint32_t> generation{0};
  std::atomic<uint32_t> requestedEpoch{0};
  std::atomic<bool> suspended{false};
  std::atomic<bool> pausedFlag{false}, endedNaturally{false};
  std::atomic<uint32_t> positionSeconds{0}, durationSeconds{0};
  void queue(const String &url, uint32_t position, uint32_t duration, bool finite, bool seeked);
  std::atomic<int> responseCode{0};
  std::atomic<RadioStatus> state{RadioStatus::Stopped};
  bool current(uint32_t id) const { return requestedEpoch == id && !suspended && generation == id; }
  void status(uint32_t id, RadioStatus value) { if (generation == id) state = value; }
  void run();
  void stream(const Command &command, struct Mp3SeekIndex &index, String &indexUrl, String &validator);
public:
  bool begin();
  void play(const String &url);
  void playEpisode(const String &url, uint32_t position=0, uint32_t duration=0, bool seeked=false) {
    queue(url, position, duration, true, seeked);
  }
  void pause(bool value) { pausedFlag=value; }
  bool paused() const { return pausedFlag.load(); }
  uint32_t position() const { return positionSeconds.load(); }
  uint32_t duration() const { return durationSeconds.load(); }
  bool naturalEnd() const { return endedNaturally.load(); }
  void stop();
  void suspend() { suspended = true; stop(); }
  void resume() { suspended = false; }
  uint32_t epoch() const { return generation.load(); }
  bool isSuspended() const { return suspended.load(); }
  bool active() const { return requestedEpoch != 0 && !suspended; }
  int httpCode() const { return responseCode.load(); }
  RadioStatus status() const { return pausedFlag && active() ? RadioStatus::Paused : state.load(); }
  // Audio task: a bounded copy, no network or decoding on the I2S task.
  size_t read(int16_t *stereo, size_t frames, uint32_t &rate);
  static const char *label(RadioStatus value);
};
extern Radio radio;
} // namespace leap
