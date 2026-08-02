#pragma once

#include <stdint.h>

constexpr uint16_t MICRO_RADAR_MAX_DISPLAY_AIRCRAFT = 48;
constexpr uint16_t MICRO_RADAR_MAX_DISPLAY_MAP_SEGMENTS = 900;
constexpr uint8_t MICRO_RADAR_MAX_DISPLAY_MAP_POINTS = 24;

struct MicroRadarDisplayAircraft {
  int16_t x;
  int16_t y;
  int16_t headingDeg;
  int32_t altitudeFt;
  uint16_t speedKt;
  char label[9];
  char icao[7];
  char registration[11];
  char typeCode[8];
  char airline[22];
  char operatorName[22];
  char modelName[24];
  char routeOrigin[8];
  char routeDestination[8];
  char squawk[6];
  char emergency[16];
  int16_t trailX[8];
  int16_t trailY[8];
  uint8_t trailCount;
  uint8_t iconType;
  bool stale;
  bool selected;
  bool tracked;
  bool emergencyActive;
};

struct MicroRadarDisplaySegment {
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  uint8_t kind;
};

struct MicroRadarDisplayMapPoint {
  int16_t x;
  int16_t y;
  uint8_t kind;
  char label[6];
};

struct MicroRadarDisplayState {
  uint16_t rangeNm;
  uint16_t aircraftReceived;
  uint16_t aircraftRendered;
  uint8_t labelDensity;
  uint8_t mapStyle;
  uint8_t aircraftLabelMode;
  uint8_t uiScale;
  uint8_t aircraftIconScale;
  uint8_t aircraftTextScale;
  uint8_t fontStyle;
  uint8_t displayRotation;
  uint8_t labelBackplateOpacity;
  uint8_t brightness;
  uint8_t sweepSecondsPerRotation;
  uint8_t airportIconType;
  uint8_t airportLabelScale;
  uint8_t rangeRingStyle;
  uint8_t rangeRingThickness;
  uint8_t crosshairStyle;
  uint8_t crosshairThickness;
  uint8_t sweepFadeWidthDeg;
  uint32_t aircraftColor;
  uint32_t sweepColor;
  uint32_t trailColor;
  uint32_t labelColor;
  uint32_t detailLabelColor;
  uint32_t altitudeLabelColor;
  uint32_t speedLabelColor;
  uint32_t landColor;
  uint32_t waterColor;
  uint32_t roadColor;
  uint32_t airportColor;
  uint32_t airportLabelColor;
  uint32_t scopeBackgroundColor;
  uint32_t scopeOutsideColor;
  uint32_t mapCoastColor;
  uint32_t mapBorderColor;
  uint32_t mapWaterLineColor;
  uint32_t rangeRingColor;
  uint32_t crosshairColor;
  bool mapEnabled;
  bool airportsEnabled;
  bool airportLabelsEnabled;
  bool rangeRingsEnabled;
  bool outerRangeRingEnabled;
  bool rangeRingLabelsEnabled;
  bool crosshairEnabled;
  bool wifiConnected;
  bool providerOk;
  bool metricUnits;
  bool lightTheme;
  bool trailsEnabled;
  bool sweepLineEnabled;
  bool tracking;
  bool touchDetected;
  bool touchActive;
  bool rasterBackgroundReady;
  bool detailOpen;
  uint16_t mapSegmentCount;
  uint8_t mapPointCount;
  MicroRadarDisplayAircraft aircraft[MICRO_RADAR_MAX_DISPLAY_AIRCRAFT];
  MicroRadarDisplaySegment mapSegments[MICRO_RADAR_MAX_DISPLAY_MAP_SEGMENTS];
  MicroRadarDisplayMapPoint mapPoints[MICRO_RADAR_MAX_DISPLAY_MAP_POINTS];
};

bool microRadarSmokeBegin();
void microRadarSmokeTick();
void microRadarSmokeSetState(const MicroRadarDisplayState& state);
