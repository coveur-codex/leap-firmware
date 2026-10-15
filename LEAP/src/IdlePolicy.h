#pragma once
#include <cstdint>
namespace leap {
struct IdlePolicy {
  bool dim, lock;
  static IdlePolicy evaluate(uint32_t now, uint32_t lastInput, bool chillVisible,
                             bool diagnostics) {
    bool eligible = !chillVisible && !diagnostics;
    return {eligible && uint32_t(now - lastInput) >= 60000,
            eligible && uint32_t(now - lastInput) >= 180000};
  }
};
} // namespace leap
