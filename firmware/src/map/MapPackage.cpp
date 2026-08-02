#include "map/MapPackage.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <math.h>

namespace micro_radar {
namespace {
constexpr uint8_t kMapCoast = 1;
constexpr uint8_t kMapBorder = 2;
constexpr uint8_t kMapReference = 3;
constexpr uint8_t kMapRoadMajor = 4;
constexpr uint8_t kMapRoadMinor = 5;
constexpr uint8_t kMapWater = 6;
constexpr uint8_t kMapAirport = 10;
constexpr const char* kMapJsonPath = "/map_active.json";
constexpr uint8_t kDynamicMapSchemaVersion = 14;

const MapLineSegment kEastMedSegments[] = {
    // Eastern Mediterranean fallback. Dense enough to avoid a straight-line coast when Overpass returns sparse data.
    {{33.27, 35.11}, {33.20, 35.10}, kMapCoast}, {{33.20, 35.10}, {33.12, 35.10}, kMapCoast},
    {{33.12, 35.10}, {33.04, 35.10}, kMapCoast}, {{33.04, 35.10}, {32.97, 35.08}, kMapCoast},
    {{32.97, 35.08}, {32.91, 35.06}, kMapCoast}, {{32.91, 35.06}, {32.84, 35.04}, kMapCoast},
    {{32.84, 35.04}, {32.78, 35.02}, kMapCoast}, {{32.78, 35.02}, {32.72, 34.99}, kMapCoast},
    {{32.72, 34.99}, {32.66, 34.96}, kMapCoast}, {{32.66, 34.96}, {32.59, 34.93}, kMapCoast},
    {{32.59, 34.93}, {32.52, 34.91}, kMapCoast}, {{32.52, 34.91}, {32.45, 34.89}, kMapCoast},
    {{32.45, 34.89}, {32.38, 34.87}, kMapCoast}, {{32.38, 34.87}, {32.32, 34.85}, kMapCoast},
    {{32.32, 34.85}, {32.25, 34.82}, kMapCoast}, {{32.25, 34.82}, {32.18, 34.79}, kMapCoast},
    {{32.18, 34.79}, {32.10, 34.76}, kMapCoast}, {{32.10, 34.76}, {32.03, 34.74}, kMapCoast},
    {{32.03, 34.74}, {31.96, 34.72}, kMapCoast}, {{31.96, 34.72}, {31.88, 34.69}, kMapCoast},
    {{31.88, 34.69}, {31.80, 34.66}, kMapCoast}, {{31.80, 34.66}, {31.72, 34.62}, kMapCoast},
    {{31.72, 34.62}, {31.64, 34.58}, kMapCoast}, {{31.64, 34.58}, {31.56, 34.53}, kMapCoast},
    {{31.56, 34.53}, {31.48, 34.48}, kMapCoast}, {{31.48, 34.48}, {31.39, 34.43}, kMapCoast},
    {{31.39, 34.43}, {31.31, 34.37}, kMapCoast}, {{31.31, 34.37}, {31.23, 34.30}, kMapCoast},
    {{31.23, 34.30}, {31.15, 34.22}, kMapCoast}, {{31.15, 34.22}, {31.06, 34.13}, kMapCoast},
    {{31.06, 34.13}, {30.96, 34.04}, kMapCoast}, {{30.96, 34.04}, {30.86, 33.94}, kMapCoast},
    {{30.86, 33.94}, {30.76, 33.83}, kMapCoast}, {{30.76, 33.83}, {30.66, 33.70}, kMapCoast},
    {{30.66, 33.70}, {30.56, 33.55}, kMapCoast}, {{30.56, 33.55}, {30.47, 33.39}, kMapCoast},
    {{30.47, 33.39}, {30.39, 33.22}, kMapCoast}, {{30.39, 33.22}, {30.31, 33.03}, kMapCoast},
    {{30.31, 33.03}, {30.22, 32.84}, kMapCoast}, {{30.22, 32.84}, {30.12, 32.64}, kMapCoast},
    {{33.90, 35.48}, {34.15, 35.63}, kMapCoast}, {{34.15, 35.63}, {34.40, 35.75}, kMapCoast},
    {{34.40, 35.75}, {34.68, 35.84}, kMapCoast}, {{34.68, 35.84}, {34.95, 35.88}, kMapCoast},
    {{34.95, 35.88}, {35.22, 35.91}, kMapCoast}, {{35.22, 35.91}, {35.54, 35.91}, kMapCoast},
    {{35.54, 35.91}, {35.86, 35.88}, kMapCoast}, {{35.86, 35.88}, {36.18, 35.82}, kMapCoast},
    {{36.18, 35.82}, {36.50, 35.75}, kMapCoast}, {{36.50, 35.75}, {36.82, 35.67}, kMapCoast},
    {{33.27, 35.11}, {33.09, 35.28}, kMapBorder}, {{33.09, 35.28}, {32.92, 35.43}, kMapBorder},
    {{32.92, 35.43}, {32.72, 35.57}, kMapBorder}, {{32.72, 35.57}, {32.45, 35.56}, kMapBorder},
    {{32.45, 35.56}, {32.14, 35.54}, kMapBorder}, {{32.14, 35.54}, {31.86, 35.48}, kMapBorder},
    {{31.86, 35.48}, {31.58, 35.36}, kMapBorder}, {{31.58, 35.36}, {31.35, 35.18}, kMapBorder},
    {{31.35, 35.18}, {31.19, 34.96}, kMapBorder}, {{31.19, 34.96}, {30.92, 34.90}, kMapBorder},
    {{30.92, 34.90}, {30.55, 34.88}, kMapBorder}, {{30.55, 34.88}, {30.20, 34.92}, kMapBorder},
    {{30.20, 34.92}, {29.86, 34.98}, kMapBorder}, {{29.86, 34.98}, {29.55, 34.95}, kMapBorder},
    {{32.08, 34.78}, {32.02, 34.86}, kMapReference}, {{32.02, 34.86}, {31.96, 34.94}, kMapReference},
    {{31.25, 34.80}, {31.30, 34.94}, kMapReference}, {{31.30, 34.94}, {31.35, 35.10}, kMapReference},
    {{32.48, 34.90}, {32.44, 35.02}, kMapReference}, {{32.44, 35.02}, {32.38, 35.16}, kMapReference},

    // Central Israel high-detail seed for the common LLBG/Tel Aviv operating area.
    {{32.52, 34.91}, {32.50, 34.905}, kMapCoast}, {{32.50, 34.905}, {32.47, 34.895}, kMapCoast},
    {{32.47, 34.895}, {32.44, 34.888}, kMapCoast}, {{32.44, 34.888}, {32.41, 34.878}, kMapCoast},
    {{32.41, 34.878}, {32.385, 34.872}, kMapCoast}, {{32.385, 34.872}, {32.36, 34.864}, kMapCoast},
    {{32.36, 34.864}, {32.335, 34.855}, kMapCoast}, {{32.335, 34.855}, {32.31, 34.846}, kMapCoast},
    {{32.31, 34.846}, {32.285, 34.835}, kMapCoast}, {{32.285, 34.835}, {32.265, 34.827}, kMapCoast},
    {{32.265, 34.827}, {32.245, 34.817}, kMapCoast}, {{32.245, 34.817}, {32.225, 34.807}, kMapCoast},
    {{32.225, 34.807}, {32.205, 34.798}, kMapCoast}, {{32.205, 34.798}, {32.185, 34.790}, kMapCoast},
    {{32.185, 34.790}, {32.165, 34.782}, kMapCoast}, {{32.165, 34.782}, {32.145, 34.774}, kMapCoast},
    {{32.145, 34.774}, {32.125, 34.767}, kMapCoast}, {{32.125, 34.767}, {32.105, 34.760}, kMapCoast},
    {{32.105, 34.760}, {32.085, 34.754}, kMapCoast}, {{32.085, 34.754}, {32.065, 34.748}, kMapCoast},
    {{32.065, 34.748}, {32.045, 34.742}, kMapCoast}, {{32.045, 34.742}, {32.025, 34.736}, kMapCoast},
    {{32.025, 34.736}, {32.005, 34.731}, kMapCoast}, {{32.005, 34.731}, {31.985, 34.724}, kMapCoast},
    {{31.985, 34.724}, {31.965, 34.718}, kMapCoast}, {{31.965, 34.718}, {31.945, 34.711}, kMapCoast},
    {{31.945, 34.711}, {31.925, 34.704}, kMapCoast}, {{31.925, 34.704}, {31.905, 34.697}, kMapCoast},
    {{31.905, 34.697}, {31.885, 34.690}, kMapCoast}, {{31.885, 34.690}, {31.865, 34.683}, kMapCoast},
    {{31.865, 34.683}, {31.845, 34.675}, kMapCoast}, {{31.845, 34.675}, {31.825, 34.668}, kMapCoast},

    {{32.47, 34.96}, {32.43, 34.99}, kMapReference}, {{32.43, 34.99}, {32.39, 35.02}, kMapReference},
    {{32.39, 35.02}, {32.35, 35.055}, kMapReference}, {{32.35, 35.055}, {32.31, 35.09}, kMapReference},
    {{32.31, 35.09}, {32.27, 35.125}, kMapReference}, {{32.27, 35.125}, {32.23, 35.16}, kMapReference},
    {{32.23, 35.16}, {32.19, 35.19}, kMapReference}, {{32.19, 35.19}, {32.15, 35.22}, kMapReference},
    {{32.40, 34.78}, {32.36, 34.83}, kMapReference}, {{32.36, 34.83}, {32.31, 34.88}, kMapReference},
    {{32.31, 34.88}, {32.26, 34.93}, kMapReference}, {{32.26, 34.93}, {32.21, 34.98}, kMapReference},
    {{32.21, 34.98}, {32.16, 35.03}, kMapReference}, {{32.16, 35.03}, {32.11, 35.08}, kMapReference},
    {{32.11, 35.08}, {32.06, 35.13}, kMapReference}, {{32.06, 35.13}, {32.01, 35.18}, kMapReference},
    {{31.96, 34.78}, {32.01, 34.82}, kMapReference}, {{32.01, 34.82}, {32.06, 34.86}, kMapReference},
    {{32.06, 34.86}, {32.11, 34.90}, kMapReference}, {{32.11, 34.90}, {32.16, 34.94}, kMapReference},
    {{32.16, 34.94}, {32.21, 34.98}, kMapReference}, {{32.21, 34.98}, {32.26, 35.02}, kMapReference},
    {{32.26, 35.02}, {32.31, 35.06}, kMapReference}, {{32.31, 35.06}, {32.36, 35.10}, kMapReference},

    {{32.08, 34.79}, {32.095, 34.81}, kMapWater}, {{32.095, 34.81}, {32.11, 34.835}, kMapWater},
    {{32.11, 34.835}, {32.12, 34.86}, kMapWater}, {{32.12, 34.86}, {32.135, 34.885}, kMapWater},
    {{32.135, 34.885}, {32.15, 34.91}, kMapWater}, {{32.15, 34.91}, {32.165, 34.935}, kMapWater},
    {{32.165, 34.935}, {32.18, 34.96}, kMapWater}, {{32.18, 34.96}, {32.195, 34.985}, kMapWater},
    {{32.195, 34.985}, {32.21, 35.01}, kMapWater},
};

const MapPoint kEastMedAirports[] = {
    {{32.0003, 34.8707}, kMapAirport, "LLBG"},  // Ben Gurion
    {{32.1806, 34.8347}, kMapAirport, "LLHZ"},  // Herzliya
    {{31.8647, 34.7228}, kMapAirport, "LL59"},  // Palmachim area
    {{32.4408, 34.9122}, kMapAirport, "LLES"},  // Ein Shemer area
};

bool isExcludedAirportCode(const String& code) {
  return code == "LLSD";
}

bool inEastMed(double lat, double lon) {
  return lat > 28.0 && lat < 37.5 && lon > 30.0 && lon < 37.5;
}

double clampDouble(double value, double lo, double hi) {
  return value < lo ? lo : value > hi ? hi : value;
}

String uppercaseCode(String value) {
  value.trim();
  value.replace("-", "");
  value.replace("_", "");
  value.replace(" ", "");
  if (value.length() > 5) value = value.substring(0, 5);
  value.toUpperCase();
  return value;
}

String airportCodeFromTags(JsonObject tags) {
  String code = tags["icao"] | "";
  if (code.isEmpty()) code = tags["iata"] | "";
  if (code.isEmpty()) code = tags["ref"] | "";
  return uppercaseCode(code);
}

bool hasAirportPoint(const std::vector<MapPoint>& points, const String& code) {
  if (code.isEmpty()) return false;
  for (const MapPoint& point : points) {
    if (point.kind == kMapAirport && point.code == code) return true;
  }
  return false;
}

void mergeEastMedSeedAirports(std::vector<MapPoint>* points) {
  if (!points) return;
  for (const MapPoint& point : kEastMedAirports) {
    if (isExcludedAirportCode(point.code) || hasAirportPoint(*points, point.code)) continue;
    points->push_back(point);
  }
}

}  // namespace

bool MapPackageManager::begin() {
  active_ = loadActivePackage();
  rasterActive_ = false;
  status_ = active_ ? "active dynamic package" : "radar-only: no active package";
  Serial.printf("Map begin: active=%d center=%.6f,%.6f range=%u schema=%u\n", active_, activeCenterLat_,
                activeCenterLon_, activeRadiusNm_, kDynamicMapSchemaVersion);
  return true;
}

bool MapPackageManager::hasActivePackage() const { return active_; }

bool MapPackageManager::hasRasterBackground() const { return false; }

bool MapPackageManager::isCurrentForLocation(double centerLat, double centerLon, uint16_t radiusNm) const {
  if (!active_) return false;
  if (radiusNm != activeRadiusNm_) return false;
  return fabs(centerLat - activeCenterLat_) < 0.000001 && fabs(centerLon - activeCenterLon_) < 0.000001;
}

String MapPackageManager::statusText() const { return status_; }

bool MapPackageManager::validatePackage(const String& manifestJson, const String& payloadPath, String* error) {
  JsonDocument doc;
  if (deserializeJson(doc, manifestJson) != DeserializationError::Ok) {
    if (error) *error = "Invalid map manifest JSON";
    return false;
  }
  if (!doc["packageId"].is<const char*>() || !doc["version"].is<const char*>() || !doc["checksumSha256"].is<const char*>()) {
    if (error) *error = "Map manifest missing required fields";
    return false;
  }
  if (!LittleFS.exists(payloadPath)) {
    if (error) *error = "Map payload missing";
    return false;
  }
  status_ = "validated package " + String(doc["packageId"].as<const char*>());
  return true;
}

bool MapPackageManager::activateValidatedPackage(const String&) {
  active_ = true;
  status_ = "active";
  return true;
}

bool MapPackageManager::retryDownloadForLocation(double centerLat, double centerLon, uint16_t radiusNm) {
  if (inEastMed(centerLat, centerLon)) {
    Serial.printf("Map fetch skipped: using curated EastMed vector seed for %.6f, %.6f, %u NM\n", centerLat, centerLon, radiusNm);
    return ensureLocalSeedForLocation(centerLat, centerLon, radiusNm);
  }

  const uint32_t queryRadiusMeters = static_cast<uint32_t>(std::min(220000.0, std::max(30000.0, radiusNm * 1852.0 * 1.15)));
  Serial.printf("Map fetch: %.6f, %.6f, %u NM (%lu m)\n", centerLat, centerLon, radiusNm, static_cast<unsigned long>(queryRadiusMeters));
  String query = "[out:json][timeout:18];(";
  query += "way[\"natural\"=\"coastline\"](around:" + String(queryRadiusMeters) + "," + String(centerLat, 6) + "," + String(centerLon, 6) + ");";
  query += "way[\"boundary\"=\"administrative\"][\"admin_level\"~\"2|4\"](around:" + String(queryRadiusMeters) + "," + String(centerLat, 6) + "," + String(centerLon, 6) + ");";
  query += "node[\"aeroway\"=\"aerodrome\"](around:" + String(queryRadiusMeters) + "," + String(centerLat, 6) + "," + String(centerLon, 6) + ");";
  query += "way[\"aeroway\"=\"aerodrome\"](around:" + String(queryRadiusMeters) + "," + String(centerLat, 6) + "," + String(centerLon, 6) + ");";
  query += "relation[\"aeroway\"=\"aerodrome\"](around:" + String(queryRadiusMeters) + "," + String(centerLat, 6) + "," + String(centerLon, 6) + ");";
  query += ");out center geom;";

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(30000);
  if (!http.begin(client, "http://overpass-api.de/api/interpreter")) {
    status_ = "map fetch failed: http begin";
    Serial.println(status_);
    return false;
  }
  http.addHeader("Content-Type", "text/plain");
  http.addHeader("User-Agent", "AeroScope/0.1 map downloader");
  const int code = http.POST(query);
  Serial.printf("Map fetch HTTP: %d\n", code);
  if (code != 200) {
    status_ = "map fetch failed: HTTP " + String(code);
    Serial.println(status_);
    http.end();
    return false;
  }
  JsonDocument filter;
  JsonObject elementFilter = filter["elements"].add<JsonObject>();
  elementFilter["type"] = true;
  elementFilter["lat"] = true;
  elementFilter["lon"] = true;
  elementFilter["center"]["lat"] = true;
  elementFilter["center"]["lon"] = true;
  elementFilter["tags"]["natural"] = true;
  elementFilter["tags"]["boundary"] = true;
  elementFilter["tags"]["admin_level"] = true;
  elementFilter["tags"]["aeroway"] = true;
  elementFilter["tags"]["icao"] = true;
  elementFilter["tags"]["iata"] = true;
  elementFilter["tags"]["ref"] = true;
  JsonObject geometryFilter = elementFilter["geometry"].add<JsonObject>();
  geometryFilter["lat"] = true;
  geometryFilter["lon"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    status_ = "map parse failed: " + String(err.c_str());
    Serial.println(status_);
    return false;
  }

  segments_.clear();
  points_.clear();
  rasterActive_ = false;
  JsonArray elements = doc["elements"].as<JsonArray>();
  for (JsonObject element : elements) {
    if (element["tags"]["aeroway"] == "aerodrome") {
      const String code = airportCodeFromTags(element["tags"].as<JsonObject>());
      if (isExcludedAirportCode(code)) continue;
      if (element["lat"].is<double>() && element["lon"].is<double>()) {
        points_.push_back({{element["lat"].as<double>(), element["lon"].as<double>()}, kMapAirport, code});
      } else if (element["center"]["lat"].is<double>() && element["center"]["lon"].is<double>()) {
        points_.push_back({{element["center"]["lat"].as<double>(), element["center"]["lon"].as<double>()}, kMapAirport, code});
      }
      continue;
    }
  }

  for (JsonObject element : elements) {
    if (element["tags"]["aeroway"] == "aerodrome") continue;
    uint8_t kind = kMapBorder;
    if (element["tags"]["natural"] == "coastline") {
      kind = kMapCoast;
    } else if (element["tags"]["natural"] == "water" || element["tags"]["water"].is<const char*>()) {
      kind = kMapWater;
    }
    JsonArray geometry = element["geometry"].as<JsonArray>();
    if (geometry.size() < 2) continue;
    geo::Location prev {};
    bool havePrev = false;
    uint8_t skip = 0;
    for (JsonObject node : geometry) {
      if (!node["lat"].is<double>() || !node["lon"].is<double>()) continue;
      ++skip;
      geo::Location current {node["lat"].as<double>(), node["lon"].as<double>()};
      if (havePrev) segments_.push_back({prev, current, kind});
      prev = current;
      havePrev = true;
      if (segments_.size() >= 3600) break;
    }
    if (segments_.size() >= 3600) break;
  }

  if (segments_.size() < 120 || points_.empty()) {
    Serial.printf("Map sparse: %u segments, %u points; using enhanced regional fallback\n", static_cast<unsigned>(segments_.size()),
                  static_cast<unsigned>(points_.size()));
    segments_.clear();
    points_.clear();
    loadEastMedSeedIfApplicable(centerLat, centerLon);
  } else if (inEastMed(centerLat, centerLon)) {
    mergeEastMedSeedAirports(&points_);
  }
  Serial.printf("Map parsed: %u segments, %u airport points\n", static_cast<unsigned>(segments_.size()), static_cast<unsigned>(points_.size()));
  return saveActivePackage(centerLat, centerLon, radiusNm);
}

bool MapPackageManager::ensureLocalSeedForLocation(double centerLat, double centerLon, uint16_t radiusNm) {
  if (!inEastMed(centerLat, centerLon)) return false;
  if (isCurrentForLocation(centerLat, centerLon, radiusNm) && (!segments_.empty() || !points_.empty())) return true;
  segments_.clear();
  points_.clear();
  rasterActive_ = false;
  loadEastMedSeedIfApplicable(centerLat, centerLon);
  return saveActivePackage(centerLat, centerLon, radiusNm);
}

bool MapPackageManager::saveActivePackage(double centerLat, double centerLon, uint16_t radiusNm) {
  File mf = LittleFS.open(kMapJsonPath, "w");
  if (!mf) {
    status_ = "map save failed: storage";
    return false;
  }
  JsonDocument doc;
  doc["schemaVersion"] = kDynamicMapSchemaVersion;
  doc["packageId"] = "dynamic-overpass";
  doc["version"] = "1";
  doc["centerLat"] = centerLat;
  doc["centerLon"] = centerLon;
  doc["radiusNm"] = radiusNm;
  doc["attribution"] = "Map data © OpenStreetMap contributors; airports from OSM aeroway data";
  JsonArray segs = doc["segments"].to<JsonArray>();
  for (const auto& s : segments_) {
    JsonArray row = segs.add<JsonArray>();
    row.add(s.a.lat);
    row.add(s.a.lon);
    row.add(s.b.lat);
    row.add(s.b.lon);
    row.add(s.kind);
  }
  JsonArray pts = doc["points"].to<JsonArray>();
  for (const auto& p : points_) {
    JsonArray row = pts.add<JsonArray>();
    row.add(p.location.lat);
    row.add(p.location.lon);
    row.add(p.kind);
    row.add(p.code);
  }
  serializeJson(doc, mf);
  mf.close();
  active_ = true;
  rasterActive_ = false;
  activeCenterLat_ = centerLat;
  activeCenterLon_ = centerLon;
  activeRadiusNm_ = radiusNm;
  packageId_ = "dynamic-overpass";
  version_ = "1";
  attribution_ = "Map data © OpenStreetMap contributors; airports from OSM aeroway data";
  status_ = "active dynamic map: " + String(segments_.size()) + " segments, " + String(points_.size()) + " points";
  Serial.println("Map save: " + status_);
  return true;
}

bool MapPackageManager::deleteCache() {
  LittleFS.remove("/maps/active.mrmap");
  LittleFS.remove("/maps/active.json");
  LittleFS.remove("/maps/background.rgb565");
  LittleFS.remove(kMapJsonPath);
  active_ = false;
  rasterActive_ = false;
  activeCenterLat_ = 999.0;
  activeCenterLon_ = 999.0;
  activeRadiusNm_ = 0;
  status_ = "radar-only: cache deleted";
  packageId_ = "";
  version_ = "";
  attribution_ = "";
  segments_.clear();
  points_.clear();
  return true;
}

String MapPackageManager::statusJson() const {
  JsonDocument doc;
  doc["active"] = active_;
  doc["status"] = status_;
  doc["packageId"] = packageId_;
  doc["version"] = version_;
  doc["attribution"] = attribution_;
  doc["segments"] = segments_.size();
  doc["points"] = points_.size();
  doc["raster"] = rasterActive_;
  doc["centerLat"] = activeCenterLat_;
  doc["centerLon"] = activeCenterLon_;
  doc["radiusNm"] = activeRadiusNm_;
  String out;
  serializeJson(doc, out);
  return out;
}

std::vector<ProjectedMapSegment> MapPackageManager::projectSegments(geo::Location origin, uint16_t rangeNm, size_t maxSegments) const {
  std::vector<ProjectedMapSegment> out;
  if (!active_) return out;
  out.reserve(std::min(maxSegments, segments_.size()));
  const uint8_t passes[] = {kMapCoast, kMapBorder, kMapWater, kMapReference};
  for (uint8_t kind : passes) {
    for (const MapLineSegment& segment : segments_) {
      if (out.size() >= maxSegments) break;
      if (segment.kind != kind) continue;
      const double da = geo::distanceNm(origin, segment.a);
      const double db = geo::distanceNm(origin, segment.b);
      if (da > rangeNm * 1.4 && db > rangeNm * 1.4) continue;
      const auto a = geo::projectAzimuthalEquidistant(origin, segment.a, rangeNm, 232);
      const auto b = geo::projectAzimuthalEquidistant(origin, segment.b, rangeNm, 232);
      out.push_back({static_cast<int16_t>(a.x), static_cast<int16_t>(a.y), static_cast<int16_t>(b.x), static_cast<int16_t>(b.y), segment.kind});
    }
  }
  return out;
}

std::vector<ProjectedMapPoint> MapPackageManager::projectPoints(geo::Location origin, uint16_t rangeNm, size_t maxPoints) const {
  std::vector<ProjectedMapPoint> out;
  if (!active_) return out;
  out.reserve(std::min(maxPoints, points_.size()));
  for (const MapPoint& point : points_) {
    if (out.size() >= maxPoints) break;
    if (geo::distanceNm(origin, point.location) > rangeNm) continue;
    const auto p = geo::projectAzimuthalEquidistant(origin, point.location, rangeNm, 232);
    out.push_back({static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), point.kind, point.code});
  }
  return out;
}

bool MapPackageManager::loadActivePackage() {
  segments_.clear();
  points_.clear();
  if (!LittleFS.exists(kMapJsonPath)) return false;
  File file = LittleFS.open(kMapJsonPath, "r");
  if (!file) return false;
  JsonDocument doc;
  if (deserializeJson(doc, file)) return false;
  if ((doc["schemaVersion"] | 0) < kDynamicMapSchemaVersion) {
    status_ = "radar-only: cached map schema is old";
    file.close();
    LittleFS.remove(kMapJsonPath);
    return false;
  }
  for (JsonArray row : doc["segments"].as<JsonArray>()) {
    if (row.size() < 5) continue;
    segments_.push_back({{row[0].as<double>(), row[1].as<double>()}, {row[2].as<double>(), row[3].as<double>()}, row[4].as<uint8_t>()});
  }
  for (JsonArray row : doc["points"].as<JsonArray>()) {
    if (row.size() < 3) continue;
    const String code = row.size() > 3 ? row[3].as<String>() : "";
    if (isExcludedAirportCode(code)) continue;
    points_.push_back({{row[0].as<double>(), row[1].as<double>()}, row[2].as<uint8_t>(), code});
  }
  packageId_ = doc["packageId"] | "dynamic-overpass";
  version_ = doc["version"] | "1";
  activeCenterLat_ = doc["centerLat"] | 999.0;
  activeCenterLon_ = doc["centerLon"] | 999.0;
  activeRadiusNm_ = doc["radiusNm"] | 0;
  if (inEastMed(activeCenterLat_, activeCenterLon_)) mergeEastMedSeedAirports(&points_);
  attribution_ = doc["attribution"] | "Map data © OpenStreetMap contributors";
  rasterActive_ = false;
  return !segments_.empty() || !points_.empty();
}

void MapPackageManager::loadEastMedSeedIfApplicable(double centerLat, double centerLon) {
  if (!inEastMed(centerLat, centerLon)) return;
  for (const MapLineSegment& segment : kEastMedSegments) segments_.push_back(segment);
  mergeEastMedSeedAirports(&points_);
  Serial.printf("Map fallback seed loaded: %u segments, %u points\n", static_cast<unsigned>(segments_.size()),
                static_cast<unsigned>(points_.size()));
}

}  // namespace micro_radar
