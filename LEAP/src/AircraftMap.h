#pragma once
#include <algorithm>
#include <cmath>

namespace leap {
// North-up azimuthal projection: true bearing and great-circle distance from home.
inline bool aircraftOffset(double lat, double lon, double planeLat, double planeLon,
                           double radiusNm, int radiusPixels, int &x, int &y) {
  if (!std::isfinite(lat) || !std::isfinite(lon) || !std::isfinite(planeLat) ||
      !std::isfinite(planeLon) || !std::isfinite(radiusNm) || std::abs(lat) > 90 ||
      std::abs(planeLat) > 90 || std::abs(lon) > 180 || std::abs(planeLon) > 180 ||
      radiusNm <= 0 || radiusPixels < 1)
    return false;
  constexpr double rad = 3.141592653589793 / 180;
  double a = lat * rad, b = planeLat * rad, delta = (planeLon - lon) * rad;
  double h = std::pow(std::sin((b - a) / 2), 2) +
             std::cos(a) * std::cos(b) * std::pow(std::sin(delta / 2), 2);
  double distance = 3440.065 * 2 * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
  if (distance > radiusNm)
    return false;
  double bearing = std::atan2(std::sin(delta) * std::cos(b),
                              std::cos(a) * std::sin(b) - std::sin(a) * std::cos(b) * std::cos(delta));
  x = std::lround(std::sin(bearing) * distance / radiusNm * radiusPixels);
  y = -std::lround(std::cos(bearing) * distance / radiusNm * radiusPixels);
  return true;
}
} // namespace leap
