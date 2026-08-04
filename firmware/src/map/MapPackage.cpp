#include "map/MapPackage.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <math.h>

#include "third_party/lodepng/lodepng.h"

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
constexpr uint16_t kStaticMapWidth = 480;
constexpr uint16_t kStaticMapHeight = 480;
constexpr uint8_t kMaxFetchedAirportPoints = 24;

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

String formEncode(const String& value) {
  String encoded;
  const char* hex = "0123456789ABCDEF";
  encoded.reserve(value.length() + 32);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else if (c == ' ') {
      encoded += '+';
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

bool readHttpPayloadBounded(HTTPClient& http, String* payload, uint32_t maxBytes, uint32_t totalTimeoutMs, uint32_t idleTimeoutMs) {
  if (!payload) return false;
  payload->clear();
  payload->reserve(std::min<uint32_t>(maxBytes, 49152));
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return false;
  const int contentLength = http.getSize();
  if (contentLength > 0 && static_cast<uint32_t>(contentLength) > maxBytes) {
    Serial.printf("Map fetch payload too large: expected=%d max=%u\n", contentLength, static_cast<unsigned>(maxBytes));
    return false;
  }
  const uint32_t startMs = millis();
  uint32_t lastDataMs = millis();
  uint8_t buffer[384];
  while (millis() - startMs < totalTimeoutMs) {
    const int available = stream->available();
    if (available > 0) {
      const size_t toRead = std::min<size_t>(sizeof(buffer), available);
      const int readCount = stream->readBytes(buffer, toRead);
      if (readCount > 0) {
        if (payload->length() + static_cast<size_t>(readCount) > maxBytes) {
          Serial.printf("Map fetch payload too large: current=%u next=%d max=%u\n", static_cast<unsigned>(payload->length()), readCount,
                        static_cast<unsigned>(maxBytes));
          return false;
        }
        payload->concat(reinterpret_cast<const char*>(buffer), readCount);
        lastDataMs = millis();
        if (contentLength > 0 && payload->length() >= static_cast<size_t>(contentLength)) return true;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    if (!stream->connected()) return payload->length() > 0 && (contentLength <= 0 || payload->length() == static_cast<size_t>(contentLength));
    if (payload->length() > 0 && contentLength <= 0 && millis() - lastDataMs > idleTimeoutMs) return true;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  Serial.printf("Map fetch read timeout: payload=%u expected=%d\n", static_cast<unsigned>(payload->length()), contentLength);
  return payload->length() > 0 && contentLength <= 0;
}

bool decodeChunkedTextPayload(String* payload) {
  if (!payload || payload->isEmpty()) return false;
  size_t lineEnd = payload->indexOf("\r\n");
  size_t newlineLen = 2;
  if (lineEnd == static_cast<size_t>(-1)) {
    lineEnd = payload->indexOf('\n');
    newlineLen = 1;
  }
  if (lineEnd == static_cast<size_t>(-1) || lineEnd > 12) return true;
  String firstLine = payload->substring(0, lineEnd);
  firstLine.trim();
  if (firstLine.isEmpty()) return true;
  for (size_t i = 0; i < firstLine.length(); ++i) {
    const char c = firstLine[i];
    if (c == ';') break;
    const bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!isHex) return true;
  }

  String decoded;
  decoded.reserve(payload->length());
  size_t pos = 0;
  while (pos < payload->length()) {
    size_t end = payload->indexOf("\r\n", pos);
    size_t nl = 2;
    if (end == static_cast<size_t>(-1)) {
      end = payload->indexOf('\n', pos);
      nl = 1;
    }
    if (end == static_cast<size_t>(-1)) return false;
    String sizeLine = payload->substring(pos, end);
    sizeLine.trim();
    const int semi = sizeLine.indexOf(';');
    if (semi >= 0) sizeLine = sizeLine.substring(0, semi);
    char* parseEnd = nullptr;
    const unsigned long chunkSize = strtoul(sizeLine.c_str(), &parseEnd, 16);
    if (!parseEnd || *parseEnd != '\0') return false;
    pos = end + nl;
    if (chunkSize == 0) {
      *payload = decoded;
      return true;
    }
    if (pos + chunkSize > payload->length()) return false;
    decoded.concat(payload->substring(pos, pos + chunkSize));
    pos += chunkSize;
    if (payload->startsWith("\r\n", pos)) pos += 2;
    else if (pos < payload->length() && (*payload)[pos] == '\n') ++pos;
  }
  return false;
}

bool readHttpPayloadBytesBounded(HTTPClient& http, uint8_t** payload, size_t* payloadLength, uint32_t maxBytes, uint32_t totalTimeoutMs,
                                 uint32_t idleTimeoutMs) {
  if (!payload || !payloadLength) return false;
  *payload = nullptr;
  *payloadLength = 0;
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return false;
  const int contentLength = http.getSize();
  const size_t capacity = (contentLength > 0 && static_cast<uint32_t>(contentLength) <= maxBytes) ? static_cast<size_t>(contentLength) : maxBytes;
  uint8_t* out = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!out) out = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_8BIT));
  if (!out) {
    Serial.printf("Map binary payload alloc failed: capacity=%u\n", static_cast<unsigned>(capacity));
    return false;
  }
  const uint32_t startMs = millis();
  uint32_t lastDataMs = millis();
  while (millis() - startMs < totalTimeoutMs) {
    const int available = stream->available();
    if (available > 0) {
      const size_t remaining = capacity - *payloadLength;
      if (remaining == 0) {
        Serial.printf("Map binary payload too large: max=%u\n", static_cast<unsigned>(maxBytes));
        free(out);
        return false;
      }
      const size_t toRead = std::min<size_t>(std::min<size_t>(512, available), remaining);
      const int readCount = stream->readBytes(out + *payloadLength, toRead);
      if (readCount > 0) {
        *payloadLength += static_cast<size_t>(readCount);
        lastDataMs = millis();
        if (contentLength > 0 && *payloadLength >= static_cast<size_t>(contentLength)) {
          *payload = out;
          return true;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    if (!stream->connected()) {
      *payload = out;
      return *payloadLength > 0 && (contentLength <= 0 || *payloadLength == static_cast<size_t>(contentLength));
    }
    if (*payloadLength > 0 && millis() - lastDataMs > idleTimeoutMs) {
      *payload = out;
      return contentLength <= 0 || *payloadLength == static_cast<size_t>(contentLength);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  Serial.printf("Map binary read timeout: payload=%u expected=%d\n", static_cast<unsigned>(*payloadLength), contentLength);
  free(out);
  *payloadLength = 0;
  return false;
}

uint32_t fnv1a(const uint16_t* data, size_t count) {
  uint32_t h = 2166136261UL;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
  for (size_t i = 0; i < count * sizeof(uint16_t); ++i) {
    h ^= bytes[i];
    h *= 16777619UL;
  }
  return h;
}

uint8_t zoomForRange(double lat, uint16_t radiusNm) {
  const double mapMeters = std::max(9000.0, static_cast<double>(radiusNm) * 1852.0 * 2.15);
  const double metersPerPixelAtZ0 = 156543.03392 * std::max(0.18, cos(lat * M_PI / 180.0));
  const double zoom = log((metersPerPixelAtZ0 * kStaticMapWidth) / mapMeters) / log(2.0);
  return static_cast<uint8_t>(std::max(4.0, std::min(15.0, floor(zoom + 0.5))));
}

const char* geoapifyStyleParam(MapStyle style) {
  switch (style) {
    case MapStyle::OSMBrightGrey: return "osm-bright-grey";
    case MapStyle::OSMBright: return "osm-bright";
    case MapStyle::Positron: return "positron";
    case MapStyle::DarkMatter:
    default: return "dark-matter";
  }
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

bool MapPackageManager::hasRasterBackground() const { return rasterActive_ && rasterBackground_ != nullptr; }

const uint16_t* MapPackageManager::rasterBackground() const { return rasterActive_ ? rasterBackground_ : nullptr; }

uint32_t MapPackageManager::rasterBackgroundHash() const { return rasterActive_ ? rasterHash_ : 0; }

bool MapPackageManager::isRefreshing() const { return refreshing_; }

void MapPackageManager::setRefreshing(bool refreshing) { refreshing_ = refreshing; }

size_t MapPackageManager::pointCount() const { return points_.size(); }

bool MapPackageManager::refreshAirportPointsForLocation(double centerLat, double centerLon, uint16_t radiusNm, const String& geoapifyApiKey) {
  fetchGeoapifyAirports(centerLat, centerLon, radiusNm, geoapifyApiKey);
  return !points_.empty();
}

bool MapPackageManager::isCurrentForLocation(double centerLat, double centerLon, uint16_t radiusNm) const {
  if (!active_) return false;
  if (radiusNm != activeRadiusNm_) return false;
  return fabs(centerLat - activeCenterLat_) < 0.000001 && fabs(centerLon - activeCenterLon_) < 0.000001;
}

bool MapPackageManager::isCurrentForLocation(double centerLat, double centerLon, uint16_t radiusNm, MapStyle style) const {
  return isCurrentForLocation(centerLat, centerLon, radiusNm) && activeStyle_ == style;
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
  return retryDownloadForLocation(centerLat, centerLon, radiusNm, "", MapStyle::DarkMatter);
}

bool MapPackageManager::retryDownloadForLocation(double centerLat, double centerLon, uint16_t radiusNm, const String& geoapifyApiKey, MapStyle style) {
  if (geoapifyApiKey.isEmpty()) {
    status_ = "static map skipped: Geoapify API key not configured";
    Serial.println(status_);
    return false;
  }
  return fetchStaticRasterMap(centerLat, centerLon, radiusNm, geoapifyApiKey, style);
}

bool MapPackageManager::fetchStaticRasterMap(double centerLat, double centerLon, uint16_t radiusNm, const String& geoapifyApiKey, MapStyle style) {
  const uint8_t zoom = zoomForRange(centerLat, radiusNm);
  const char* styleParam = geoapifyStyleParam(style);
  String url = "https://maps.geoapify.com/v1/staticmap?style=" + String(styleParam);
  url += "&width=" + String(kStaticMapWidth);
  url += "&height=" + String(kStaticMapHeight);
  url += "&center=lonlat:" + String(centerLon, 6) + "," + String(centerLat, 6);
  url += "&zoom=" + String(zoom);
  url += "&format=png";
  url += "&scaleFactor=1";
  url += "&apiKey=" + formEncode(geoapifyApiKey);

  Serial.printf("Static map fetch: %.6f, %.6f, %u NM zoom=%u style=%s\n", centerLat, centerLon, radiusNm, zoom, styleParam);
  WiFiClientSecure* client = new WiFiClientSecure();
  HTTPClient* http = new HTTPClient();
  if (!client || !http) {
    delete http;
    delete client;
    status_ = "static map failed: no HTTP memory";
    Serial.println(status_);
    return false;
  }
  client->setInsecure();
  http->setTimeout(14000);
  http->setReuse(false);
  http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http->begin(*client, url)) {
    delete http;
    delete client;
    status_ = "static map failed: http begin";
    Serial.println(status_);
    return false;
  }
  http->addHeader("Accept", "image/png");
  http->addHeader("Accept-Encoding", "identity");
  http->addHeader("User-Agent", "DesktopAirspace/0.1 static map");
  const int code = http->GET();
  const int responseSize = http->getSize();
  const String responseType = http->header("Content-Type");
  Serial.printf("Static map HTTP: %d size=%d type=%s\n", code, responseSize, responseType.c_str());
  if (code != 200) {
    status_ = "static map failed: HTTP " + String(code);
    http->end();
    delete http;
    delete client;
    Serial.println(status_);
    return false;
  }
  uint8_t* payload = nullptr;
  size_t payloadLength = 0;
  const bool readOk = readHttpPayloadBytesBounded(*http, &payload, &payloadLength, 420000, 12000, 1200);
  http->end();
  delete http;
  delete client;
  Serial.printf("Static map payload: %u bytes read=%s sig=%02X %02X %02X %02X %02X %02X %02X %02X\n", static_cast<unsigned>(payloadLength),
                readOk ? "ok" : "failed", payloadLength > 0 ? payload[0] : 0, payloadLength > 1 ? payload[1] : 0,
                payloadLength > 2 ? payload[2] : 0, payloadLength > 3 ? payload[3] : 0, payloadLength > 4 ? payload[4] : 0,
                payloadLength > 5 ? payload[5] : 0, payloadLength > 6 ? payload[6] : 0, payloadLength > 7 ? payload[7] : 0);
  if (!readOk || payloadLength < 512) {
    status_ = "static map failed: short payload";
    Serial.println(status_);
    if (payload) free(payload);
    return false;
  }
  uint16_t* decoded = rasterBackground_;
  if (!decoded) {
    decoded = static_cast<uint16_t*>(heap_caps_malloc(kStaticMapWidth * kStaticMapHeight * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!decoded) decoded = static_cast<uint16_t*>(heap_caps_malloc(kStaticMapWidth * kStaticMapHeight * sizeof(uint16_t), MALLOC_CAP_8BIT));
  }
  if (!decoded) {
    status_ = "static map failed: no raster memory";
    Serial.println(status_);
    free(payload);
    return false;
  }
  unsigned char* rgb = nullptr;
  unsigned width = 0;
  unsigned height = 0;
  LodePNGState pngState;
  lodepng_state_init(&pngState);
  pngState.info_raw.colortype = LCT_RGB;
  pngState.info_raw.bitdepth = 8;
  pngState.decoder.ignore_crc = 1;
  pngState.decoder.zlibsettings.ignore_adler32 = 1;
#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS
  pngState.decoder.read_text_chunks = 0;
  pngState.decoder.remember_unknown_chunks = 0;
#endif
  const unsigned decodeErr = lodepng_decode(&rgb, &width, &height, &pngState, payload, payloadLength);
  lodepng_state_cleanup(&pngState);
  free(payload);
  Serial.printf("Static map PNG decode: err=%u size=%ux%u rgb=%p heap=%lu psram=%lu\n", decodeErr, width, height, rgb,
                static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getFreePsram()));
  if (decodeErr != 0 || !rgb) {
    status_ = "static map failed: png decode " + String(decodeErr);
    Serial.println(status_);
    if (rgb) free(rgb);
    if (!rasterBackground_) free(decoded);
    return false;
  }
  if (width != kStaticMapWidth || height != kStaticMapHeight) {
    status_ = "static map failed: png size " + String(width) + "x" + String(height);
    Serial.println(status_);
    free(rgb);
    if (!rasterBackground_) free(decoded);
    return false;
  }
  for (size_t i = 0; i < static_cast<size_t>(kStaticMapWidth) * kStaticMapHeight; ++i) {
    const uint8_t r = rgb[i * 3 + 0];
    const uint8_t g = rgb[i * 3 + 1];
    const uint8_t b = rgb[i * 3 + 2];
    decoded[i] = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  }
  free(rgb);
  rasterBackground_ = decoded;
  rasterHash_ = fnv1a(rasterBackground_, kStaticMapWidth * kStaticMapHeight);
  rasterActive_ = true;
  active_ = true;
  activeCenterLat_ = centerLat;
  activeCenterLon_ = centerLon;
  activeRadiusNm_ = radiusNm;
  activeStyle_ = style;
  fetchGeoapifyAirports(centerLat, centerLon, radiusNm, geoapifyApiKey);
  packageId_ = "geoapify-static";
  version_ = "1";
  attribution_ = "Map image Geoapify; map data OpenStreetMap contributors";
  status_ = "active static map: Geoapify PNG " + String(kStaticMapWidth) + "x" + String(kStaticMapHeight) + " " + styleParam +
            ", airports=" + String(points_.size());
  Serial.println("Map save: " + status_);
  return true;
}

void MapPackageManager::fetchGeoapifyAirports(double centerLat, double centerLon, uint16_t radiusNm, const String& geoapifyApiKey) {
  points_.clear();
  if (geoapifyApiKey.isEmpty()) return;
  const uint32_t radiusMeters = max<uint32_t>(8000, static_cast<uint32_t>(radiusNm) * 1852UL);
  String url = "https://api.geoapify.com/v2/places?categories=airport,airport.international,airport.private,airport.military";
  url += "&filter=circle:" + String(centerLon, 6) + "," + String(centerLat, 6) + "," + String(radiusMeters);
  url += "&limit=" + String(kMaxFetchedAirportPoints);
  url += "&apiKey=" + formEncode(geoapifyApiKey);

  Serial.printf("Airport points fetch: %.6f, %.6f, radius=%u m key=%s\n", centerLat, centerLon, static_cast<unsigned>(radiusMeters),
                geoapifyApiKey.isEmpty() ? "missing" : "set");
  WiFiClientSecure* client = new WiFiClientSecure();
  HTTPClient* http = new HTTPClient();
  if (!client || !http) {
    delete http;
    delete client;
    Serial.println("Airport points skipped: no HTTP memory");
    return;
  }
  client->setInsecure();
  http->setTimeout(7000);
  http->setReuse(false);
  http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http->begin(*client, url)) {
    delete http;
    delete client;
    Serial.println("Airport points skipped: http begin");
    return;
  }
  http->addHeader("Accept", "application/json");
  http->addHeader("Accept-Encoding", "identity");
  http->addHeader("User-Agent", "DesktopAirspace/0.1 airport points");
  const int code = http->GET();
  String payload;
  bool readOk = false;
  if (code == 200) readOk = readHttpPayloadBounded(*http, &payload, 72000, 7000, 700);
  http->end();
  delete http;
  delete client;
  if (code != 200 || !readOk || payload.isEmpty()) {
    Serial.printf("Airport points skipped: HTTP %d read=%s bytes=%u\n", code, readOk ? "ok" : "failed", static_cast<unsigned>(payload.length()));
    return;
  }
  const size_t rawPayloadLength = payload.length();
  if (!decodeChunkedTextPayload(&payload)) {
    String head = payload.substring(0, min<size_t>(120, payload.length()));
    head.replace("\r", " ");
    head.replace("\n", " ");
    Serial.printf("Airport points skipped: chunk decode failed rawBytes=%u head=%s\n", static_cast<unsigned>(rawPayloadLength), head.c_str());
    return;
  }
  if (payload.length() != rawPayloadLength) {
    Serial.printf("Airport points dechunked: raw=%u json=%u\n", static_cast<unsigned>(rawPayloadLength), static_cast<unsigned>(payload.length()));
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, payload);
  if (parseError != DeserializationError::Ok) {
    String head = payload.substring(0, min<size_t>(120, payload.length()));
    head.replace("\r", " ");
    head.replace("\n", " ");
    Serial.printf("Airport points skipped: JSON parse failed err=%s bytes=%u head=%s\n", parseError.c_str(),
                  static_cast<unsigned>(payload.length()), head.c_str());
    return;
  }
  JsonArray features = doc["features"].as<JsonArray>();
  Serial.printf("Airport points parse: features=%u\n", static_cast<unsigned>(features.size()));
  points_.reserve(min(static_cast<size_t>(kMaxFetchedAirportPoints), features.size()));
  uint16_t rejectedNoCode = 0;
  uint16_t rejectedBadCoord = 0;
  uint16_t rejectedDuplicate = 0;
  uint16_t rejectedExcluded = 0;
  for (JsonObject feature : features) {
    if (points_.size() >= kMaxFetchedAirportPoints) break;
    JsonObject props = feature["properties"];
    String codeText = props["airport"]["icao"] | "";
    if (codeText.isEmpty()) codeText = props["datasource"]["raw"]["icao"] | "";
    if (codeText.isEmpty()) codeText = props["airport"]["iata"] | "";
    if (codeText.isEmpty()) codeText = props["datasource"]["raw"]["iata"] | "";
    codeText = uppercaseCode(codeText);
    if (codeText.isEmpty()) {
      ++rejectedNoCode;
      continue;
    }
    if (isExcludedAirportCode(codeText)) {
      ++rejectedExcluded;
      continue;
    }

    JsonArray coords = feature["geometry"]["coordinates"].as<JsonArray>();
    double lon = coords.size() >= 2 ? coords[0].as<double>() : (props["lon"] | 999.0);
    double lat = coords.size() >= 2 ? coords[1].as<double>() : (props["lat"] | 999.0);
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
      ++rejectedBadCoord;
      continue;
    }

    bool duplicate = false;
    for (const MapPoint& existing : points_) {
      if (existing.code == codeText) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      ++rejectedDuplicate;
      continue;
    }
    points_.push_back({{lat, lon}, kMapAirport, codeText});
    Serial.printf("  airport %s lat=%.6f lon=%.6f dist=%.1fNM\n", codeText.c_str(), lat, lon, geo::distanceNm({centerLat, centerLon}, {lat, lon}));
  }
  Serial.printf("Airport points: accepted=%u rejected noCode=%u badCoord=%u duplicate=%u excluded=%u\n", static_cast<unsigned>(points_.size()),
                static_cast<unsigned>(rejectedNoCode), static_cast<unsigned>(rejectedBadCoord),
                static_cast<unsigned>(rejectedDuplicate), static_cast<unsigned>(rejectedExcluded));
  if (points_.empty() && inEastMed(centerLat, centerLon)) {
    for (const auto& point : kEastMedAirports) {
      if (geo::distanceNm({centerLat, centerLon}, point.location) <= radiusNm && !isExcludedAirportCode(point.code)) {
        points_.push_back(point);
      }
    }
    Serial.printf("Airport points fallback seed: %u\n", static_cast<unsigned>(points_.size()));
  }
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
  doc["packageId"] = "local-vector-cache";
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
  packageId_ = "local-vector-cache";
  version_ = "1";
  attribution_ = "Map data © OpenStreetMap contributors; airports from OSM aeroway data";
  status_ = "active local map: " + String(segments_.size()) + " segments, " + String(points_.size()) + " points";
  Serial.println("Map save: " + status_);
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
  if (!active_) {
    Serial.println("Airport project: skipped inactive map");
    return out;
  }
  out.reserve(std::min(maxPoints, points_.size()));
  uint16_t dropRange = 0;
  for (const MapPoint& point : points_) {
    if (out.size() >= maxPoints) break;
    const double distance = geo::distanceNm(origin, point.location);
    if (distance > rangeNm) {
      ++dropRange;
      continue;
    }
    const auto p = geo::projectAzimuthalEquidistant(origin, point.location, rangeNm, 232);
    out.push_back({static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), point.kind, point.code});
  }
  static uint32_t lastProjectLogMs = 0;
  if (millis() - lastProjectLogMs > 5000 || (!points_.empty() && out.empty())) {
    lastProjectLogMs = millis();
    Serial.printf("Airport project: stored=%u shown=%u dropRange=%u range=%u\n", static_cast<unsigned>(points_.size()),
                  static_cast<unsigned>(out.size()), static_cast<unsigned>(dropRange), static_cast<unsigned>(rangeNm));
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
  packageId_ = doc["packageId"] | "local-vector-cache";
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
