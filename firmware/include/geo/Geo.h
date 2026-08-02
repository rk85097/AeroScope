#pragma once

#include <math.h>

namespace micro_radar::geo {

constexpr double kEarthRadiusNm = 3440.065;

struct Location {
  double lat;
  double lon;
};

struct Point {
  double x;
  double y;
};

double degToRad(double deg);
double radToDeg(double rad);
double normalizeDeg(double deg);
double distanceNm(Location a, Location b);
double bearingDeg(Location from, Location to);
Point projectAzimuthalEquidistant(Location origin, Location point, double rangeNm, double radiusPx);

}  // namespace micro_radar::geo
