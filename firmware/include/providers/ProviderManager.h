#pragma once

#include <memory>
#include <vector>

#include "config/Config.h"
#include "providers/AdsbExchangeJsonProvider.h"
#include "providers/LocalDump1090Provider.h"
#include "providers/OpenSkyProvider.h"

namespace micro_radar {

class ProviderManager {
 public:
  bool begin();
  void configure(ProviderMode mode, const String& localReceiverUrl = "");
  bool fetch(geo::Location origin, uint16_t rangeNm, std::vector<Aircraft>* out);
  ProviderStatus status() const;

 private:
  ProviderMode mode_ {ProviderMode::Automatic};
  AdsbExchangeJsonProvider adsbLol_ {"http://api.adsb.lol/v2", "ADSB.lol ODbL 1.0", 5, ProviderId::AdsbLol};
  AdsbExchangeJsonProvider adsbFi_ {"https://opendata.adsb.fi/api/v3", "ADSB.fi open data", 1, ProviderId::AdsbFi};
  AdsbExchangeJsonProvider airplanesLive_ {"https://api.airplanes.live/v2", "Airplanes.live non-commercial API", 1, ProviderId::AirplanesLive};
  OpenSkyProvider openSky_;
  LocalDump1090Provider localReceiver_;
  IDataProvider* active_ {&adsbLol_};
  ProviderStatus status_;
  std::vector<Aircraft> openSkyCache_;
  uint32_t openSkyCacheMs_ {0};
  uint32_t openSkyLastAttemptMs_ {0};
  double openSkyCacheLat_ {999.0};
  double openSkyCacheLon_ {999.0};
  uint16_t openSkyCacheRangeNm_ {0};
  std::vector<Aircraft> airplanesCache_;
  uint32_t airplanesCacheMs_ {0};
  uint32_t airplanesLastAttemptMs_ {0};
  double airplanesCacheLat_ {999.0};
  double airplanesCacheLon_ {999.0};
  uint16_t airplanesCacheRangeNm_ {0};
};

}  // namespace micro_radar
