#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace leap {
// Separate versioned NVS record: cable provisioning and OTA keep schema-1
// device credentials intact. Rows map sensor vectors into device coordinates.
struct MotionOrientation {
  uint32_t schema = 1;
  float axes[3][3]{};
  uint32_t checksum = 0;
  uint32_t hash() const {
    uint32_t h = 2166136261u;
    auto bytes = reinterpret_cast<const uint8_t *>(this);
    for (size_t i = 0; i < offsetof(MotionOrientation, checksum); ++i)
      h = (h ^ bytes[i]) * 16777619u;
    return h;
  }
  bool valid() const {
    if (schema != 1 || checksum != hash()) return false;
    for (int i = 0; i < 3; ++i)
      for (int j = i; j < 3; ++j) {
        float dot = 0;
        for (int k = 0; k < 3; ++k) {
          if (!std::isfinite(axes[i][k])) return false;
          dot += axes[i][k] * axes[j][k];
        }
        if (std::fabs(dot - (i == j ? 1.f : 0.f)) > 0.02f) return false;
      }
    return true;
  }
  void orient(float &x, float &y, float &z) const {
    float v[3]{x, y, z}, out[3]{};
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) out[i] += axes[i][j] * v[j];
    x = out[0]; y = out[1]; z = out[2];
  }
};
class MotionCalibration {
  float sum[3]{}, anchor[3]{}, positions[3][3]{};
  unsigned samples = 0;
public:
  int step = 0; // Flat/display up, right edge down, bottom edge down.
  bool collecting = false, rejected = false;
  MotionOrientation result;
  void capture() { rejected = false; samples = 0; for (float &v : sum) v = 0; collecting = true; }
  // 50 consecutive stationary samples at the MPU's 20-ms polling cadence.
  bool sample(float x, float y, float z, float gx, float gy, float gz) {
    if (!collecting || step >= 3) return false;
    float v[3]{x, y, z};
    float norm = x*x + y*y + z*z;
    bool stable = std::isfinite(norm) && norm > 0.81f && norm < 1.21f &&
        std::isfinite(gx+gy+gz) && gx*gx + gy*gy + gz*gz < 25;
    for (int i = 0; i < 3; ++i)
      if (samples && std::fabs(v[i] - anchor[i]) > 0.04f) stable = false;
    if (!stable) { capture(); return false; }
    for (int i = 0; i < 3; ++i) {
      if (!samples) anchor[i] = v[i];
      sum[i] += v[i];
    }
    if (++samples < 50) return false;
    float length = std::sqrt(sum[0]*sum[0] + sum[1]*sum[1] + sum[2]*sum[2]);
    for (int i = 0; i < 3; ++i) positions[step][i] = sum[i] / length;
    collecting = false;
    // Reject repeated/non-perpendicular poses without losing earlier steps.
    for (int p = 0; p < step; ++p) {
      float dot = 0;
      for (int i = 0; i < 3; ++i) dot += positions[step][i] * positions[p][i];
      if (std::fabs(dot) > 0.15f) { rejected = true; return false; }
    }
    if (++step < 3) return true;
    // Gram-Schmidt removes small placement errors, including arbitrary mounting.
    for (int row = 0; row < 3; ++row) {
      int pose = row == 0 ? 1 : row == 1 ? 2 : 0;
      for (int i = 0; i < 3; ++i) result.axes[row][i] = positions[pose][i];
      for (int p = 0; p < row; ++p) {
        float dot = 0;
        for (int i = 0; i < 3; ++i) dot += result.axes[row][i] * result.axes[p][i];
        for (int i = 0; i < 3; ++i) result.axes[row][i] -= dot * result.axes[p][i];
      }
      float n = 0;
      for (float a : result.axes[row]) n += a*a;
      for (float &a : result.axes[row]) a /= std::sqrt(n);
    }
    result.checksum = result.hash();
    return result.valid();
  }
};
} // namespace leap
