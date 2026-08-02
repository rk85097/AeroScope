#include "config/Config.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace micro_radar {
namespace {
constexpr const char* kConfigPath = "/config.json";
constexpr const char* kTempPath = "/config.tmp";
constexpr const char* kLittleFsPartition = "assets";
constexpr const char* kDefaultAviationstackAccessKey = "26c1d927027790d770cdf538f7f33533";
constexpr uint16_t kSupportedSchema = 9;

bool mountConfigFs() {
  return LittleFS.begin(true, "/littlefs", 10, kLittleFsPartition);
}
}  // namespace

ConfigManager::ConfigManager() {
  mutex_ = xSemaphoreCreateMutex();
}

void ConfigManager::lock() const {
  if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
}

void ConfigManager::unlock() const {
  if (mutex_) xSemaphoreGive(mutex_);
}

bool ConfigManager::begin() {
  if (!mountConfigFs()) {
    Serial.println("ConfigManager: LittleFS mount failed on assets partition");
    return false;
  }
  if (!LittleFS.exists(kConfigPath)) return saveAtomic(config_);

  File file = LittleFS.open(kConfigPath, "r");
  JsonDocument doc;
  if (deserializeJson(doc, file) != DeserializationError::Ok) {
    file.close();
    return factoryReset();
  }
  file.close();

  const uint16_t loadedSchema = doc["schemaVersion"] | 1;
  config_.schemaVersion = loadedSchema > kSupportedSchema ? kSupportedSchema : loadedSchema;
  config_.wifiSsid = doc["wifiSsid"] | "";
  config_.wifiPassword = doc["wifiPassword"] | "";
  config_.homeLat = doc["homeLat"] | config_.homeLat;
  config_.homeLon = doc["homeLon"] | config_.homeLon;
  config_.locationName = doc["locationName"] | config_.locationName;
  config_.timeZone = doc["timeZone"] | config_.timeZone;
  config_.localReceiverUrl = doc["localReceiverUrl"] | "";
  config_.aviationstackAccessKey = doc["aviationstackAccessKey"] | config_.aviationstackAccessKey;
  if (config_.aviationstackAccessKey.isEmpty()) config_.aviationstackAccessKey = kDefaultAviationstackAccessKey;
  config_.rangeNm = doc["rangeNm"] | config_.rangeNm;
  config_.mapEnabled = true;
  config_.airportsEnabled = doc["airportsEnabled"] | true;
  config_.airportLabelsEnabled = doc["airportLabelsEnabled"] | config_.airportLabelsEnabled;
  config_.rangeRingsEnabled = doc["rangeRingsEnabled"] | config_.rangeRingsEnabled;
  config_.outerRangeRingEnabled = doc["outerRangeRingEnabled"] | config_.outerRangeRingEnabled;
  config_.trailsEnabled = doc["trailsEnabled"] | true;
  config_.sweepLineEnabled = doc["sweepLineEnabled"] | config_.sweepLineEnabled;
  config_.showGroundAircraft = doc["showGroundAircraft"] | false;
  config_.labelDensity = doc["labelDensity"] | 5;
  config_.uiScale = doc["uiScale"] | 100;
  config_.aircraftIconScale = doc["aircraftIconScale"] | config_.uiScale;
  config_.aircraftTextScale = doc["aircraftTextScale"] | config_.uiScale;
  config_.fontStyle = doc["fontStyle"] | config_.fontStyle;
  config_.displayRotation = doc["displayRotation"] | config_.displayRotation;
  config_.labelBackplateOpacity = doc["labelBackplateOpacity"] | config_.labelBackplateOpacity;
  config_.sweepSecondsPerRotation = doc["sweepSecondsPerRotation"] | config_.sweepSecondsPerRotation;
  config_.airportIconType = doc["airportIconType"] | config_.airportIconType;
  config_.airportLabelScale = doc["airportLabelScale"] | config_.airportLabelScale;
  config_.rangeRingStyle = doc["rangeRingStyle"] | config_.rangeRingStyle;
  config_.rangeRingThickness = doc["rangeRingThickness"] | config_.rangeRingThickness;
  config_.crosshairEnabled = doc["crosshairEnabled"] | config_.crosshairEnabled;
  config_.crosshairStyle = doc["crosshairStyle"] | config_.crosshairStyle;
  config_.crosshairThickness = doc["crosshairThickness"] | config_.crosshairThickness;
  config_.sweepFadeWidthDeg = doc["sweepFadeWidthDeg"] | config_.sweepFadeWidthDeg;
  config_.aircraftColor = doc["aircraftColor"] | config_.aircraftColor;
  config_.sweepColor = doc["sweepColor"] | config_.sweepColor;
  config_.trailColor = doc["trailColor"] | config_.trailColor;
  config_.labelColor = doc["labelColor"] | config_.labelColor;
  config_.detailLabelColor = doc["detailLabelColor"] | config_.detailLabelColor;
  config_.altitudeLabelColor = doc["altitudeLabelColor"] | config_.detailLabelColor;
  config_.speedLabelColor = doc["speedLabelColor"] | config_.detailLabelColor;
  config_.landColor = doc["landColor"] | config_.landColor;
  config_.waterColor = doc["waterColor"] | config_.waterColor;
  config_.roadColor = doc["roadColor"] | config_.roadColor;
  config_.airportColor = doc["airportColor"] | config_.airportColor;
  config_.airportLabelColor = doc["airportLabelColor"] | config_.airportLabelColor;
  config_.scopeBackgroundColor = doc["scopeBackgroundColor"] | config_.scopeBackgroundColor;
  config_.scopeOutsideColor = doc["scopeOutsideColor"] | config_.scopeOutsideColor;
  config_.mapCoastColor = doc["mapCoastColor"] | config_.mapCoastColor;
  config_.mapBorderColor = doc["mapBorderColor"] | config_.mapBorderColor;
  config_.mapWaterLineColor = doc["mapWaterLineColor"] | config_.mapWaterLineColor;
  config_.rangeRingColor = doc["rangeRingColor"] | config_.rangeRingColor;
  config_.crosshairColor = doc["crosshairColor"] | config_.crosshairColor;
  config_.rangeRingLabelsEnabled = doc["rangeRingLabelsEnabled"] | config_.rangeRingLabelsEnabled;
  config_.brightness = doc["brightness"] | 210;
  config_.dimBrightness = doc["dimBrightness"] | 70;
  config_.dimStartHour = doc["dimStartHour"] | 22;
  config_.dimEndHour = doc["dimEndHour"] | 7;
  config_.adminPasswordHash = doc["adminPasswordHash"] | "";
  const char* units = doc["units"] | "nm";
  config_.units = String(units) == "metric" ? Units::Metric : Units::Nautical;
  const char* theme = doc["theme"] | "auto";
  if (String(theme) == "light") config_.theme = ThemeMode::Light;
  else if (String(theme) == "dark") config_.theme = ThemeMode::Dark;
  else config_.theme = ThemeMode::AutoSun;
  const char* provider = doc["providerMode"] | "auto";
  if (String(provider) == "adsb_lol") config_.providerMode = ProviderMode::AdsbLol;
  else if (String(provider) == "airplanes_live") config_.providerMode = ProviderMode::AirplanesLive;
  else if (String(provider) == "opensky") config_.providerMode = ProviderMode::OpenSky;
  else if (String(provider) == "adsb_fi") config_.providerMode = ProviderMode::AdsbFi;
  else config_.providerMode = ProviderMode::Automatic;
  const char* mapStyle = doc["mapStyle"] | "fr24_dark";
  if (String(mapStyle) == "radar_dark") config_.mapStyle = MapStyle::RadarDark;
  else if (String(mapStyle) == "high_contrast") config_.mapStyle = MapStyle::HighContrast;
  else config_.mapStyle = MapStyle::Fr24Dark;
  const char* labelMode = doc["aircraftLabelMode"] | "basic";
  if (String(labelMode) == "callsign") config_.aircraftLabelMode = AircraftLabelMode::CallsignOnly;
  else if (String(labelMode) == "full") config_.aircraftLabelMode = AircraftLabelMode::Full;
  else if (String(labelMode) == "off") config_.aircraftLabelMode = AircraftLabelMode::Off;
  else config_.aircraftLabelMode = AircraftLabelMode::Basic;

  if (loadedSchema < 2) {
    config_.rangeRingsEnabled = true;
    config_.labelBackplateOpacity = 35;
    config_.sweepSecondsPerRotation = 1;
    config_.aircraftColor = 0x23F46C;
    config_.sweepColor = 0x16F46B;
    config_.trailColor = 0x36444E;
    config_.labelColor = 0x28F26E;
    config_.detailLabelColor = 0x168C49;
    config_.altitudeLabelColor = 0x168C49;
    config_.speedLabelColor = 0x168C49;
    config_.airportColor = 0x2AF46F;
    config_.airportLabelColor = 0x2AF46F;
    config_.mapStyle = MapStyle::RadarDark;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 3) {
    config_.airportIconType = 0;
    config_.airportLabelsEnabled = true;
    config_.airportLabelScale = 100;
    config_.scopeBackgroundColor = 0x0D1218;
    config_.scopeOutsideColor = 0x000000;
    config_.mapCoastColor = 0x697C86;
    config_.mapBorderColor = 0x46535C;
    config_.mapWaterLineColor = 0x303E4A;
    config_.rangeRingColor = 0x202E38;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 4) {
    config_.airportLabelsEnabled = true;
    config_.airportLabelScale = 100;
    config_.airportLabelColor = config_.airportColor ? config_.airportColor : 0x2AF46F;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 5) {
    config_.airportIconType = 1;
    config_.sweepLineEnabled = true;
    config_.trailColor = 0x36444E;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 6) {
    config_.altitudeLabelColor = config_.detailLabelColor ? config_.detailLabelColor : 0x168C49;
    config_.speedLabelColor = config_.detailLabelColor ? config_.detailLabelColor : 0x168C49;
    config_.rangeRingStyle = 0;
    config_.rangeRingThickness = 1;
    config_.rangeRingLabelsEnabled = false;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 7) {
    config_.crosshairEnabled = true;
    config_.crosshairStyle = 0;
    config_.crosshairThickness = 1;
    config_.crosshairColor = 0x203A36;
    config_.sweepFadeWidthDeg = 24;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 8) {
    config_.fontStyle = 0;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (loadedSchema < 9) {
    config_.displayRotation = 0;
    config_.schemaVersion = kSupportedSchema;
    saveAtomic(config_);
  }
  if (doc["enabledRangesNm"].is<JsonArray>()) {
    config_.enabledRangesNm.clear();
    for (uint16_t range : doc["enabledRangesNm"].as<JsonArray>()) config_.enabledRangesNm.push_back(range);
  }
  return true;
}

AppConfig ConfigManager::get() const {
  lock();
  AppConfig snapshot = config_;
  unlock();
  return snapshot;
}

bool ConfigManager::validate(const AppConfig& config, String* error) const {
  if (config.schemaVersion > kSupportedSchema) {
    if (error) *error = "Unsupported configuration schema";
    return false;
  }
  if (config.homeLat < -90 || config.homeLat > 90 || config.homeLon < -180 || config.homeLon > 180) {
    if (error) *error = "Location coordinates are outside valid bounds";
    return false;
  }
  if (!config.localReceiverUrl.isEmpty() && !config.localReceiverUrl.startsWith("http://") && !config.localReceiverUrl.startsWith("https://")) {
    if (error) *error = "Local receiver URL must start with http:// or https://";
    return false;
  }
  if (config.enabledRangesNm.empty()) {
    if (error) *error = "At least one range preset is required";
    return false;
  }
  for (uint16_t range : config.enabledRangesNm) {
    if (!(range == 5 || range == 10 || range == 20 || range == 40 || range == 80 || range == 160 || range == 320)) {
      if (error) *error = "Invalid range preset";
      return false;
    }
  }
  if (config.brightness > 255 || config.dimBrightness > 255) {
    if (error) *error = "Brightness is outside valid bounds";
    return false;
  }
  if (config.uiScale < 70 || config.uiScale > 140 || config.aircraftIconScale < 70 || config.aircraftIconScale > 160 ||
      config.aircraftTextScale < 70 || config.aircraftTextScale > 220) {
    if (error) *error = "UI scale is outside valid bounds";
    return false;
  }
  if (config.labelBackplateOpacity > 100) {
    if (error) *error = "Label background opacity is outside valid bounds";
    return false;
  }
  if (config.fontStyle > 2) {
    if (error) *error = "Font style is outside valid bounds";
    return false;
  }
  if (config.displayRotation > 3) {
    if (error) *error = "Display rotation is outside valid bounds";
    return false;
  }
  if (config.sweepSecondsPerRotation < 1 || config.sweepSecondsPerRotation > 10) {
    if (error) *error = "Sweep speed is outside valid bounds";
    return false;
  }
  if (config.airportIconType > 2) {
    if (error) *error = "Airport icon type is outside valid bounds";
    return false;
  }
  if (config.airportLabelScale < 70 || config.airportLabelScale > 160) {
    if (error) *error = "Airport label size is outside valid bounds";
    return false;
  }
  if (config.rangeRingStyle > 1) {
    if (error) *error = "Range ring style is outside valid bounds";
    return false;
  }
  if (config.rangeRingThickness < 1 || config.rangeRingThickness > 5) {
    if (error) *error = "Range ring thickness is outside valid bounds";
    return false;
  }
  if (config.crosshairStyle > 1) {
    if (error) *error = "Crosshair style is outside valid bounds";
    return false;
  }
  if (config.crosshairThickness < 1 || config.crosshairThickness > 5) {
    if (error) *error = "Crosshair thickness is outside valid bounds";
    return false;
  }
  if (config.sweepFadeWidthDeg < 8 || config.sweepFadeWidthDeg > 90) {
    if (error) *error = "Sweep fade width is outside valid bounds";
    return false;
  }
  return true;
}

bool ConfigManager::saveAtomic(const AppConfig& config) {
  String error;
  if (!validate(config, &error)) return false;

  File file = LittleFS.open(kTempPath, "w");
  if (!file) {
    Serial.println("ConfigManager: failed to open temporary config file");
    return false;
  }
  JsonDocument doc;
  doc["schemaVersion"] = config.schemaVersion;
  doc["wifiSsid"] = config.wifiSsid;
  doc["wifiPassword"] = config.wifiPassword;
  doc["homeLat"] = config.homeLat;
  doc["homeLon"] = config.homeLon;
  doc["locationName"] = config.locationName;
  doc["timeZone"] = config.timeZone;
  doc["localReceiverUrl"] = config.localReceiverUrl;
  doc["aviationstackAccessKey"] = config.aviationstackAccessKey;
  doc["rangeNm"] = config.rangeNm;
  doc["mapEnabled"] = config.mapEnabled;
  doc["airportsEnabled"] = config.airportsEnabled;
  doc["airportLabelsEnabled"] = config.airportLabelsEnabled;
  doc["rangeRingsEnabled"] = config.rangeRingsEnabled;
  doc["outerRangeRingEnabled"] = config.outerRangeRingEnabled;
  doc["trailsEnabled"] = config.trailsEnabled;
  doc["sweepLineEnabled"] = config.sweepLineEnabled;
  doc["showGroundAircraft"] = config.showGroundAircraft;
  doc["labelDensity"] = config.labelDensity;
  doc["uiScale"] = config.uiScale;
  doc["aircraftIconScale"] = config.aircraftIconScale;
  doc["aircraftTextScale"] = config.aircraftTextScale;
  doc["fontStyle"] = config.fontStyle;
  doc["displayRotation"] = config.displayRotation;
  doc["labelBackplateOpacity"] = config.labelBackplateOpacity;
  doc["sweepSecondsPerRotation"] = config.sweepSecondsPerRotation;
  doc["airportIconType"] = config.airportIconType;
  doc["airportLabelScale"] = config.airportLabelScale;
  doc["rangeRingStyle"] = config.rangeRingStyle;
  doc["rangeRingThickness"] = config.rangeRingThickness;
  doc["crosshairEnabled"] = config.crosshairEnabled;
  doc["crosshairStyle"] = config.crosshairStyle;
  doc["crosshairThickness"] = config.crosshairThickness;
  doc["sweepFadeWidthDeg"] = config.sweepFadeWidthDeg;
  doc["aircraftColor"] = config.aircraftColor;
  doc["sweepColor"] = config.sweepColor;
  doc["trailColor"] = config.trailColor;
  doc["labelColor"] = config.labelColor;
  doc["detailLabelColor"] = config.detailLabelColor;
  doc["altitudeLabelColor"] = config.altitudeLabelColor;
  doc["speedLabelColor"] = config.speedLabelColor;
  doc["landColor"] = config.landColor;
  doc["waterColor"] = config.waterColor;
  doc["roadColor"] = config.roadColor;
  doc["airportColor"] = config.airportColor;
  doc["airportLabelColor"] = config.airportLabelColor;
  doc["scopeBackgroundColor"] = config.scopeBackgroundColor;
  doc["scopeOutsideColor"] = config.scopeOutsideColor;
  doc["mapCoastColor"] = config.mapCoastColor;
  doc["mapBorderColor"] = config.mapBorderColor;
  doc["mapWaterLineColor"] = config.mapWaterLineColor;
  doc["rangeRingColor"] = config.rangeRingColor;
  doc["crosshairColor"] = config.crosshairColor;
  doc["rangeRingLabelsEnabled"] = config.rangeRingLabelsEnabled;
  doc["brightness"] = config.brightness;
  doc["dimBrightness"] = config.dimBrightness;
  doc["dimStartHour"] = config.dimStartHour;
  doc["dimEndHour"] = config.dimEndHour;
  doc["adminPasswordHash"] = config.adminPasswordHash;
  doc["units"] = config.units == Units::Metric ? "metric" : "nm";
  doc["theme"] = config.theme == ThemeMode::Light ? "light" : config.theme == ThemeMode::Dark ? "dark" : "auto";
  doc["providerMode"] = config.providerMode == ProviderMode::AdsbLol
                            ? "adsb_lol"
                            : config.providerMode == ProviderMode::AirplanesLive ? "airplanes_live"
                            : config.providerMode == ProviderMode::OpenSky       ? "opensky"
                            : config.providerMode == ProviderMode::AdsbFi        ? "adsb_fi"
                                                                                  : "auto";
  doc["mapStyle"] = config.mapStyle == MapStyle::RadarDark ? "radar_dark" : config.mapStyle == MapStyle::HighContrast ? "high_contrast" : "fr24_dark";
  doc["aircraftLabelMode"] = config.aircraftLabelMode == AircraftLabelMode::CallsignOnly
                                 ? "callsign"
                                 : config.aircraftLabelMode == AircraftLabelMode::Full ? "full" : config.aircraftLabelMode == AircraftLabelMode::Off ? "off" : "basic";
  JsonArray ranges = doc["enabledRangesNm"].to<JsonArray>();
  for (uint16_t range : config.enabledRangesNm) ranges.add(range);
  if (serializeJson(doc, file) == 0) {
    Serial.println("ConfigManager: failed to serialize config");
    return false;
  }
  file.close();
  LittleFS.remove(kConfigPath);
  if (!LittleFS.rename(kTempPath, kConfigPath)) {
    Serial.println("ConfigManager: failed to rename temporary config file");
    return false;
  }
  lock();
  config_ = config;
  unlock();
  return true;
}

bool ConfigManager::factoryReset() {
  lock();
  config_ = AppConfig {};
  unlock();
  LittleFS.remove(kConfigPath);
  LittleFS.remove(kTempPath);
  return saveAtomic(config_);
}

}  // namespace micro_radar
