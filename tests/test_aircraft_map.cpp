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
  puts("PASS: aircraft map centre, bearings, scale, date line, bounds and invalid coordinates");
}
