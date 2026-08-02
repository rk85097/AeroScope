#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

namespace micro_radar {

enum class Units : uint8_t { Nautical, Metric };
enum class ThemeMode : uint8_t { Light, Dark, AutoSun };
enum class ProviderMode : uint8_t { Automatic, AdsbLol, AirplanesLive, OpenSky, AdsbFi };
enum class MapStyle : uint8_t { RadarDark, Fr24Dark, HighContrast };
enum class AircraftLabelMode : uint8_t { CallsignOnly, Basic, Full, Off };

struct AppConfig {
  uint16_t schemaVersion {9};
  String wifiSsid;
  String wifiPassword;
  double homeLat {32.0853};
  double homeLon {34.7818};
  String locationName {"Tel Aviv"};
  String timeZone {"Asia/Jerusalem"};
  String localReceiverUrl;
  String aviationstackAccessKey {"26c1d927027790d770cdf538f7f33533"};
  Units units {Units::Nautical};
  ThemeMode theme {ThemeMode::AutoSun};
  uint8_t brightness {210};
  uint8_t dimBrightness {70};
  uint8_t dimStartHour {22};
  uint8_t dimEndHour {7};
  uint16_t rangeNm {20};
  std::vector<uint16_t> enabledRangesNm {5, 10, 20, 40, 80, 160, 320};
  bool mapEnabled {true};
  bool airportsEnabled {true};
  bool airportLabelsEnabled {true};
  bool rangeRingsEnabled {true};
  bool outerRangeRingEnabled {true};
  bool trailsEnabled {true};
  bool sweepLineEnabled {true};
  bool showGroundAircraft {false};
  uint8_t labelDensity {5};
  uint8_t uiScale {100};
  uint8_t aircraftIconScale {100};
  uint8_t aircraftTextScale {100};
  uint8_t fontStyle {0};
  uint8_t displayRotation {0};
  uint8_t labelBackplateOpacity {35};
  uint8_t sweepSecondsPerRotation {1};
  uint8_t airportIconType {1};
  uint8_t airportLabelScale {100};
  uint8_t rangeRingStyle {0};
  uint8_t rangeRingThickness {1};
  bool crosshairEnabled {true};
  uint8_t crosshairStyle {0};
  uint8_t crosshairThickness {1};
  uint8_t sweepFadeWidthDeg {24};
  uint32_t aircraftColor {0x23F46C};
  uint32_t sweepColor {0x16F46B};
  uint32_t trailColor {0x36444E};
  uint32_t labelColor {0x28F26E};
  uint32_t detailLabelColor {0x168C49};
  uint32_t altitudeLabelColor {0x168C49};
  uint32_t speedLabelColor {0x168C49};
  uint32_t landColor {0x102629};
  uint32_t waterColor {0x11181D};
  uint32_t roadColor {0x2A534E};
  uint32_t airportColor {0x2AF46F};
  uint32_t airportLabelColor {0x2AF46F};
  uint32_t scopeBackgroundColor {0x0D1218};
  uint32_t scopeOutsideColor {0x000000};
  uint32_t mapCoastColor {0x697C86};
  uint32_t mapBorderColor {0x46535C};
  uint32_t mapWaterLineColor {0x303E4A};
  uint32_t rangeRingColor {0x202E38};
  uint32_t crosshairColor {0x203A36};
  bool rangeRingLabelsEnabled {false};
  MapStyle mapStyle {MapStyle::RadarDark};
  AircraftLabelMode aircraftLabelMode {AircraftLabelMode::Basic};
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
