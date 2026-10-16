#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace leap {
// Monotonic durations: independent of WLAN, wall-clock corrections and visible page.
class TimeModule {
  uint32_t timerAt = 0, stopwatchAt = 0;
  uint64_t stopwatchMs = 0;
  uint32_t remainingMs = 60000, durationMs = 60000;
  bool timerRunning = false, stopwatchRunning = false, finished = false, alarmPending = false;
public:
  static constexpr uint32_t MaxSeconds = 99 * 60 + 59;
  std::array<uint64_t, 20> laps{};
  unsigned lapCount = 0;
  bool advance(uint32_t now) {
    if (stopwatchRunning) stopwatchMs += uint32_t(now - stopwatchAt);
    stopwatchAt = now;
    if (!timerRunning) return false;
    uint32_t delta = now - timerAt;
    timerAt = now;
    if (delta < remainingMs) { remainingMs -= delta; return false; }
    remainingMs = 0; timerRunning = false; finished = alarmPending = true;
    return true;
  }
  bool tick(uint32_t now) { advance(now); bool alarm = alarmPending; alarmPending = false; return alarm; }
  void setSeconds(uint32_t seconds) {
    durationMs = std::clamp(seconds, uint32_t(1), MaxSeconds) * 1000;
    resetTimer();
  }
  void resetTimer() { remainingMs = durationMs; timerRunning = finished = false; }
  void toggleTimer(uint32_t now) {
    advance(now);
    if (finished) resetTimer();
    timerAt = now; timerRunning = !timerRunning;
  }
  void toggleStopwatch(uint32_t now) { advance(now); stopwatchAt = now; stopwatchRunning = !stopwatchRunning; }
  void resetStopwatch() { if (!stopwatchRunning) { stopwatchMs = 0; lapCount = 0; } }
  void lap(uint32_t now) { advance(now); if (stopwatchRunning && lapCount < laps.size()) laps[lapCount++] = stopwatchMs; }
  uint32_t secondsLeft() const { return (remainingMs + 999) / 1000; }
  uint32_t totalMs() const { return durationMs; }
  uint32_t leftMs() const { return remainingMs; }
  uint64_t elapsedMs() const { return stopwatchMs; }
  bool running() const { return timerRunning; }
  bool measuring() const { return stopwatchRunning; }
  bool done() const { return finished; }
};
} // namespace leap
