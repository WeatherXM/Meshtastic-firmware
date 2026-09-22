#pragma once

#include "configuration.h"

#if HAS_TFT && (defined(WG1200) || defined(HAS_WEATHERXM)) && !MESHTASTIC_EXCLUDE_WEATHERXM

class WeatherXMTFTView
{
  public:
    static void init();
    static void show();
    static void update();

  private:
    static void createUI();
    static void updateTelemetryUI();
};

#endif
