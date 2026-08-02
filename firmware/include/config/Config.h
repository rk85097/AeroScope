#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

namespace micro_radar {

enum class Units : uint8_t { Nautical, Metric };
enum class ThemeMode : uint8_t { Light, Dark, AutoSun };
enum class ProviderMode : uint8_t { Automatic, AdsbLol, AirplanesLive, OpenSky, AdsbFi };
enum class MapStyle : uint8_t { DarkMatter, OSMBrightGrey, OSMBright, Positron };
enum class AircraftLabelMode : uint8_t { CallsignOnly, Basic, Full, Off };

struct AppConfig {
  uint16_t schemaVersion {13};
  String wifiSsid;
  String wifiPassword;
  double homeLat {32.0853};
  double homeLon {34.7818};
  String locationName {""};
  String timeZone {"Asia/Jerusalem"};
  String localReceiverUrl;
  String geoapifyApiKey;
  MapStyle mapStyle {MapStyle::DarkMatter};
  uint8_t mapBrightness {100};
  Units units {Units::Nautical};
  ThemeMode theme {ThemeMode::AutoSun};
  uint8_t brightness {255};
  uint16_t rangeNm {15};
  std::vector<uint16_t> enabledRangesNm {5, 10, 20, 40, 80, 160, 320};
  bool mapEnabled {true};
  bool airportsEnabled {true};
  bool airportLabelsEnabled {true};
  bool rangeRingsEnabled {true};
  bool outerRangeRingEnabled {false};
  bool scopeEdgeEnabled {true};
  bool cardinalLabelsEnabled {true};
  bool ordinalLabelsEnabled {true};
  bool trailsEnabled {true};
  bool sweepLineEnabled {true};
  bool showGroundAircraft {false};
  uint8_t labelDensity {10};
  uint8_t uiScale {100};
  uint8_t aircraftIconScale {70};
  uint8_t aircraftTextScale {150};
  uint8_t aircraftLabelSpacing {33};
  uint8_t fontStyle {0};
  uint8_t displayRotation {0};
  uint8_t labelBackplateOpacity {0};
  uint8_t sweepSecondsPerRotation {10};
  uint8_t airportIconType {1};
  uint8_t airportLabelScale {135};
  uint8_t rangeRingStyle {0};
  uint8_t rangeRingThickness {1};
  uint8_t scopeEdgeThickness {1};
  bool crosshairEnabled {true};
  uint8_t crosshairStyle {1};
  uint8_t crosshairThickness {3};
  uint8_t sweepFadeWidthDeg {8};
  uint32_t aircraftColor {0xF5EC00};
  uint32_t sweepColor {0x16F46B};
  uint32_t trailColor {0x666100};
  uint32_t labelColor {0x28F26E};
  uint32_t detailLabelColor {0x168C49};
  uint32_t detailBackgroundColor {0x000000};
  uint32_t altitudeLabelColor {0xBE38F3};
  uint32_t speedLabelColor {0x00A1D8};
  uint32_t landColor {0x000000};
  uint32_t waterColor {0x011D57};
  uint32_t roadColor {0x2A534E};
  uint32_t airportColor {0xFFFFFF};
  uint32_t airportLabelColor {0xFFFFFF};
  uint32_t scopeBackgroundColor {0x20FF36};
  uint32_t scopeOutsideColor {0x000000};
  uint32_t mapCoastColor {0x000000};
  uint32_t mapBorderColor {0x000000};
  uint32_t mapWaterLineColor {0x000000};
  uint32_t rangeRingColor {0x20FF36};
  uint32_t crosshairColor {0x20FF36};
  uint32_t cardinalLabelColor {0x28F26E};
  uint32_t ordinalLabelColor {0x28F26E};
  bool rangeRingLabelsEnabled {true};
  AircraftLabelMode aircraftLabelMode {AircraftLabelMode::Full};
  ProviderMode providerMode {ProviderMode::Automatic};
  String adminPasswordHash;
};

class ConfigManager {
 public:
  ConfigManager();
  bool begin();
  AppConfig get() const;
  bool validate(const AppConfig& config, String* error) const;
  bool saveAtomic(const AppConfig& config);
  bool factoryReset();

 private:
  void lock() const;
  void unlock() const;

  AppConfig config_;
  mutable SemaphoreHandle_t mutex_ {nullptr};
};

}  // namespace micro_radar
