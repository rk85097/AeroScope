#include "radar/RadarEngine.h"

#include <algorithm>
#include <math.h>

namespace micro_radar {
namespace {
constexpr uint32_t kInterpolationDurationMs = 5200;
constexpr uint32_t kDeadReckonMaxMs = 18000;
constexpr uint32_t kMissingAircraftGraceMs = 22000;
constexpr uint16_t kProjectionRadiusPx = 232;
constexpr double kDegToRad = 0.017453292519943295;
constexpr double kTrailMinStepPx = 5.0;
constexpr bool kVerboseAircraftIngestLog = false;

const char* providerName(ProviderId provider) {
  switch (provider) {
    case ProviderId::AdsbLol: return "adsb_lol";
    case ProviderId::AirplanesLive: return "airplanes_live";
    case ProviderId::OpenSky: return "opensky";
    case ProviderId::AdsbFi: return "adsb_fi";
    case ProviderId::LocalReceiver: return "local";
    default: return "unknown";
  }
}

uint32_t aircraftLastSeenMs(const Aircraft& ac) {
  if (ac.lastReceivedMs.available() && ac.lastReceivedMs.value < 0xFFFFFFFFULL) return static_cast<uint32_t>(ac.lastReceivedMs.value);
  return ac.interpolationStartMs;
}
}

RadarEngine::RadarEngine() {
  mutex_ = xSemaphoreCreateMutex();
}

void RadarEngine::lock() const {
  if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
}

void RadarEngine::unlock() const {
  if (mutex_) xSemaphoreGive(mutex_);
}

void RadarEngine::configure(const AppConfig& config) {
  lock();
  const bool scopeChanged = config_.rangeNm != config.rangeNm || fabs(config_.homeLat - config.homeLat) > 0.000001 ||
                            fabs(config_.homeLon - config.homeLon) > 0.000001;
  config_ = config;
  aircraft_.erase(std::remove_if(aircraft_.begin(), aircraft_.end(), [&](const Aircraft& ac) {
                    return ac.distanceNm > config_.rangeNm || (!config_.showGroundAircraft && ac.onGround.available() && ac.onGround.value);
                  }),
                  aircraft_.end());
  updateProjection();
  if (scopeChanged || !config_.trailsEnabled) clearTrailsLocked();
  unlock();
}

void RadarEngine::ingest(std::vector<Aircraft> aircraft) {
  lock();
  const size_t incomingCount = aircraft.size();
  size_t dropGround = 0;
  size_t dropRange = 0;
  if (kVerboseAircraftIngestLog && incomingCount > 0 && incomingCount <= 16) {
    Serial.printf("Radar ingest begin: incoming=%u range=%u groundHidden=%s\n", static_cast<unsigned>(incomingCount), config_.rangeNm,
                  config_.showGroundAircraft ? "off" : "on");
    for (const Aircraft& ac : aircraft) {
      const bool isGround = ac.onGround.available() && ac.onGround.value;
      const bool outOfRange = ac.distanceNm > config_.rangeNm;
      Serial.printf("  ac %s %s src=%s type=%s model=%s dist=%.1f ground=%s alt=%ld gs=%.0f %s\n", ac.icao24.c_str(),
                    ac.callsign.available() ? ac.callsign.value.c_str() : "-", providerName(ac.provider),
                    ac.typeCode.available() ? ac.typeCode.value.c_str() : "-", ac.modelName.available() ? ac.modelName.value.c_str() : "-",
                    ac.distanceNm, isGround ? "yes" : "no",
                    ac.baroAltitudeFt.available() ? static_cast<long>(ac.baroAltitudeFt.value) : -99999L,
                    ac.groundSpeedKt.available() ? ac.groundSpeedKt.value : -1.0,
                    outOfRange ? "DROP_RANGE" : (!config_.showGroundAircraft && isGround ? "DROP_GROUND" : "KEEP"));
    }
  }
  aircraft.erase(std::remove_if(aircraft.begin(), aircraft.end(), [&](const Aircraft& ac) {
                   const bool outOfRange = ac.distanceNm > config_.rangeNm;
                   const bool hiddenGround = !config_.showGroundAircraft && ac.onGround.available() && ac.onGround.value;
                   if (outOfRange) ++dropRange;
                   if (hiddenGround) ++dropGround;
                   return outOfRange || hiddenGround;
                 }),
                 aircraft.end());
  if (incomingCount > 0) {
    Serial.printf("Radar ingest filter: incoming=%u kept=%u dropGround=%u dropRange=%u\n", static_cast<unsigned>(incomingCount),
                  static_cast<unsigned>(aircraft.size()), static_cast<unsigned>(dropGround), static_cast<unsigned>(dropRange));
  }
  for (Aircraft& incoming : aircraft) {
    auto old = std::find_if(aircraft_.begin(), aircraft_.end(), [&](const Aircraft& existing) {
      return !incoming.icao24.isEmpty() && existing.icao24 == incoming.icao24;
    });
    if (old == aircraft_.end()) continue;
    incoming.selected = old->selected;
    incoming.tracked = old->tracked;
    if (!incoming.airlineName.available() && old->airlineName.available()) incoming.airlineName = old->airlineName;
    if (!incoming.routeOrigin.available() && old->routeOrigin.available()) incoming.routeOrigin = old->routeOrigin;
    if (!incoming.routeDestination.available() && old->routeDestination.available()) incoming.routeDestination = old->routeDestination;
    if (!incoming.operatorName.available() && old->operatorName.available()) incoming.operatorName = old->operatorName;
    if (!incoming.modelName.available() && old->modelName.available()) incoming.modelName = old->modelName;
    if (!incoming.typeCode.available() && old->typeCode.available()) incoming.typeCode = old->typeCode;
    if (!incoming.registration.available() && old->registration.available()) incoming.registration = old->registration;
    incoming.renderX = old->renderX;
    incoming.renderY = old->renderY;
    incoming.interpolationStartX = old->renderX;
    incoming.interpolationStartY = old->renderY;
    incoming.interpolationStartMs = millis();
    incoming.interpolationDurationMs = kInterpolationDurationMs;
    incoming.trailCount = old->trailCount;
    incoming.trailX = old->trailX;
    incoming.trailY = old->trailY;
    if (config_.trailsEnabled) {
      const double trailDx = old->targetX - (incoming.trailCount > 0 ? incoming.trailX[0] : old->targetX);
      const double trailDy = old->targetY - (incoming.trailCount > 0 ? incoming.trailY[0] : old->targetY);
      const bool addTrailPoint = incoming.trailCount == 0 || (trailDx * trailDx + trailDy * trailDy) >= kTrailMinStepPx * kTrailMinStepPx;
      if (addTrailPoint) {
      const uint8_t count = incoming.trailCount < incoming.trailX.size() ? incoming.trailCount + 1 : incoming.trailCount;
      for (int i = static_cast<int>(count) - 1; i > 0; --i) {
        incoming.trailX[i] = incoming.trailX[i - 1];
        incoming.trailY[i] = incoming.trailY[i - 1];
      }
      incoming.trailX[0] = static_cast<int16_t>(old->targetX);
      incoming.trailY[0] = static_cast<int16_t>(old->targetY);
      incoming.trailCount = count;
      }
    } else {
      incoming.trailCount = 0;
    }
  }
  const uint32_t now = millis();
  for (const Aircraft& old : aircraft_) {
    const bool alreadyPresent = std::find_if(aircraft.begin(), aircraft.end(), [&](const Aircraft& ac) {
                                  return !old.icao24.isEmpty() && ac.icao24 == old.icao24;
                                }) != aircraft.end();
    if (alreadyPresent || old.icao24.isEmpty()) continue;
    const uint32_t lastSeen = aircraftLastSeenMs(old);
    if (lastSeen == 0 || now - lastSeen > kMissingAircraftGraceMs) continue;
    if (old.distanceNm > config_.rangeNm || (!config_.showGroundAircraft && old.onGround.available() && old.onGround.value)) continue;
    Aircraft kept = old;
    kept.stale = true;
    aircraft.push_back(kept);
  }
  std::sort(aircraft.begin(), aircraft.end(), [](const Aircraft& a, const Aircraft& b) {
    if (a.selected != b.selected) return a.selected > b.selected;
    if (a.tracked != b.tracked) return a.tracked > b.tracked;
    return a.distanceNm < b.distanceNm;
  });
  const size_t cap = config_.rangeNm >= 320 ? 180 : 260;
  if (aircraft.size() > cap) aircraft.resize(cap);
  aircraft_ = std::move(aircraft);
  updateProjection();
  unlock();
}

const std::vector<Aircraft>& RadarEngine::visibleAircraft() const { return aircraft_; }

std::vector<Aircraft> RadarEngine::snapshotVisibleAircraft() const {
  lock();
  std::vector<Aircraft> snapshot = aircraft_;
  unlock();
  return snapshot;
}

void RadarEngine::snapshotVisibleAircraft(std::vector<Aircraft>* out) const {
  if (!out) return;
  lock();
  out->assign(aircraft_.begin(), aircraft_.end());
  unlock();
}

bool RadarEngine::mergeEnrichment(const Aircraft& enrichment) {
  bool updated = false;
  lock();
  for (Aircraft& ac : aircraft_) {
    const bool sameIcao = !enrichment.icao24.isEmpty() && ac.icao24 == enrichment.icao24;
    const bool sameCallsign = enrichment.callsign.available() && ac.callsign.available() && ac.callsign.value == enrichment.callsign.value;
    if (!sameIcao && !sameCallsign) continue;
    if (enrichment.airlineName.available()) {
      ac.airlineName = enrichment.airlineName;
      updated = true;
    }
    if (enrichment.routeOrigin.available()) {
      ac.routeOrigin = enrichment.routeOrigin;
      updated = true;
    }
    if (enrichment.routeDestination.available()) {
      ac.routeDestination = enrichment.routeDestination;
      updated = true;
    }
    if (enrichment.modelName.available()) {
      ac.modelName = enrichment.modelName;
      updated = true;
    }
    if (enrichment.typeCode.available()) {
      ac.typeCode = enrichment.typeCode;
      updated = true;
    }
    if (enrichment.operatorName.available()) {
      ac.operatorName = enrichment.operatorName;
      updated = true;
    }
    if (enrichment.registration.available()) {
      ac.registration = enrichment.registration;
      updated = true;
    }
  }
  unlock();
  return updated;
}

size_t RadarEngine::visibleAircraftCount() const {
  lock();
  const size_t count = aircraft_.size();
  unlock();
  return count;
}

AppConfig RadarEngine::snapshotConfig() const {
  lock();
  AppConfig snapshot = config_;
  unlock();
  return snapshot;
}

uint16_t RadarEngine::nextRange() {
  lock();
  config_.rangeNm = config_.rangeNm >= 320 ? 5 : static_cast<uint16_t>(((config_.rangeNm / 5) + 1) * 5);
  updateProjection();
  clearTrailsLocked();
  const uint16_t range = config_.rangeNm;
  unlock();
  return range;
}

uint16_t RadarEngine::previousRange() {
  lock();
  config_.rangeNm = config_.rangeNm <= 5 ? 320 : static_cast<uint16_t>(((config_.rangeNm - 1) / 5) * 5);
  updateProjection();
  clearTrailsLocked();
  const uint16_t range = config_.rangeNm;
  unlock();
  return range;
}

void RadarEngine::setRange(uint16_t rangeNm) {
  lock();
  if (rangeNm >= 5 && rangeNm <= 320 && rangeNm % 5 == 0) {
    config_.rangeNm = rangeNm;
    aircraft_.erase(std::remove_if(aircraft_.begin(), aircraft_.end(), [&](const Aircraft& ac) {
                      return ac.distanceNm > config_.rangeNm || (!config_.showGroundAircraft && ac.onGround.available() && ac.onGround.value);
                    }),
                    aircraft_.end());
    updateProjection();
    clearTrailsLocked();
  }
  unlock();
}

uint16_t RadarEngine::rangeNm() const {
  lock();
  const uint16_t range = config_.rangeNm;
  unlock();
  return range;
}

bool RadarEngine::toggleMap() {
  lock();
  config_.mapEnabled = !config_.mapEnabled;
  const bool enabled = config_.mapEnabled;
  unlock();
  return enabled;
}

void RadarEngine::setMapEnabled(bool enabled) {
  lock();
  config_.mapEnabled = enabled;
  unlock();
}

bool RadarEngine::mapEnabled() const {
  lock();
  const bool enabled = config_.mapEnabled;
  unlock();
  return enabled;
}

void RadarEngine::updateProjection() {
  geo::Location origin {config_.homeLat, config_.homeLon};
  for (Aircraft& ac : aircraft_) {
    if (!ac.latitude.available() || !ac.longitude.available()) continue;
    const auto point = geo::projectAzimuthalEquidistant(origin, {ac.latitude.value, ac.longitude.value}, config_.rangeNm, kProjectionRadiusPx);
    ac.targetX = point.x;
    ac.targetY = point.y;
    if (ac.interpolationDurationMs == 0) {
      ac.renderX = point.x;
      ac.renderY = point.y;
      ac.interpolationStartX = point.x;
      ac.interpolationStartY = point.y;
    }
  }
}

void RadarEngine::clearTrailsLocked() {
  for (Aircraft& ac : aircraft_) {
    ac.trailCount = 0;
    ac.trailX.fill(0);
    ac.trailY.fill(0);
    ac.renderX = ac.targetX;
    ac.renderY = ac.targetY;
    ac.interpolationStartX = ac.targetX;
    ac.interpolationStartY = ac.targetY;
    ac.interpolationDurationMs = 0;
  }
}

void RadarEngine::animate(uint32_t nowMs) {
  lock();
  for (Aircraft& ac : aircraft_) {
    if (ac.interpolationDurationMs == 0) {
      const bool canDeadReckon = ac.groundSpeedKt.available() && ac.groundSpeedKt.value > 5.0 &&
                                (ac.trackDeg.available() || ac.headingDeg.available()) &&
                                (!ac.onGround.available() || !ac.onGround.value) && !ac.stale;
      if (!canDeadReckon) continue;
      const uint32_t baseMs = ac.interpolationStartMs == 0 ? nowMs : ac.interpolationStartMs;
      const uint32_t elapsed = nowMs > baseMs ? nowMs - baseMs : 0;
      if (elapsed == 0 || elapsed > kDeadReckonMaxMs) continue;
      const double headingDeg = ac.trackDeg.available() ? ac.trackDeg.value : ac.headingDeg.value;
      const double distanceNm = ac.groundSpeedKt.value * (static_cast<double>(elapsed) / 3600000.0);
      const double pixelsPerNm = static_cast<double>(kProjectionRadiusPx) / static_cast<double>(max<uint16_t>(1, config_.rangeNm));
      const double distancePx = distanceNm * pixelsPerNm;
      const double rad = headingDeg * kDegToRad;
      ac.renderX = ac.targetX + sin(rad) * distancePx;
      ac.renderY = ac.targetY - cos(rad) * distancePx;
      continue;
    }
    const uint32_t elapsed = nowMs - ac.interpolationStartMs;
    if (elapsed >= ac.interpolationDurationMs) {
      ac.renderX = ac.targetX;
      ac.renderY = ac.targetY;
      ac.interpolationStartX = ac.targetX;
      ac.interpolationStartY = ac.targetY;
      ac.interpolationStartMs = ac.interpolationStartMs + ac.interpolationDurationMs;
      ac.interpolationDurationMs = 0;
      continue;
    }
    const double t = static_cast<double>(elapsed) / static_cast<double>(ac.interpolationDurationMs);
    const double eased = t * t * (3.0 - 2.0 * t);
    ac.renderX = ac.interpolationStartX + (ac.targetX - ac.interpolationStartX) * eased;
    ac.renderY = ac.interpolationStartY + (ac.targetY - ac.interpolationStartY) * eased;
  }
  unlock();
}

bool RadarEngine::selectNearestDisplayPoint(int16_t x, int16_t y) {
  lock();
  int bestIndex = -1;
  int32_t bestScore = 36 * 36;
  for (size_t i = 0; i < aircraft_.size(); ++i) {
    const int16_t ax = static_cast<int16_t>(aircraft_[i].renderX);
    const int16_t ay = static_cast<int16_t>(aircraft_[i].renderY);
    const int32_t dx = ax - x;
    const int32_t dy = ay - y;
    const int32_t d2 = dx * dx + dy * dy;
    int32_t score = d2;
    const int16_t labelLeft = ax + 10;
    const int16_t labelRight = ax + 132;
    const int16_t labelTop = ay - 26;
    const int16_t labelBottom = ay + 46;
    if (x >= labelLeft && x <= labelRight && y >= labelTop && y <= labelBottom) {
      const int32_t ldx = x - ((labelLeft + labelRight) / 2);
      const int32_t ldy = y - ((labelTop + labelBottom) / 2);
      score = min(score, (ldx * ldx + ldy * ldy) / 8);
    }
    if (score < bestScore) {
      bestScore = score;
      bestIndex = static_cast<int>(i);
    }
  }
  for (Aircraft& ac : aircraft_) ac.selected = false;
  if (bestIndex >= 0) {
    aircraft_[bestIndex].selected = true;
  }
  unlock();
  return bestIndex >= 0;
}

void RadarEngine::clearSelection() {
  lock();
  for (Aircraft& ac : aircraft_) {
    ac.selected = false;
    ac.tracked = false;
  }
  unlock();
}

void RadarEngine::toggleTrackSelected() {
  lock();
  for (Aircraft& ac : aircraft_) {
    if (ac.selected) {
      ac.tracked = !ac.tracked;
    } else {
      ac.tracked = false;
    }
  }
  unlock();
}

}  // namespace micro_radar
