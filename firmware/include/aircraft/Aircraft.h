#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <array>

namespace micro_radar {

enum class FieldState : uint8_t { Unavailable, ProviderSupplied, Inferred, Estimated };
enum class AircraftCategory : uint8_t { Unknown, Light, Heavy, Rotorcraft, Glider, Ground, Emergency };
enum class ProviderId : uint8_t { AdsbLol, AirplanesLive, OpenSky, AdsbFi, LocalReceiver };

template <typename T> struct OptionalField {
  T value {};
  FieldState state {FieldState::Unavailable};
  OptionalField() = default;
  OptionalField(T fieldValue, FieldState fieldState) : value(fieldValue), state(fieldState) {}
  bool available() const { return state != FieldState::Unavailable; }
};

struct Aircraft {
  String icao24;
  OptionalField<String> providerId;
  OptionalField<String> callsign;
  OptionalField<String> airlineIcao;
  OptionalField<String> airlineName;
  OptionalField<String> flightNumber;
  OptionalField<String> registration;
  OptionalField<String> typeCode;
  OptionalField<String> modelName;
  OptionalField<String> operatorName;
  OptionalField<String> country;

  OptionalField<double> latitude;
  OptionalField<double> longitude;
  OptionalField<uint64_t> positionTimestampMs;
  OptionalField<uint32_t> positionAgeSec;
  OptionalField<int32_t> baroAltitudeFt;
  OptionalField<int32_t> geomAltitudeFt;
  OptionalField<double> groundSpeedKt;
  OptionalField<double> trueAirspeedKt;
  OptionalField<double> indicatedAirspeedKt;
  OptionalField<double> trackDeg;
  OptionalField<double> headingDeg;
  OptionalField<int32_t> verticalRateFpm;
  OptionalField<bool> onGround;

  double distanceNm {0};
  double bearingDeg {0};
  bool estimatedPositionUsed {false};

  OptionalField<String> squawk;
  OptionalField<String> emergency;
  OptionalField<String> sourceType;
  ProviderId provider {ProviderId::AdsbLol};
  OptionalField<uint32_t> messageAgeSec;
  OptionalField<uint64_t> lastReceivedMs;
  OptionalField<String> routeOrigin;
  OptionalField<String> routeDestination;
  FieldState routeProvenance {FieldState::Unavailable};

  double renderX {0};
  double renderY {0};
  double interpolationStartX {0};
  double interpolationStartY {0};
  double targetX {0};
  double targetY {0};
  uint32_t interpolationStartMs {0};
  uint32_t interpolationDurationMs {0};
  double displayHeadingDeg {0};
  AircraftCategory iconCategory {AircraftCategory::Unknown};
  bool selected {false};
  bool tracked {false};
  bool stale {false};
  uint8_t labelPriority {0};
  std::array<int16_t, 8> trailX {};
  std::array<int16_t, 8> trailY {};
  uint8_t trailCount {0};
};

}  // namespace micro_radar
