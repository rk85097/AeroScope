#pragma once

#include "config/Config.h"
#include "map/MapPackage.h"
#include "radar/RadarEngine.h"

namespace micro_radar {

class RadarUi {
 public:
  bool begin(RadarEngine* radar, ConfigManager* config, MapPackageManager* maps);
  void tick();
  void invalidate();

 private:
  void handleTouch(uint32_t now);
  void persistRadarConfig();
  bool touchInDetailDismiss(uint16_t x, uint16_t y) const;
  bool openAircraftDetailAt(uint16_t x, uint16_t y, uint32_t now);

  RadarEngine* radar_ {nullptr};
  ConfigManager* config_ {nullptr};
  MapPackageManager* maps_ {nullptr};
  uint32_t lastFrameMs_ {0};
  uint32_t lastTouchPollMs_ {0};
  uint32_t touchDownMs_ {0};
  uint32_t lastTouchActionMs_ {0};
  uint32_t lastTapMs_ {0};
  uint16_t touchStartX_ {0};
  uint16_t touchStartY_ {0};
  uint16_t lastTouchX_ {0};
  uint16_t lastTouchY_ {0};
  String selectedIcao_;
  bool dragConsumed_ {false};
  bool touchWasDown_ {false};
  bool touchStartedInRange_ {false};
  bool detailOpen_ {false};
};

}  // namespace micro_radar
