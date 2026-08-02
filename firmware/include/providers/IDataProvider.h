#pragma once

#include <Arduino.h>
#include <vector>

#include "aircraft/Aircraft.h"
#include "geo/Geo.h"

namespace micro_radar {

enum class ProviderError : uint8_t {
  None,
  Network,
  Timeout,
  Http,
  RateLimited,
  Auth,
  MalformedJson,
  Oversized,
  Unsupported,
};

struct ProviderCapabilities {
  bool requiresAuth {false};
  bool supportsRadius {true};
  bool supportsRoutes {false};
  uint16_t minPollIntervalSec {5};
  uint16_t maxRadiusNm {320};
  String attribution;
};

struct ProviderStatus {
  ProviderError lastError {ProviderError::None};
  uint32_t retryAfterSec {0};
  uint64_t lastSuccessMs {0};
  String message {"idle"};
};

class IDataProvider {
 public:
  virtual ~IDataProvider() = default;
  virtual bool begin() = 0;
  virtual bool configure(const String& json) = 0;
  virtual bool validateConfiguration(String* error) const = 0;
  virtual bool fetchAircraft(geo::Location origin, uint16_t radiusNm, std::vector<Aircraft>* out) = 0;
  virtual bool fetchOptionalRoute(Aircraft* aircraft) = 0;
  virtual ProviderCapabilities getCapabilities() const = 0;
  virtual ProviderStatus getProviderStatus() const = 0;
  virtual void cancelPendingRequest() = 0;
  virtual ProviderError mapError(int httpCode, const String& body) const = 0;
  virtual void shutdown() = 0;
};

}  // namespace micro_radar
