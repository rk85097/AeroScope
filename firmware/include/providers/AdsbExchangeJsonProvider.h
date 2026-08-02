#pragma once

#include "providers/IDataProvider.h"

#include <Stream.h>

namespace micro_radar {

class AdsbExchangeJsonProvider : public IDataProvider {
 public:
  explicit AdsbExchangeJsonProvider(String baseUrl, String attribution, uint16_t minPollIntervalSec, ProviderId providerId = ProviderId::AdsbLol);
  bool begin() override;
  bool configure(const String& json) override;
  bool validateConfiguration(String* error) const override;
  bool fetchAircraft(geo::Location origin, uint16_t radiusNm, std::vector<Aircraft>* out) override;
  bool fetchOptionalRoute(Aircraft* aircraft) override;
  ProviderCapabilities getCapabilities() const override;
  ProviderStatus getProviderStatus() const override;
  void cancelPendingRequest() override;
  ProviderError mapError(int httpCode, const String& body) const override;
  void shutdown() override;

 protected:
  bool parsePayload(Stream& stream, geo::Location origin, uint16_t radiusNm, std::vector<Aircraft>* out);
  String makeRadiusUrl(geo::Location origin, uint16_t radiusNm) const;

 private:
  String baseUrl_;
  String attribution_;
  ProviderId providerId_ {ProviderId::AdsbLol};
  uint16_t minPollIntervalSec_;
  uint16_t maxRadiusNm_ {250};
  ProviderStatus status_;
};

}  // namespace micro_radar
