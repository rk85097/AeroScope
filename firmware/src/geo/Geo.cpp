#include "geo/Geo.h"

namespace micro_radar::geo {

double degToRad(double deg) { return deg * M_PI / 180.0; }
double radToDeg(double rad) { return rad * 180.0 / M_PI; }

double normalizeDeg(double deg) {
  while (deg < 0) deg += 360.0;
  while (deg >= 360.0) deg -= 360.0;
  return deg;
}

double distanceNm(Location a, Location b) {
  const double lat1 = degToRad(a.lat);
  const double lat2 = degToRad(b.lat);
  const double dLat = degToRad(b.lat - a.lat);
  const double dLon = degToRad(b.lon - a.lon);
  const double s = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
  return 2.0 * kEarthRadiusNm * atan2(sqrt(s), sqrt(1.0 - s));
}

double bearingDeg(Location from, Location to) {
  const double lat1 = degToRad(from.lat);
  const double lat2 = degToRad(to.lat);
  const double dLon = degToRad(to.lon - from.lon);
  const double y = sin(dLon) * cos(lat2);
  const double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
  return normalizeDeg(radToDeg(atan2(y, x)));
}

Point projectAzimuthalEquidistant(Location origin, Location point, double rangeNm, double radiusPx) {
  const double distance = distanceNm(origin, point);
  const double bearing = degToRad(bearingDeg(origin, point));
  const double r = (distance / rangeNm) * radiusPx;
  return {sin(bearing) * r, -cos(bearing) * r};
}

}  // namespace micro_radar::geo
