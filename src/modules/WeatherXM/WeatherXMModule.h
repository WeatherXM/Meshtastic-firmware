#pragma once

#include "configuration.h"

#if (defined(WG1200) || defined(HAS_WEATHERXM)) && !MESHTASTIC_EXCLUDE_WEATHERXM

#include "WeatherData.h"
#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "mesh/SinglePortModule.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

class WeatherXMModule : public SinglePortModule, public Observable<const UIFrameEvent *>, private concurrency::OSThread
{
  public:
    WeatherXMModule();
    virtual ~WeatherXMModule() = default;

    virtual void setup() override;
    virtual int32_t runOnce() override;

    // MeshModule overrides
    virtual bool wantUIFrame() override { return true; }
    virtual Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;

    // Data access
    const weatherxm::WeatherData &getData() const { return currentData; }
    weatherxm::WeatherData &getMutableData() { return currentData; }

    // Units toggle
    void toggleUnits();
    bool isImperial() const { return useImperial; }
    void setImperial(bool imperial) { useImperial = imperial; }

    // Weather station packet ingestion
    void processRawWsPacket(const uint8_t *payload, size_t length, int16_t rssi = 0, float snr = 0.0f);

  private:
    weatherxm::WeatherData currentData;
    bool useImperial = false;
    uint32_t lastSensorPollMs = 0;

    void updateOnboardSensors();
    void drawHeader(OLEDDisplay *display, int16_t x, int16_t y);
    void drawTempCard(OLEDDisplay *display, int16_t x, int16_t y);
    void drawRainCard(OLEDDisplay *display, int16_t x, int16_t y);
    void drawBarometerCard(OLEDDisplay *display, int16_t x, int16_t y);
    void drawHumidityCard(OLEDDisplay *display, int16_t x, int16_t y);
    void drawSolarCard(OLEDDisplay *display, int16_t x, int16_t y);
    void drawWindCard(OLEDDisplay *display, int16_t x, int16_t y);
};

extern WeatherXMModule *weatherXMModule;

#endif
