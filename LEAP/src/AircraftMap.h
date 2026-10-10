#pragma once
#include <algorithm>
#include <cmath>

namespace leap {
constexpr double AircraftMaxPredictionSeconds = 120;
// Great-circle forward projection; never alter the original observation.
inline bool predictAircraft(double &lat, double &lon, double speedKnots,
                            double headingDegrees, double ageSeconds) {
  if (!std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > 90 || std::abs(lon) > 180 ||
      !std::isfinite(speedKnots) || speedKnots < 0 || !std::isfinite(headingDegrees) ||
      !std::isfinite(ageSeconds) || ageSeconds < 0 || ageSeconds > AircraftMaxPredictionSeconds)
    return false;
  constexpr double rad = 3.141592653589793 / 180;
  double distance = speedKnots * ageSeconds / 3600 / 3440.065;
  double a = lat * rad, heading = headingDegrees * rad;
  double b = std::asin(std::clamp(std::sin(a) * std::cos(distance) +
      std::cos(a) * std::sin(distance) * std::cos(heading), -1.0, 1.0));
  lon += std::atan2(std::sin(heading) * std::sin(distance) * std::cos(a),
                    std::cos(distance) - std::sin(a) * std::sin(b)) / rad;
  lon = std::fmod(lon + 540, 360) - 180;
  lat = b / rad;
  return true;
}
inline double aircraftDistanceKm(double lat, double lon, double planeLat, double planeLon) {
  constexpr double rad = 3.141592653589793 / 180;
  double h = std::pow(std::sin((planeLat - lat) * rad / 2), 2) +
      std::cos(lat * rad) * std::cos(planeLat * rad) * std::pow(std::sin((planeLon - lon) * rad / 2), 2);
  return 6371.0088 * 2 * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
}
// Same centred Mercator square as the RainViewer crop on the homeserver.
constexpr double RadarWidthKm = 50;
constexpr int RadarRingStepKm = 10;
inline int radarRingPixels(int distanceKm, int halfPixels) {
  return std::lround(distanceKm * halfPixels / (RadarWidthKm / 2));
}
inline bool radarOffset(double lat, double lon, double planeLat, double planeLon,
                        int halfPixels, int &x, int &y) {
  if (!std::isfinite(lat) || !std::isfinite(lon) || !std::isfinite(planeLat) ||
      !std::isfinite(planeLon) || std::abs(lat) > 85 || std::abs(planeLat) > 85 ||
      std::abs(lon) > 180 || std::abs(planeLon) > 180 || halfPixels < 1) return false;
  constexpr double rad = 3.141592653589793 / 180;
  double scale = 6378.137 * std::cos(lat * rad) / (RadarWidthKm / 2);
  double east = std::remainder(planeLon - lon, 360.0) * rad * scale;
  double north = (std::asinh(std::tan(planeLat * rad)) - std::asinh(std::tan(lat * rad))) * scale;
  if (std::abs(east) > 1 || std::abs(north) > 1) return false;
  x = std::lround(east * halfPixels);
  y = -std::lround(north * halfPixels);
  return true;
}
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
