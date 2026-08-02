#pragma once

#include <Arduino.h>
#include <vector>

#include "geo/Geo.h"

namespace micro_radar {

struct MapPackageManifest {
  String packageId;
  String version;
  double centerLat {0};
  double centerLon {0};
  uint16_t radiusNm {0};
  String checksumSha256;
  String attribution;
};

struct MapLineSegment {
  geo::Location a;
  geo::Location b;
  uint8_t kind {0};
  MapLineSegment() = default;
  MapLineSegment(geo::Location start, geo::Location end, uint8_t segmentKind) : a(start), b(end), kind(segmentKind) {}
};

struct ProjectedMapSegment {
  int16_t x0 {0};
  int16_t y0 {0};
  int16_t x1 {0};
  int16_t y1 {0};
  uint8_t kind {0};
  ProjectedMapSegment() = default;
  ProjectedMapSegment(int16_t startX, int16_t startY, int16_t endX, int16_t endY, uint8_t segmentKind)
      : x0(startX), y0(startY), x1(endX), y1(endY), kind(segmentKind) {}
};

struct MapPoint {
  geo::Location location;
  uint8_t kind {0};
  String code;
  MapPoint() = default;
  MapPoint(geo::Location pointLocation, uint8_t pointKind, String pointCode = "") : location(pointLocation), kind(pointKind), code(pointCode) {}
};

struct ProjectedMapPoint {
  int16_t x {0};
  int16_t y {0};
  uint8_t kind {0};
  String code;
  ProjectedMapPoint() = default;
  ProjectedMapPoint(int16_t pointX, int16_t pointY, uint8_t pointKind, String pointCode = "") : x(pointX), y(pointY), kind(pointKind), code(pointCode) {}
};

class MapPackageManager {
 public:
  bool begin();
  bool hasActivePackage() const;
  bool validatePackage(const String& manifestJson, const String& payloadPath, String* error);
  bool activateValidatedPackage(const String& packageId);
  bool retryDownloadForLocation(double centerLat, double centerLon, uint16_t radiusNm);
  bool ensureLocalSeedForLocation(double centerLat, double centerLon, uint16_t radiusNm);
  bool deleteCache();
  bool hasRasterBackground() const;
  bool isCurrentForLocation(double centerLat, double centerLon, uint16_t radiusNm) const;
  String statusText() const;
  std::vector<ProjectedMapSegment> projectSegments(geo::Location origin, uint16_t rangeNm, size_t maxSegments) const;
  std::vector<ProjectedMapPoint> projectPoints(geo::Location origin, uint16_t rangeNm, size_t maxPoints) const;
  String statusJson() const;

 private:
  bool loadActivePackage();
  bool saveActivePackage(double centerLat, double centerLon, uint16_t radiusNm);
  void loadEastMedSeedIfApplicable(double centerLat, double centerLon);

  bool active_ {false};
  bool rasterActive_ {false};
  double activeCenterLat_ {999.0};
  double activeCenterLon_ {999.0};
  uint16_t activeRadiusNm_ {0};
  String status_ {"radar-only: no active package"};
  String packageId_;
  String version_;
  String attribution_;
  std::vector<MapLineSegment> segments_;
  std::vector<MapPoint> points_;
};

}  // namespace micro_radar
