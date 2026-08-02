#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config/Config.h"
#include "map/MapPackage.h"
#include "providers/ProviderManager.h"
#include "radar/RadarEngine.h"
#include "ui/RadarUi.h"
#include "web/WebServerApp.h"

namespace micro_radar {

class Application {
 public:
  void begin();
  void loop();

 private:
  static void providerTaskEntry(void* arg);
  static void mapTaskEntry(void* arg);
  static void enrichmentTaskEntry(void* arg);
  void providerTask();
  void mapTask();
  void enrichmentTask();

  ConfigManager config_;
  RadarEngine radar_;
  ProviderManager providers_;
  MapPackageManager maps_;
  RadarUi ui_;
  WebServerApp web_;
  TaskHandle_t providerTaskHandle_ {nullptr};
  TaskHandle_t mapTaskHandle_ {nullptr};
  TaskHandle_t enrichmentTaskHandle_ {nullptr};
  SemaphoreHandle_t networkMutex_ {nullptr};
};

}  // namespace micro_radar
