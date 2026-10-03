#include "AircraftMap.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
  int x, y;
  assert(leap::aircraftOffset(52.52, 13.405, 52.52, 13.405, 25, 46, x, y));
  assert(x == 0 && y == 0);
  assert(leap::aircraftOffset(0, 0, 0.1, 0, 12, 48, x, y));
  assert(x == 0 && y == -24);
  assert(leap::aircraftOffset(0, 0, -0.1, 0, 12, 48, x, y));
  assert(x == 0 && y == 24);
  assert(leap::aircraftOffset(0, 0, 0, 0.1, 12, 48, x, y));
  assert(x == 24 && y == 0);
  assert(leap::aircraftOffset(0, 0, 0, -0.1, 12, 48, x, y));
  assert(x == -24 && y == 0);
  assert(leap::aircraftOffset(0, 179.95, 0, -179.95, 12, 48, x, y));
  assert(x == 24 && y == 0);
  assert(!leap::aircraftOffset(0, 0, 1, 0, 12, 48, x, y));
  assert(!leap::aircraftOffset(91, 0, 0, 0, 12, 48, x, y));
  assert(!leap::aircraftOffset(0, 0, 0, 181, 12, 48, x, y));
  assert(!leap::aircraftOffset(0, 0, 0, 0, 0, 48, x, y));
  assert(!leap::aircraftOffset(0, 0, std::numeric_limits<double>::quiet_NaN(), 0, 12, 48, x, y));
  double lat = 0, lon = 0;
  assert(leap::predictAircraft(lat, lon, 360, 90, 2));
  assert(std::abs(lat) < 0.000001);
  assert(std::abs(leap::aircraftDistanceKm(0, 0, lat, lon) - 0.3704) < 0.0001);
  lat = 0; lon = 179.999;
  assert(leap::predictAircraft(lat, lon, 360, 90, 2));
  assert(lon < -179.99);
  lat = 0; lon = 0;
  assert(leap::predictAircraft(lat, lon, 360, 0, 2));
  assert(lat > 0 && std::abs(lon) < 0.000001);
  assert(!leap::predictAircraft(lat, lon, 360, 90, 121));
  assert(!leap::predictAircraft(lat, lon, -1, 90, 2));
  assert(!leap::predictAircraft(lat, lon, 360, std::numeric_limits<double>::quiet_NaN(), 2));
  assert(!leap::predictAircraft(lat, lon, 360, 90, -1));
  double savedLat = lat, savedLon = lon;
  assert(leap::predictAircraft(lat, lon, 0, 90, 120));
  assert(std::abs(lat - savedLat) < 0.000001 && std::abs(lon - savedLon) < 0.000001);
  puts("PASS: aircraft map centre, bearings, scale, date line, bounds and invalid coordinates");
}
