#include "providers/ProviderManager.h"

#include <Arduino.h>
#include <algorithm>
#include <math.h>

namespace micro_radar {
namespace {
constexpr uint32_t kOpenSkyCacheMs = 60000;
constexpr uint32_t kAirplanesCacheMs = 30000;
constexpr uint32_t kFallbackTlsMinFreeHeap = 56000;
constexpr uint32_t kFallbackTlsMinMaxBlock = 30000;
constexpr double kLocationEpsilon = 0.000001;

uint8_t providerPriority(ProviderId provider) {
  switch (provider) {
    case ProviderId::LocalReceiver: return 0;
    case ProviderId::AdsbFi: return 1;
    case ProviderId::AdsbLol: return 2;
    case ProviderId::AirplanesLive: return 3;
    case ProviderId::OpenSky: return 4;
    default: return 9;
  }
}

bool cacheCurrent(uint32_t cacheMs, double cacheLat, double cacheLon, uint16_t cacheRangeNm, geo::Location origin, uint16_t rangeNm, uint32_t ttlMs) {
  return cacheRangeNm == rangeNm && fabs(cacheLat - origin.lat) < kLocationEpsilon && fabs(cacheLon - origin.lon) < kLocationEpsilon &&
         millis() - cacheMs < ttlMs;
}

bool fallbackTlsHeadroomOk() {
  return ESP.getFreeHeap() >= kFallbackTlsMinFreeHeap && ESP.getMaxAllocHeap() >= kFallbackTlsMinMaxBlock;
}

String providerSummary(const char* name, bool ok, const ProviderStatus& status) {
  if (ok) return String(name) + " ok/" + (status.message.isEmpty() ? "ok" : status.message);
  const String message = status.message.isEmpty() ? "no status" : status.message;
  if (message.startsWith("skip") || message.startsWith("not configured") || message.startsWith("cached")) {
    return String(name) + " skipped/" + message;
  }
  return String(name) + " failed/" + message;
}

bool incomingHasFresherPosition(const Aircraft& existing, const Aircraft& incoming) {
  if (!incoming.latitude.available() || !incoming.longitude.available()) return false;
  if (!existing.latitude.available() || !existing.longitude.available()) return true;
  const uint8_t incomingPriority = providerPriority(incoming.provider);
  const uint8_t existingPriority = providerPriority(existing.provider);
  if (incomingPriority < existingPriority) return true;
  if (incoming.messageAgeSec.available() && existing.messageAgeSec.available()) {
    return incoming.messageAgeSec.value + 2 < existing.messageAgeSec.value;
  }
  return incomingPriority == existingPriority;
}

template <typename T>
void fillIfMissing(OptionalField<T>* existing, const OptionalField<T>& incoming) {
  if (existing && !existing->available() && incoming.available()) *existing = incoming;
}

void updateDynamicFields(Aircraft* existing, const Aircraft& incoming) {
  if (!existing) return;
  existing->provider = incoming.provider;
  if (incoming.latitude.available()) existing->latitude = incoming.latitude;
  if (incoming.longitude.available()) existing->longitude = incoming.longitude;
  if (incoming.positionTimestampMs.available()) existing->positionTimestampMs = incoming.positionTimestampMs;
  if (incoming.positionAgeSec.available()) existing->positionAgeSec = incoming.positionAgeSec;
  if (incoming.baroAltitudeFt.available()) existing->baroAltitudeFt = incoming.baroAltitudeFt;
  if (incoming.geomAltitudeFt.available()) existing->geomAltitudeFt = incoming.geomAltitudeFt;
  if (incoming.groundSpeedKt.available()) existing->groundSpeedKt = incoming.groundSpeedKt;
  if (incoming.trueAirspeedKt.available()) existing->trueAirspeedKt = incoming.trueAirspeedKt;
  if (incoming.indicatedAirspeedKt.available()) existing->indicatedAirspeedKt = incoming.indicatedAirspeedKt;
  if (incoming.trackDeg.available()) existing->trackDeg = incoming.trackDeg;
  if (incoming.headingDeg.available()) existing->headingDeg = incoming.headingDeg;
  if (incoming.verticalRateFpm.available()) existing->verticalRateFpm = incoming.verticalRateFpm;
  if (incoming.onGround.available()) existing->onGround = incoming.onGround;
  if (incoming.messageAgeSec.available()) existing->messageAgeSec = incoming.messageAgeSec;
  if (incoming.lastReceivedMs.available()) existing->lastReceivedMs = incoming.lastReceivedMs;
  if (incoming.sourceType.available()) existing->sourceType = incoming.sourceType;
  existing->distanceNm = incoming.distanceNm;
  existing->bearingDeg = incoming.bearingDeg;
  existing->stale = incoming.stale;
}

void mergeAircraft(std::vector<Aircraft>* base, const std::vector<Aircraft>& extra) {
  if (!base) return;
  for (const Aircraft& incoming : extra) {
    auto existing = std::find_if(base->begin(), base->end(), [&](const Aircraft& ac) {
      return !incoming.icao24.isEmpty() && ac.icao24 == incoming.icao24;
    });
    if (existing == base->end()) {
      base->push_back(incoming);
      continue;
    }
    fillIfMissing(&existing->callsign, incoming.callsign);
    fillIfMissing(&existing->registration, incoming.registration);
    fillIfMissing(&existing->typeCode, incoming.typeCode);
    fillIfMissing(&existing->modelName, incoming.modelName);
    fillIfMissing(&existing->operatorName, incoming.operatorName);
    fillIfMissing(&existing->country, incoming.country);
    fillIfMissing(&existing->squawk, incoming.squawk);
    fillIfMissing(&existing->emergency, incoming.emergency);
    fillIfMissing(&existing->baroAltitudeFt, incoming.baroAltitudeFt);
    fillIfMissing(&existing->geomAltitudeFt, incoming.geomAltitudeFt);
    fillIfMissing(&existing->groundSpeedKt, incoming.groundSpeedKt);
    fillIfMissing(&existing->trueAirspeedKt, incoming.trueAirspeedKt);
    fillIfMissing(&existing->indicatedAirspeedKt, incoming.indicatedAirspeedKt);
    fillIfMissing(&existing->trackDeg, incoming.trackDeg);
    fillIfMissing(&existing->headingDeg, incoming.headingDeg);
    fillIfMissing(&existing->verticalRateFpm, incoming.verticalRateFpm);
    fillIfMissing(&existing->onGround, incoming.onGround);
    fillIfMissing(&existing->messageAgeSec, incoming.messageAgeSec);
    fillIfMissing(&existing->lastReceivedMs, incoming.lastReceivedMs);
    if (incomingHasFresherPosition(*existing, incoming)) updateDynamicFields(&(*existing), incoming);
  }
}
}  // namespace

bool ProviderManager::begin() {
  adsbLol_.begin();
  adsbFi_.begin();
  airplanesLive_.begin();
  openSky_.begin();
  localReceiver_.begin();
  active_ = &adsbLol_;
  return true;
}

void ProviderManager::configure(ProviderMode mode, const String& localReceiverUrl) {
  mode_ = mode;
  localReceiver_.configure(localReceiverUrl);
  if (mode_ == ProviderMode::AirplanesLive) active_ = &airplanesLive_;
  else if (mode_ == ProviderMode::OpenSky) active_ = &openSky_;
  else if (mode_ == ProviderMode::AdsbFi) active_ = &adsbFi_;
  else active_ = &adsbLol_;
}

bool ProviderManager::fetch(geo::Location origin, uint16_t rangeNm, std::vector<Aircraft>* out) {
  if (!out) return false;
  if (mode_ == ProviderMode::AdsbFi) {
    active_ = &adsbFi_;
    if (active_->fetchAircraft(origin, rangeNm, out)) {
      status_ = active_->getProviderStatus();
      return true;
    }
    status_ = active_->getProviderStatus();
    return false;
  }
  if (mode_ == ProviderMode::OpenSky) {
    active_ = &openSky_;
    if (active_->fetchAircraft(origin, rangeNm, out)) {
      status_ = active_->getProviderStatus();
      return true;
    }
    status_ = active_->getProviderStatus();
    return false;
  }
  if (mode_ == ProviderMode::AirplanesLive) {
    active_ = &airplanesLive_;
    if (active_->fetchAircraft(origin, rangeNm, out)) {
      status_ = active_->getProviderStatus();
      return true;
    }
    status_ = active_->getProviderStatus();
    return false;
  }
  if (mode_ == ProviderMode::AdsbLol) {
    active_ = &adsbLol_;
    if (active_->fetchAircraft(origin, rangeNm, out)) {
      status_ = active_->getProviderStatus();
      return true;
    }
    status_ = active_->getProviderStatus();
    return false;
  }
  std::vector<Aircraft> primary;
  const bool primaryOk = adsbLol_.fetchAircraft(origin, rangeNm, &primary);
  const ProviderStatus primaryStatus = adsbLol_.getProviderStatus();

  out->clear();
  bool localOk = false;
  ProviderStatus localStatus;
  if (localReceiver_.configured()) {
    std::vector<Aircraft> local;
    localOk = localReceiver_.fetchAircraft(origin, rangeNm, &local);
    localStatus = localReceiver_.getProviderStatus();
    if (localOk) *out = std::move(local);
  } else {
    localStatus.message = "not configured";
  }
  if (primaryOk) mergeAircraft(out, primary);

  std::vector<Aircraft> adsbFi;
  bool adsbFiOk = false;
  ProviderStatus adsbFiStatus;
  if (fallbackTlsHeadroomOk()) {
    adsbFiOk = adsbFi_.fetchAircraft(origin, rangeNm, &adsbFi);
    adsbFiStatus = adsbFi_.getProviderStatus();
    if (adsbFiOk) mergeAircraft(out, adsbFi);
  } else {
    adsbFiStatus.message = "skip low heap";
  }

  bool openSkyOk = false;
  ProviderStatus openSkyStatus;
  bool secondaryOk = false;
  ProviderStatus secondaryStatus;
  std::vector<Aircraft> openSky;
  const bool openSkyCacheCurrent = !openSkyCache_.empty() && cacheCurrent(openSkyCacheMs_, openSkyCacheLat_, openSkyCacheLon_, openSkyCacheRangeNm_, origin, rangeNm, kOpenSkyCacheMs);
  const bool openSkyAttemptDue = openSkyLastAttemptMs_ == 0 || millis() - openSkyLastAttemptMs_ >= kOpenSkyCacheMs;
  const bool shouldTryOpenSky = (out->empty() || (!primaryOk && !adsbFiOk) || openSkyAttemptDue) && openSkyAttemptDue && fallbackTlsHeadroomOk();
  if (openSkyCacheCurrent) {
    openSky = openSkyCache_;
    openSkyOk = true;
    openSkyStatus = openSky_.getProviderStatus();
    openSkyStatus.lastError = ProviderError::None;
    openSkyStatus.message = "cached " + String(openSky.size());
  } else if (shouldTryOpenSky) {
    openSkyLastAttemptMs_ = millis();
    openSkyOk = openSky_.fetchAircraft(origin, rangeNm, &openSky);
    openSkyStatus = openSky_.getProviderStatus();
    if (openSkyOk) {
      openSkyCache_ = openSky;
      openSkyCacheMs_ = millis();
      openSkyCacheLat_ = origin.lat;
      openSkyCacheLon_ = origin.lon;
      openSkyCacheRangeNm_ = rangeNm;
    }
  } else {
    openSkyStatus.message = openSkyAttemptDue && !fallbackTlsHeadroomOk() ? "skip low heap" : "skip not due or not needed";
  }
  if (openSkyOk) mergeAircraft(out, openSky);

  const bool airplanesCacheCurrent = !airplanesCache_.empty() && cacheCurrent(airplanesCacheMs_, airplanesCacheLat_, airplanesCacheLon_, airplanesCacheRangeNm_, origin, rangeNm, kAirplanesCacheMs);
  if (airplanesCacheCurrent) {
    secondaryOk = true;
    secondaryStatus = airplanesLive_.getProviderStatus();
    secondaryStatus.lastError = ProviderError::None;
    secondaryStatus.message = "cached " + String(airplanesCache_.size());
    mergeAircraft(out, airplanesCache_);
  } else if (((!localOk && !primaryOk && !adsbFiOk && !openSkyOk) || out->empty()) && fallbackTlsHeadroomOk() &&
             (airplanesLastAttemptMs_ == 0 || millis() - airplanesLastAttemptMs_ >= kAirplanesCacheMs)) {
    std::vector<Aircraft> secondary;
    airplanesLastAttemptMs_ = millis();
    secondaryOk = airplanesLive_.fetchAircraft(origin, rangeNm, &secondary);
    secondaryStatus = airplanesLive_.getProviderStatus();
    if (secondaryOk) {
      airplanesCache_ = secondary;
      airplanesCacheMs_ = millis();
      airplanesCacheLat_ = origin.lat;
      airplanesCacheLon_ = origin.lon;
      airplanesCacheRangeNm_ = rangeNm;
      mergeAircraft(out, secondary);
    }
  } else {
    secondaryStatus.message = !fallbackTlsHeadroomOk() ? "skip low heap" : "skip not needed; primary providers returned data";
  }

  if (localOk || primaryOk || adsbFiOk || secondaryOk || openSkyOk) {
    active_ = localOk      ? static_cast<IDataProvider*>(&localReceiver_)
              : openSkyOk  ? static_cast<IDataProvider*>(&openSky_)
              : adsbFiOk   ? static_cast<IDataProvider*>(&adsbFi_)
              : primaryOk  ? static_cast<IDataProvider*>(&adsbLol_)
                            : static_cast<IDataProvider*>(&airplanesLive_);
    status_ = localOk ? localStatus : openSkyOk ? openSkyStatus : adsbFiOk ? adsbFiStatus : primaryOk ? primaryStatus : secondaryStatus;
    status_.lastError = ProviderError::None;
    status_.lastSuccessMs = millis();
    status_.message = "auto merged " + providerSummary("Local", localOk, localStatus) +
                      ", " + providerSummary("ADSB.lol", primaryOk, primaryStatus) +
                      ", " + providerSummary("ADSB.fi", adsbFiOk, adsbFiStatus) +
                      ", " + providerSummary("Airplanes.live", secondaryOk, secondaryStatus) +
                      ", " + providerSummary("OpenSky", openSkyOk, openSkyStatus) +
                      ", total " + String(out->size());
    return true;
  }
  status_ = openSkyStatus.lastError != ProviderError::None ? openSkyStatus : secondaryStatus.lastError != ProviderError::None ? secondaryStatus : primaryStatus;
  return false;
}

ProviderStatus ProviderManager::status() const { return status_; }

}  // namespace micro_radar
