#pragma once

#include "providers/IDataProvider.h"

#include <Stream.h>

namespace micro_radar {

class LocalDump1090Provider : public IDataProvider {
 public:
  bool begin() override;
  bool configure(const String& url) override;
  bool validateConfiguration(String* error) const override;
  bool fetchAircraft(geo::Location origin, uint16_t radiusNm, std::vector<Aircraft>* out) override;
  bool fetchOptionalRoute(Aircraft* aircraft) override;
  ProviderCapabilities getCapabilities() const override;
  ProviderStatus getProviderStatus() const override;
  void cancelPendingRequest() override;
  ProviderError mapError(int httpCode, const String& body) const override;
  void shutdown() override;
  bool configured() const;

 private:
  bool parsePayload(Stream& stream, geo::Location origin, uint16_t radiusNm, std::vector<Aircraft>* out);

  String url_;
  ProviderStatus status_;
};

}  // namespace micro_radar
