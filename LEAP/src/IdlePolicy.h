#pragma once
#include <cstdint>
namespace leap {
struct IdlePolicy {
  bool dim, lock, openNotification;
  static IdlePolicy evaluate(uint32_t now, uint32_t lastInput, bool chillVisible,
                             bool diagnostics, bool radioVisible = false) {
    bool eligible = !chillVisible && !diagnostics;
    bool dim = eligible && uint32_t(now - lastInput) >= 60000;
    return {dim, eligible && !radioVisible && uint32_t(now - lastInput) >= 180000,
            dim && !radioVisible};
  }
};
// Track real interaction separately: incoming notifications update the general
// idle clock but must not extend a radio listening session indefinitely.
struct RadioIdle {
  static constexpr uint32_t TimeoutMs = 30 * 60 * 1000;
  uint32_t lastInput = 0;
  bool visible = false;
  void show(bool shown, uint32_t now) {
    if (shown && !visible) lastInput = now;
    visible = shown;
  }
  void input(uint32_t now) { if (visible) lastInput = now; }
  bool expired(uint32_t now) const {
    return visible && uint32_t(now - lastInput) >= TimeoutMs;
  }
};
} // namespace leap
