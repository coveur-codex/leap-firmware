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
  assert(leap::radarOffset(0,0,0,0,55,x,y) && x==0 && y==0);
  assert(leap::radarOffset(0,0,0.22,0.22,55,x,y) && x==54 && y==-54); // Square corner beyond 25-km circle.
  assert(!leap::radarOffset(0,0,0,0.23,55,x,y));
  assert(leap::radarOffset(0,179.95,0,-179.95,55,x,y) && x==24 && y==0);
  constexpr double pi=3.141592653589793;
  for (double latitude : {0.0,52.52,-52.52,80.0,85.0}) {
    double half = 24.99 / (6378.137 * std::cos(latitude*pi/180));
    double top = (2*std::atan(std::exp(std::asinh(std::tan(latitude*pi/180))+half))-pi/2)*180/pi;
    // At the poleward limit positions outside the provider's map are rejected.
    if (top <= 85) assert(leap::radarOffset(latitude,0,top,0,55,x,y) && x==0 && y==-55);
    assert(leap::radarOffset(latitude,0,latitude,half*180/pi,55,x,y) && x==55 && y==0);
  }
  assert(!leap::radarOffset(86,0,0,0,55,x,y));
  assert(!leap::radarOffset(0,0,NAN,0,55,x,y));
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
