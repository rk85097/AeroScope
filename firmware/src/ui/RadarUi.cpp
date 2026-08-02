#include "ui/RadarUi.h"

#include "aircraft/AircraftTypeIconDb.h"
#include "display/SpotpearSmokeDisplay.h"
#include "geo/Geo.h"

#include <Wire.h>
#include <WiFi.h>
#include <string.h>

namespace micro_radar {

namespace {
constexpr uint8_t kGt911AddrPrimary = 0x5D;
constexpr uint8_t kGt911AddrAlt = 0x14;
constexpr uint16_t kGt911StatusReg = 0x814E;
constexpr uint16_t kGt911Point1Reg = 0x8150;
constexpr int kTouchSda = 15;
constexpr int kTouchScl = 7;
constexpr int kTouchInt = 16;
constexpr int kScreenCenter = 240;
constexpr int kRadarRadius = 232;
constexpr int kRangeHudX0 = 198;
constexpr int kRangeHudX1 = 282;
constexpr int kRangeHudY0 = 430;
constexpr int kRangeHudY1 = 462;
constexpr uint32_t kTouchPollMs = 12;
constexpr uint32_t kRenderFrameMs = 16;
constexpr uint32_t kTapDebounceMs = 120;
constexpr uint32_t kRangeDebounceMs = 650;
constexpr uint32_t kLongPressMs = 900;
constexpr int kDragThresholdPx = 18;

uint8_t touchAddr = kGt911AddrPrimary;
bool touchStarted = false;
int touchSda = kTouchSda;
int touchScl = kTouchScl;
uint8_t lastTouchStatus = 0;
bool lastTouchStatusReadOk = false;

bool gt911WriteReg(uint8_t addr, uint16_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool gt911Read(uint8_t addr, uint16_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  const uint8_t got = Wire.requestFrom(addr, static_cast<uint8_t>(len));
  if (got != len) return false;
  for (size_t i = 0; i < len; ++i) data[i] = Wire.read();
  return true;
}

bool probeAddress(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void scanTouchBus() {
  Serial.print("I2C scan:");
  bool any = false;
  for (uint8_t addr = 1; addr < 0x7F; ++addr) {
    if (probeAddress(addr)) {
      Serial.printf(" 0x%02X", addr);
      any = true;
    }
  }
  Serial.println(any ? "" : " none");
}

bool beginTouchOnPins(int sda, int scl) {
  Wire.end();
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  delay(80);
  if (probeAddress(kGt911AddrPrimary)) {
    touchAddr = kGt911AddrPrimary;
    touchSda = sda;
    touchScl = scl;
    return true;
  }
  if (probeAddress(kGt911AddrAlt)) {
    touchAddr = kGt911AddrAlt;
    touchSda = sda;
    touchScl = scl;
    return true;
  }
  return false;
}

bool readTouchPoint(uint16_t* x, uint16_t* y) {
  if (!touchStarted) return false;
  uint8_t status = 0;
  if (!gt911Read(touchAddr, kGt911StatusReg, &status, 1)) {
    const uint8_t alt = touchAddr == kGt911AddrPrimary ? kGt911AddrAlt : kGt911AddrPrimary;
    if (!gt911Read(alt, kGt911StatusReg, &status, 1)) return false;
    touchAddr = alt;
  }
  lastTouchStatus = status;
  lastTouchStatusReadOk = true;
  if ((status & 0x80) == 0) return false;
  const uint8_t points = status & 0x0F;
  if (points == 0) {
    gt911WriteReg(touchAddr, kGt911StatusReg, 0);
    return false;
  }
  uint8_t point[8] = {};
  const bool ok = gt911Read(touchAddr, kGt911Point1Reg, point, sizeof(point));
  gt911WriteReg(touchAddr, kGt911StatusReg, 0);
  if (!ok) return false;
  *x = static_cast<uint16_t>(point[0] | (point[1] << 8));
  *y = static_cast<uint16_t>(point[2] | (point[3] << 8));
  return *x < 480 && *y < 480;
}

struct TouchPoint {
  uint16_t x;
  uint16_t y;
};

TouchPoint transformTouch(uint16_t rawX, uint16_t rawY, uint8_t variant) {
  switch (variant) {
    case 1: return {rawY, rawX};
    case 2: return {static_cast<uint16_t>(479 - rawX), rawY};
    case 3: return {rawX, static_cast<uint16_t>(479 - rawY)};
    case 4: return {static_cast<uint16_t>(479 - rawX), static_cast<uint16_t>(479 - rawY)};
    case 5: return {static_cast<uint16_t>(479 - rawY), rawX};
    case 6: return {rawY, static_cast<uint16_t>(479 - rawX)};
    case 7: return {static_cast<uint16_t>(479 - rawY), static_cast<uint16_t>(479 - rawX)};
    default: return {rawX, rawY};
  }
}

bool inCircle(int x, int y) {
  const int dx = x - kScreenCenter;
  const int dy = y - kScreenCenter;
  return dx * dx + dy * dy <= kRadarRadius * kRadarRadius;
}

bool inRangeHud(int x, int y) {
  return x >= kRangeHudX0 && x <= kRangeHudX1 && y >= kRangeHudY0 && y <= kRangeHudY1;
}

TouchPoint mapTouchPoint(uint16_t rawX, uint16_t rawY) {
  return transformTouch(rawX, rawY, 0);
}

TouchPoint unrotateTouchPoint(TouchPoint p, uint8_t rotation) {
  switch (rotation & 0x03) {
    case 1: return {p.y, static_cast<uint16_t>(479 - p.x)};
    case 2: return {static_cast<uint16_t>(479 - p.x), static_cast<uint16_t>(479 - p.y)};
    case 3: return {static_cast<uint16_t>(479 - p.y), p.x};
    default: return p;
  }
}

void copyLabel(char* out, const String& value) {
  size_t n = value.length();
  if (n > 8) n = 8;
  for (size_t i = 0; i < n; ++i) out[i] = value[i];
  out[n] = '\0';
}

void copyText(char* out, size_t outSize, const String& value) {
  if (!out || outSize == 0) return;
  size_t n = value.length();
  if (n > outSize - 1) n = outSize - 1;
  for (size_t i = 0; i < n; ++i) out[i] = value[i];
  out[n] = '\0';
}

bool isEmergencyAircraft(const Aircraft& aircraft) {
  if (aircraft.emergency.available() && !aircraft.emergency.value.isEmpty() && aircraft.emergency.value != "none") return true;
  if (!aircraft.squawk.available()) return false;
  const String squawk = aircraft.squawk.value;
  return squawk == "7500" || squawk == "7600" || squawk == "7700";
}

bool startsWithAny(const String& value, const char* const* prefixes, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (value.startsWith(prefixes[i])) return true;
  }
  return false;
}

uint8_t classifyAircraftIcon(const Aircraft& aircraft) {
  String type = aircraft.typeCode.available() ? aircraft.typeCode.value : "";
  type.trim();
  type.toUpperCase();
  String model = aircraft.modelName.available() ? aircraft.modelName.value : "";
  model.trim();
  model.toUpperCase();
  String callsign = aircraft.callsign.available() ? aircraft.callsign.value : "";
  callsign.trim();
  callsign.toUpperCase();
  const String combined = type + " " + model;

  if (combined.indexOf("GULFSTREAM") >= 0 || combined.indexOf("G200") >= 0 || combined.indexOf("CITATION") >= 0 ||
      combined.indexOf("CHALLENGER") >= 0 || combined.indexOf("FALCON") >= 0 || combined.indexOf("LEARJET") >= 0 ||
      combined.indexOf("PHENOM") >= 0 || combined.indexOf("HAWKER") >= 0) {
    return aircraft_type_icon_db::kIconNarrowJet;
  }

  if (!type.isEmpty()) {
    const uint8_t dbIcon = aircraft_type_icon_db::lookup(type);
    if (dbIcon != aircraft_type_icon_db::kIconNotFound && dbIcon != aircraft_type_icon_db::kIconUnknown) return dbIcon;
  }

  static const char* const fourEngineHeavyPrefixes[] = {"A34", "A38", "B74", "A124", "A225", "IL9", "IL96"};
  if (startsWithAny(type, fourEngineHeavyPrefixes, sizeof(fourEngineHeavyPrefixes) / sizeof(fourEngineHeavyPrefixes[0])) ||
      combined.indexOf("747") >= 0 || combined.indexOf("A340") >= 0 || combined.indexOf("A380") >= 0 ||
      combined.indexOf("IL-96") >= 0 || combined.indexOf("AN-124") >= 0 || combined.indexOf("AN-225") >= 0) {
    return aircraft_type_icon_db::kIconHeavyJet;
  }
  static const char* const widebodyTwinPrefixes[] = {"A33", "A35", "B76", "B77", "B78"};
  if (startsWithAny(type, widebodyTwinPrefixes, sizeof(widebodyTwinPrefixes) / sizeof(widebodyTwinPrefixes[0])) ||
      combined.indexOf("767") >= 0 || combined.indexOf("777") >= 0 || combined.indexOf("787") >= 0 ||
      combined.indexOf("A330") >= 0 || combined.indexOf("A350") >= 0) {
    return aircraft_type_icon_db::kIconNarrowJet;
  }

  static const char* const narrowPrefixes[] = {"A31", "A32", "B73", "B75", "BCS", "E19", "E29", "MD8", "MD9"};
  if (startsWithAny(type, narrowPrefixes, sizeof(narrowPrefixes) / sizeof(narrowPrefixes[0])) || combined.indexOf("737") >= 0 ||
      combined.indexOf("A320") >= 0 || combined.indexOf("A321") >= 0 || combined.indexOf("A319") >= 0 ||
      combined.indexOf("EMBRAER 190") >= 0 || combined.indexOf("E190") >= 0) {
    return aircraft_type_icon_db::kIconNarrowJet;
  }

  static const char* const regionalPrefixes[] = {"CRJ", "E13", "E14", "E17", "E18", "E19", "AT4", "AT5", "AT7", "DH8", "SF3"};
  if (startsWithAny(type, regionalPrefixes, sizeof(regionalPrefixes) / sizeof(regionalPrefixes[0])) || combined.indexOf("CRJ") >= 0 ||
      combined.indexOf("DASH") >= 0 || combined.indexOf("ATR") >= 0 || combined.indexOf("EMBRAER") >= 0) {
    return aircraft_type_icon_db::kIconRegionalJet;
  }

  static const char* const twinPropPrefixes[] = {"BE2", "B350", "C30", "C40", "C42", "P06", "PA3", "PA4", "PAY", "AC", "MU2"};
  if (startsWithAny(type, twinPropPrefixes, sizeof(twinPropPrefixes) / sizeof(twinPropPrefixes[0])) || combined.indexOf("TWIN") >= 0 ||
      combined.indexOf("KING AIR") >= 0 || combined.indexOf("BARON") >= 0 || combined.indexOf("P2006") >= 0 ||
      combined.indexOf("CHEYENNE") >= 0 || combined.indexOf("TURBO COMMANDER") >= 0) {
    return aircraft_type_icon_db::kIconTwinProp;
  }

  static const char* const propPrefixes[] = {"A22", "A210", "BLSA", "C15", "C16", "C17", "C18", "C20", "C21", "C22", "C23", "C72",
                                             "CH60", "CH65", "CH70", "CH75", "CT", "DA2", "DA4", "DV20", "EV97", "FK9", "JAB",
                                             "LA4", "MCR", "P20", "P28", "P32", "P46", "P66", "P92", "PISI", "RALL", "SIRA",
                                             "SIR2", "SIR3", "SIRA", "SR2", "STCH", "TEXA", "TL20", "ULAC", "VFOX", "VL3", "WT9"};
  if (startsWithAny(type, propPrefixes, sizeof(propPrefixes) / sizeof(propPrefixes[0])) || combined.indexOf("CESSNA") >= 0 ||
      combined.indexOf("PIPER") >= 0 || combined.indexOf("CIRRUS") >= 0 || combined.indexOf("DIAMOND") >= 0 ||
      combined.indexOf("FLY SYNTHESIS") >= 0 || combined.indexOf("TEXAN") >= 0 || combined.indexOf("LIGHT SPORT") >= 0 ||
      combined.indexOf("ULTRALIGHT") >= 0 || combined.indexOf("EVEKTOR") >= 0 || combined.indexOf("EV-97") >= 0 ||
      combined.indexOf("EUROSTAR") >= 0 || combined.indexOf("SPORTSTAR") >= 0 || combined.indexOf("TECNAM") >= 0 ||
      combined.indexOf("JABIRU") >= 0 || combined.indexOf("ZENITH") >= 0 || combined.indexOf("SAVANNAH") >= 0 ||
      combined.indexOf("AEROPRAKT") >= 0 || combined.indexOf("FLIGHT DESIGN") >= 0 || combined.indexOf("SKYRANGER") >= 0) {
    return aircraft_type_icon_db::kIconProp;
  }

  if (type.isEmpty() && model.isEmpty()) {
    static const char* const airlinePrefixes[] = {"AAL", "AAR", "ACA", "AFR", "AIC", "AIZ", "BAW", "BER", "CFG", "DAL", "DLH", "EIN",
                                                  "ELY", "ETD", "EZY", "IBE", "ISR", "KLM", "QTR", "RYR", "SAS", "SWR", "THY", "UAL",
                                                  "UAE", "VIR", "WZZ"};
    if (startsWithAny(callsign, airlinePrefixes, sizeof(airlinePrefixes) / sizeof(airlinePrefixes[0]))) return aircraft_type_icon_db::kIconNarrowJet;
    const double speed = aircraft.groundSpeedKt.available() ? aircraft.groundSpeedKt.value : 0.0;
    const int32_t altitude = aircraft.baroAltitudeFt.available() ? aircraft.baroAltitudeFt.value :
                             aircraft.geomAltitudeFt.available() ? aircraft.geomAltitudeFt.value : 0;
    if (speed > 0.0 && speed <= 175.0 && altitude < 14000) return aircraft_type_icon_db::kIconProp;
    if (speed > 0.0 && speed <= 250.0 && altitude < 20000) return aircraft_type_icon_db::kIconRegionalJet;
  }

  return aircraft_type_icon_db::kIconNarrowJet;
}
}  // namespace

bool RadarUi::begin(RadarEngine* radar, ConfigManager* config, MapPackageManager* maps) {
  radar_ = radar;
  config_ = config;
  maps_ = maps;
  microRadarSmokeBegin();
  pinMode(kTouchInt, INPUT_PULLUP);
  const int buses[][2] = {
      {kTouchSda, kTouchScl},
      {6, 7},
      {7, 15},
      {15, 6},
  };
  for (const auto& bus : buses) {
    if (beginTouchOnPins(bus[0], bus[1])) {
      touchStarted = true;
      break;
    }
  }
  if (!touchStarted) scanTouchBus();
  Serial.printf("GT911 touch: %s at 0x%02X SDA=%d SCL=%d INT=%d\n", touchStarted ? "ready" : "not detected", touchAddr, touchSda,
                touchScl, kTouchInt);
  return true;
}

void RadarUi::tick() {
  static uint32_t lastSmokeFrameMs = 0;
  static uint32_t lastTouchDiagMs = 0;
  static double cachedMapLat = 999.0;
  static double cachedMapLon = 999.0;
  static uint16_t cachedMapRange = 0;
  static bool cachedMapEnabled = false;
  static bool cachedAirportsEnabled = false;
  static uint32_t lastMapProjectMs = 0;
  static uint16_t cachedMapSegmentCount = 0;
  static uint8_t cachedMapPointCount = 0;
  static std::vector<Aircraft> aircraftSnapshot;
  static MicroRadarDisplaySegment cachedMapSegments[MICRO_RADAR_MAX_DISPLAY_MAP_SEGMENTS];
  static MicroRadarDisplayMapPoint cachedMapPoints[MICRO_RADAR_MAX_DISPLAY_MAP_POINTS];
  const uint32_t now = millis();
  handleTouch(now);
  if (now - lastTouchDiagMs >= 5000) {
    lastTouchDiagMs = now;
    lastTouchStatusReadOk = false;
    uint8_t status = 0;
    const bool ok = touchStarted && gt911Read(touchAddr, kGt911StatusReg, &status, 1);
    if (ok) {
      lastTouchStatus = status;
      lastTouchStatusReadOk = true;
    }
    Serial.printf("Touch diag: started=%d addr=0x%02X SDA=%d SCL=%d INT=%d statusOk=%d status=0x%02X\n", touchStarted ? 1 : 0,
                  touchAddr, touchSda, touchScl, digitalRead(kTouchInt), ok ? 1 : 0, status);
  }
  if (radar_) radar_->animate(now);
  if (now - lastSmokeFrameMs >= kRenderFrameMs) {
    lastSmokeFrameMs = now;
    static MicroRadarDisplayState state;
    memset(&state, 0, sizeof(state));
    if (radar_) {
      const AppConfig cfg = radar_->snapshotConfig();
      state.rangeNm = cfg.rangeNm;
      state.mapEnabled = cfg.mapEnabled;
      state.airportsEnabled = cfg.airportsEnabled;
      state.airportLabelsEnabled = cfg.airportLabelsEnabled;
      state.rangeRingsEnabled = cfg.rangeRingsEnabled;
      state.outerRangeRingEnabled = cfg.outerRangeRingEnabled;
      state.rangeRingLabelsEnabled = cfg.rangeRingLabelsEnabled;
      state.metricUnits = cfg.units == Units::Metric;
      state.lightTheme = cfg.theme == ThemeMode::Light;
      state.trailsEnabled = cfg.trailsEnabled;
      state.sweepLineEnabled = cfg.sweepLineEnabled;
      state.labelDensity = cfg.labelDensity;
      state.uiScale = cfg.uiScale;
      state.aircraftIconScale = cfg.aircraftIconScale;
      state.aircraftTextScale = cfg.aircraftTextScale;
      state.fontStyle = cfg.fontStyle;
      state.displayRotation = cfg.displayRotation;
      state.labelBackplateOpacity = cfg.labelBackplateOpacity;
      state.brightness = cfg.brightness;
      state.sweepSecondsPerRotation = cfg.sweepSecondsPerRotation;
      state.airportIconType = cfg.airportIconType;
      state.airportLabelScale = cfg.airportLabelScale;
      state.rangeRingStyle = cfg.rangeRingStyle;
      state.rangeRingThickness = cfg.rangeRingThickness;
      state.crosshairEnabled = cfg.crosshairEnabled;
      state.crosshairStyle = cfg.crosshairStyle;
      state.crosshairThickness = cfg.crosshairThickness;
      state.sweepFadeWidthDeg = cfg.sweepFadeWidthDeg;
      state.aircraftColor = cfg.aircraftColor;
      state.sweepColor = cfg.sweepColor;
      state.trailColor = cfg.trailColor;
      state.labelColor = cfg.labelColor;
      state.detailLabelColor = cfg.detailLabelColor;
      state.altitudeLabelColor = cfg.altitudeLabelColor;
      state.speedLabelColor = cfg.speedLabelColor;
      state.landColor = cfg.landColor;
      state.waterColor = cfg.waterColor;
      state.roadColor = cfg.roadColor;
      state.airportColor = cfg.airportColor;
      state.airportLabelColor = cfg.airportLabelColor;
      state.scopeBackgroundColor = cfg.scopeBackgroundColor;
      state.scopeOutsideColor = cfg.scopeOutsideColor;
      state.mapCoastColor = cfg.mapCoastColor;
      state.mapBorderColor = cfg.mapBorderColor;
      state.mapWaterLineColor = cfg.mapWaterLineColor;
      state.rangeRingColor = cfg.rangeRingColor;
      state.crosshairColor = cfg.crosshairColor;
      state.mapStyle = static_cast<uint8_t>(cfg.mapStyle);
      state.aircraftLabelMode = static_cast<uint8_t>(cfg.aircraftLabelMode);
      state.rasterBackgroundReady = false;
      if (maps_ && cfg.mapEnabled) {
        const bool mapScopeChanged = fabs(cfg.homeLat - cachedMapLat) > 0.000001 || fabs(cfg.homeLon - cachedMapLon) > 0.000001 ||
                                     cfg.rangeNm != cachedMapRange || cfg.mapEnabled != cachedMapEnabled ||
                                     cfg.airportsEnabled != cachedAirportsEnabled;
        if (mapScopeChanged || now - lastMapProjectMs > 1000) {
          lastMapProjectMs = now;
          cachedMapLat = cfg.homeLat;
          cachedMapLon = cfg.homeLon;
          cachedMapRange = cfg.rangeNm;
          cachedMapEnabled = cfg.mapEnabled;
          cachedAirportsEnabled = cfg.airportsEnabled;
          const auto segments = maps_->projectSegments({cfg.homeLat, cfg.homeLon}, cfg.rangeNm, MICRO_RADAR_MAX_DISPLAY_MAP_SEGMENTS);
          cachedMapSegmentCount = segments.size() > MICRO_RADAR_MAX_DISPLAY_MAP_SEGMENTS ? MICRO_RADAR_MAX_DISPLAY_MAP_SEGMENTS : segments.size();
          for (uint16_t i = 0; i < cachedMapSegmentCount; ++i) {
            cachedMapSegments[i].x0 = segments[i].x0;
            cachedMapSegments[i].y0 = segments[i].y0;
            cachedMapSegments[i].x1 = segments[i].x1;
            cachedMapSegments[i].y1 = segments[i].y1;
            cachedMapSegments[i].kind = segments[i].kind;
          }
          cachedMapPointCount = 0;
          if (cfg.airportsEnabled) {
          const auto points = maps_->projectPoints({cfg.homeLat, cfg.homeLon}, cfg.rangeNm, MICRO_RADAR_MAX_DISPLAY_MAP_POINTS);
            cachedMapPointCount = points.size() > MICRO_RADAR_MAX_DISPLAY_MAP_POINTS ? MICRO_RADAR_MAX_DISPLAY_MAP_POINTS : points.size();
            for (uint8_t i = 0; i < cachedMapPointCount; ++i) {
              cachedMapPoints[i].x = points[i].x;
              cachedMapPoints[i].y = points[i].y;
              cachedMapPoints[i].kind = points[i].kind;
              strncpy(cachedMapPoints[i].label, points[i].code.c_str(), sizeof(cachedMapPoints[i].label) - 1);
              cachedMapPoints[i].label[sizeof(cachedMapPoints[i].label) - 1] = '\0';
            }
          }
        }
        state.mapSegmentCount = cachedMapSegmentCount;
        for (uint16_t i = 0; i < state.mapSegmentCount; ++i) state.mapSegments[i] = cachedMapSegments[i];
        if (cfg.airportsEnabled) {
          state.mapPointCount = cachedMapPointCount;
          for (uint8_t i = 0; i < state.mapPointCount; ++i) state.mapPoints[i] = cachedMapPoints[i];
        }
      }
      radar_->snapshotVisibleAircraft(&aircraftSnapshot);
        state.aircraftReceived = aircraftSnapshot.size();
        state.aircraftRendered = aircraftSnapshot.size() > MICRO_RADAR_MAX_DISPLAY_AIRCRAFT ? MICRO_RADAR_MAX_DISPLAY_AIRCRAFT : aircraftSnapshot.size();
        bool selectedFound = false;
        for (uint16_t i = 0; i < state.aircraftRendered; ++i) {
          state.aircraft[i].x = static_cast<int16_t>(aircraftSnapshot[i].renderX);
          state.aircraft[i].y = static_cast<int16_t>(aircraftSnapshot[i].renderY);
          state.aircraft[i].altitudeFt = aircraftSnapshot[i].baroAltitudeFt.available()
                                             ? aircraftSnapshot[i].baroAltitudeFt.value
                                             : aircraftSnapshot[i].geomAltitudeFt.available() ? aircraftSnapshot[i].geomAltitudeFt.value : 0;
          state.aircraft[i].speedKt = aircraftSnapshot[i].groundSpeedKt.available() ? static_cast<uint16_t>(aircraftSnapshot[i].groundSpeedKt.value) : 0;
          state.aircraft[i].trailCount = aircraftSnapshot[i].trailCount;
          for (uint8_t j = 0; j < state.aircraft[i].trailCount && j < 8; ++j) {
            state.aircraft[i].trailX[j] = aircraftSnapshot[i].trailX[j];
            state.aircraft[i].trailY[j] = aircraftSnapshot[i].trailY[j];
          }
          if (aircraftSnapshot[i].callsign.available() && !aircraftSnapshot[i].callsign.value.isEmpty()) {
            copyLabel(state.aircraft[i].label, aircraftSnapshot[i].callsign.value);
          } else {
            copyLabel(state.aircraft[i].label, aircraftSnapshot[i].icao24);
          }
          copyText(state.aircraft[i].icao, sizeof(state.aircraft[i].icao), aircraftSnapshot[i].icao24);
          copyText(state.aircraft[i].registration, sizeof(state.aircraft[i].registration),
                   aircraftSnapshot[i].registration.available() ? aircraftSnapshot[i].registration.value : "");
          copyText(state.aircraft[i].typeCode, sizeof(state.aircraft[i].typeCode),
                   aircraftSnapshot[i].typeCode.available() ? aircraftSnapshot[i].typeCode.value : "");
          copyText(state.aircraft[i].airline, sizeof(state.aircraft[i].airline),
                   aircraftSnapshot[i].airlineName.available() ? aircraftSnapshot[i].airlineName.value : "");
          copyText(state.aircraft[i].operatorName, sizeof(state.aircraft[i].operatorName),
                   aircraftSnapshot[i].operatorName.available() ? aircraftSnapshot[i].operatorName.value : "");
          copyText(state.aircraft[i].modelName, sizeof(state.aircraft[i].modelName),
                   aircraftSnapshot[i].modelName.available() ? aircraftSnapshot[i].modelName.value : "");
          copyText(state.aircraft[i].routeOrigin, sizeof(state.aircraft[i].routeOrigin),
                   aircraftSnapshot[i].routeOrigin.available() ? aircraftSnapshot[i].routeOrigin.value : "");
          copyText(state.aircraft[i].routeDestination, sizeof(state.aircraft[i].routeDestination),
                   aircraftSnapshot[i].routeDestination.available() ? aircraftSnapshot[i].routeDestination.value : "");
          copyText(state.aircraft[i].squawk, sizeof(state.aircraft[i].squawk),
                   aircraftSnapshot[i].squawk.available() ? aircraftSnapshot[i].squawk.value : "");
          copyText(state.aircraft[i].emergency, sizeof(state.aircraft[i].emergency),
                   aircraftSnapshot[i].emergency.available() ? aircraftSnapshot[i].emergency.value : "");
          state.aircraft[i].iconType = classifyAircraftIcon(aircraftSnapshot[i]);
          if (aircraftSnapshot[i].headingDeg.available()) {
            state.aircraft[i].headingDeg = static_cast<int16_t>(aircraftSnapshot[i].headingDeg.value);
          } else if (aircraftSnapshot[i].trackDeg.available()) {
            state.aircraft[i].headingDeg = static_cast<int16_t>(aircraftSnapshot[i].trackDeg.value);
          }
          state.aircraft[i].stale = aircraftSnapshot[i].stale;
          state.aircraft[i].selected = aircraftSnapshot[i].selected;
          state.aircraft[i].tracked = aircraftSnapshot[i].tracked;
          state.aircraft[i].emergencyActive = isEmergencyAircraft(aircraftSnapshot[i]);
          if (detailOpen_ && aircraftSnapshot[i].selected) selectedFound = true;
        }
      for (uint16_t i = state.aircraftRendered; i < MICRO_RADAR_MAX_DISPLAY_AIRCRAFT; ++i) {
        memset(&state.aircraft[i], 0, sizeof(state.aircraft[i]));
      }
      if (detailOpen_ && !selectedFound) detailOpen_ = false;
      state.detailOpen = detailOpen_;
    }
    state.wifiConnected = WiFi.status() == WL_CONNECTED;
    state.providerOk = state.aircraftReceived > 0;
    microRadarSmokeSetState(state);
    microRadarSmokeTick();
  }
  lastFrameMs_ = millis();
}

void RadarUi::invalidate() {
  (void)radar_;
}

void RadarUi::handleTouch(uint32_t now) {
  if (!radar_ || now - lastTouchPollMs_ < kTouchPollMs) return;
  lastTouchPollMs_ = now;
  uint16_t x = 0;
  uint16_t y = 0;
  const bool down = readTouchPoint(&x, &y);
  TouchPoint primary {0, 0};
  if (down) {
    primary = mapTouchPoint(x, y);
    primary = unrotateTouchPoint(primary, radar_->snapshotConfig().displayRotation);
  }
  if (down && !touchWasDown_) {
    touchDownMs_ = now;
    touchStartX_ = primary.x;
    touchStartY_ = primary.y;
    lastTouchX_ = primary.x;
    lastTouchY_ = primary.y;
    touchStartedInRange_ = inRangeHud(primary.x, primary.y);
    dragConsumed_ = false;
    Serial.printf("Touch raw=%u,%u mapped=%u,%u\n", x, y, primary.x, primary.y);
    if (!detailOpen_ && !touchStartedInRange_ && inCircle(primary.x, primary.y) && now - lastTouchActionMs_ > kTapDebounceMs) {
      if (openAircraftDetailAt(primary.x, primary.y, now)) {
        touchWasDown_ = down;
        return;
      }
    }
  } else if (down) {
    lastTouchX_ = primary.x;
    lastTouchY_ = primary.y;
  }
  if (down && touchWasDown_ && now - touchDownMs_ > kLongPressMs && now - lastTouchActionMs_ > 700) {
    if (detailOpen_ || !inRangeHud(primary.x, primary.y)) {
      touchWasDown_ = down;
      return;
    }
    radar_->previousRange();
    persistRadarConfig();
    lastTouchActionMs_ = now;
    dragConsumed_ = true;
  }
  if (!down && touchWasDown_) {
    const uint32_t heldMs = now - touchDownMs_;
    if (heldMs < kLongPressMs && !dragConsumed_) {
      if (detailOpen_) {
        if (touchInDetailDismiss(touchStartX_, touchStartY_) || touchInDetailDismiss(lastTouchX_, lastTouchY_)) {
          detailOpen_ = false;
          selectedIcao_ = "";
          radar_->clearSelection();
          lastTouchActionMs_ = now;
        }
      } else if (touchStartedInRange_ && inRangeHud(lastTouchX_, lastTouchY_) && now - lastTouchActionMs_ > kRangeDebounceMs) {
        radar_->nextRange();
        persistRadarConfig();
        lastTouchActionMs_ = now;
      } else if (!touchStartedInRange_ && inCircle(touchStartX_, touchStartY_) && now - lastTouchActionMs_ > kTapDebounceMs) {
        openAircraftDetailAt(touchStartX_, touchStartY_, now);
      }
    }
    touchStartedInRange_ = false;
  }
  touchWasDown_ = down;
}

bool RadarUi::touchInDetailDismiss(uint16_t x, uint16_t y) const {
  return x >= 132 && x <= 348 && y >= 398 && y <= 456;
}

bool RadarUi::openAircraftDetailAt(uint16_t x, uint16_t y, uint32_t now) {
  const bool selected = radar_->selectNearestDisplayPoint(static_cast<int16_t>(x) - kScreenCenter, static_cast<int16_t>(y) - kScreenCenter);
  if (!selected) {
    Serial.printf("Aircraft tap miss: screen=%u,%u radar=%d,%d\n", x, y, static_cast<int>(x) - kScreenCenter, static_cast<int>(y) - kScreenCenter);
    return false;
  }
  static std::vector<Aircraft> selectedSnapshot;
  radar_->snapshotVisibleAircraft(&selectedSnapshot);
  selectedIcao_ = "";
  for (const Aircraft& ac : selectedSnapshot) {
    if (ac.selected) {
      selectedIcao_ = ac.icao24;
      break;
    }
  }
  detailOpen_ = true;
  lastTouchActionMs_ = now;
  Serial.printf("Aircraft detail open: %s at screen=%u,%u\n", selectedIcao_.c_str(), x, y);
  return true;
}

void RadarUi::persistRadarConfig() {
  if (!config_ || !radar_) return;
  AppConfig cfg = config_->get();
  AppConfig runtime = radar_->snapshotConfig();
  cfg.rangeNm = runtime.rangeNm;
  cfg.mapEnabled = runtime.mapEnabled;
  config_->saveAtomic(cfg);
}

}  // namespace micro_radar
