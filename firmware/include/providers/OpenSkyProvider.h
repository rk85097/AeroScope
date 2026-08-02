#pragma once

#include "providers/IDataProvider.h"

#include <Stream.h>

namespace micro_radar {

class OpenSkyProvider : public IDataProvider {
 public:
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

 private:
  String makeBoundsUrl(geo::Location origin, uint16_t radiusNm) const;
  bool parsePayload(Stream& stream, geo::Location origin, uint16_t radiusNm, std::vector<Aircraft>* out);

  ProviderStatus status_;
};

}  // namespace micro_radar
