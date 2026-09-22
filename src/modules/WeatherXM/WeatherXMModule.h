#pragma once

#include "configuration.h"

#if (defined(WG1200) || defined(HAS_WEATHERXM)) && !MESHTASTIC_EXCLUDE_WEATHERXM

#include "WeatherData.h"
#include "concurrency/Lock.h"
#include "concurrency/LockGuard.h"
#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#include <map>
#include <vector>

class WeatherXMModule : public MeshModule, public Observable<const UIFrameEvent *>, private concurrency::OSThread
{
  public:
    WeatherXMModule();
    virtual ~WeatherXMModule() = default;

    virtual void setup() override;
    virtual int32_t runOnce() override;

    // MeshModule overrides
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
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

    // Station rotation
    void nextStation();
    uint32_t getActiveStationNode() const { return showingStationNode; }
    size_t getActiveStationCount() const { return totalPoolSize; }

    // Telemetry ingestion
    void processRawWsPacket(const uint8_t *payload, size_t length, int16_t rssi = 0, float snr = 0.0f);
    void ingestMeshTelemetry(const meshtastic_MeshPacket &mp, const meshtastic_EnvironmentMetrics &env);

  private:
    struct StationRecord {
        uint32_t nodeNum = 0;
        weatherxm::WeatherData data;
        uint32_t lastHeardMs = 0;
    };

    concurrency::Lock dataLock;
    weatherxm::WeatherData currentData;
    std::map<uint32_t, StationRecord> activeStations;

    uint32_t showingStationNode = 0;
    size_t currentPoolIndex = 0;
    size_t totalPoolSize = 0;
    uint32_t lastFlipMs = 0;
    uint32_t lastSensorPollMs = 0;

    bool useImperial = false;
    bool hasOnboardBmp390 = false;
    float onboardPressureHpa = 0.0f;
    float onboardTempC = 0.0f;

    static constexpr uint32_t FLIP_INTERVAL_MS = 10000;
    static constexpr uint32_t STATION_STALE_TIMEOUT_MS = 30 * 60 * 1000;

    void updateOnboardSensors();
    void syncWithNodeDB();
    std::vector<uint32_t> getActiveRotationPool();
    void updateActiveStationRotation();
    void mapEnvironmentMetricsToWeatherData(uint32_t fromNode, const meshtastic_EnvironmentMetrics &env,
                                            weatherxm::WeatherData &data);

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
