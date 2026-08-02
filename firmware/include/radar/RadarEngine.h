#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

#include "aircraft/Aircraft.h"
#include "config/Config.h"
#include "geo/Geo.h"

namespace micro_radar {

class RadarEngine {
 public:
  RadarEngine();
  void configure(const AppConfig& config);
  void ingest(std::vector<Aircraft> aircraft);
  const std::vector<Aircraft>& visibleAircraft() const;
  std::vector<Aircraft> snapshotVisibleAircraft() const;
  void snapshotVisibleAircraft(std::vector<Aircraft>* out) const;
  bool mergeEnrichment(const Aircraft& enrichment);
  size_t visibleAircraftCount() const;
  AppConfig snapshotConfig() const;
  uint16_t nextRange();
  uint16_t previousRange();
  void setRange(uint16_t rangeNm);
  uint16_t rangeNm() const;
  bool toggleMap();
  void setMapEnabled(bool enabled);
  bool mapEnabled() const;
  bool selectNearestDisplayPoint(int16_t x, int16_t y);
  void clearSelection();
  void toggleTrackSelected();
  void animate(uint32_t nowMs);
  void updateProjection();

 private:
  void lock() const;
  void unlock() const;
  void clearTrailsLocked();

  AppConfig config_;
  std::vector<Aircraft> aircraft_;
  mutable SemaphoreHandle_t mutex_ {nullptr};
};

}  // namespace micro_radar
