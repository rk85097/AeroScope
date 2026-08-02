#pragma once

#include <ESPAsyncWebServer.h>

#include "config/Config.h"
#include "map/MapPackage.h"
#include "providers/ProviderManager.h"
#include "radar/RadarEngine.h"

namespace micro_radar {

class WebServerApp {
 public:
  void begin(ConfigManager* config, RadarEngine* radar, MapPackageManager* maps, ProviderManager* providers);

 private:
  AsyncWebServer server_ {80};
  ConfigManager* config_ {nullptr};
  RadarEngine* radar_ {nullptr};
  MapPackageManager* maps_ {nullptr};
  ProviderManager* providers_ {nullptr};
};

}  // namespace micro_radar
