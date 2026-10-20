#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace leap {
// Navigation has no Arduino dependencies, allocations or network operations.
struct FinanceNavigation {
  size_t selected = 0, detailSelected = 0;
  bool detail = false;
  uint32_t nextRotation = 0;
  void enter(uint32_t now) { nextRotation = now + 10000; }
  void normalize(size_t count) {
    if (!count) { selected = detailSelected = 0; detail = false; return; }
    selected = std::min(selected, count - 1);
    detailSelected = std::min(detailSelected, count - 1);
  }
  size_t index() const { return detail ? detailSelected : selected; }
  size_t page() const { return selected / 3; }
  static size_t pages(size_t count) { return (count + 2) / 3; }
  void move(int delta, size_t count, uint32_t now) {
    normalize(count);
    if (!count) return;
    auto &target = detail ? detailSelected : selected;
    target = size_t((int(target) + int(count) + delta) % int(count));
    nextRotation = now + 40000; // 30 seconds pause, then normal 10-second period.
  }
  void center(size_t count, uint32_t now) {
    normalize(count);
    if (!count) return;
    if (!detail) detailSelected = selected;
    detail = !detail;
    nextRotation = now + 40000;
  }
  void back(uint32_t now) { detail = false; nextRotation = now + 40000; }
  bool tick(size_t count, uint32_t now) {
    normalize(count);
    if (detail || count <= 3 || int32_t(now - nextRotation) < 0) return false;
    selected = ((page() + 1) % pages(count)) * 3;
    nextRotation = now + 10000;
    return true;
  }
};

inline std::string financeNumber(double value) {
  if (!std::isfinite(value) || value <= 0) return "--";
  char result[32];
  // At most eight cells: 96 pixels in the 114-pixel overview column at size 2.
  if (value >= 1e12) std::snprintf(result, sizeof(result), "%.1fT", value / 1e12);
  else if (value >= 1e9) std::snprintf(result, sizeof(result), "%.1fB", value / 1e9);
  else if (value >= 1e6) std::snprintf(result, sizeof(result), "%.1fM", value / 1e6);
  else if (value >= 100000) std::snprintf(result, sizeof(result), "%.0f", value);
  else if (value < .01) std::snprintf(result, sizeof(result), "%.5f", value);
  else std::snprintf(result, sizeof(result), "%.2f", value);
  return result;
}
inline bool financeStale(bool flagged, bool online, int64_t now, int64_t generated,
                         bool closed) {
  return flagged || !online || now <= 0 || generated <= 0 || now < generated ||
         now - generated > (closed ? 1200 : 600);
}
inline int financeChartX(int64_t t, int64_t first, int64_t last, int x, int width) {
  if (last <= first) return x + width / 2;
  double fraction = double(t - first) / double(last - first);
  return x + int(std::max(0.0, std::min(1.0, fraction)) * (width - 1));
}
inline int financeChartY(double p, double low, double high, int y, int height) {
  if (high <= low) return y + height / 2;
  return y + height - 1 - int(std::max(0.0, std::min(1.0, (p-low)/(high-low))) * (height-1));
}
} // namespace leap
