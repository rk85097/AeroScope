#include <Arduino.h>

#include "app/Application.h"
#include "display/SpotpearSmokeDisplay.h"

micro_radar::Application app;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("AeroScope boot");
  Serial.printf("Version: %s\n", MICRO_RADAR_VERSION);
  Serial.printf("Reset reason CPU0: %d\n", static_cast<int>(esp_reset_reason()));
  Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("Heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("PSRAM free: %u bytes\n", ESP.getFreePsram());
  Serial.println("Early display bring-up");
  microRadarSmokeBegin();
  app.begin();
  Serial.println("Application started");
}

void loop() {
  app.loop();
}
