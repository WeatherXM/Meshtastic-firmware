#include "WeatherXMModule.h"

#if (defined(WG1200) || defined(HAS_WEATHERXM)) && !MESHTASTIC_EXCLUDE_WEATHERXM

#include "DebugConfiguration.h"
#include "UptimeClock.h"
#include "decoders/WsDecoders.h"
#include "graphics/ScreenFonts.h"
#include "mesh/NodeDB.h"
#include "mesh/Throttle.h"
#include "mesh/mesh-pb-constants.h"
#include <cmath>
#include <cstdio>

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_BMP3XX.h>)
#include "modules/Telemetry/Sensor/BMP3XXSensor.h"
#endif

WeatherXMModule *weatherXMModule = nullptr;

WeatherXMModule::WeatherXMModule() : MeshModule("Weather", meshtastic_PortNum_TELEMETRY_APP), concurrency::OSThread("WeatherXM")
{
    isPromiscuous = true;
}

bool WeatherXMModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p->decoded.portnum == meshtastic_PortNum_TELEMETRY_APP || p->decoded.portnum == meshtastic_PortNum_PRIVATE_APP;
}

ProcessMessage WeatherXMModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.which_payload_variant != meshtastic_MeshPacket_decoded_tag) {
        return ProcessMessage::CONTINUE;
    }

    if (mp.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP) {
        meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
        if (pb_decode_from_bytes(mp.decoded.payload.bytes, mp.decoded.payload.size, &meshtastic_Telemetry_msg, &telemetry)) {
            if (telemetry.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
                ingestMeshTelemetry(mp, telemetry.variant.environment_metrics);
            }
        }
    } else if (mp.decoded.portnum == meshtastic_PortNum_PRIVATE_APP) {
        processRawWsPacket(mp.decoded.payload.bytes, mp.decoded.payload.size, mp.rx_rssi, mp.rx_snr);
    }

    return ProcessMessage::CONTINUE;
}

void WeatherXMModule::setup()
{
    LOG_INFO("WeatherXMModule initialized");
    updateOnboardSensors();
    syncWithNodeDB();
    updateActiveStationRotation();
    setInterval(2000);
}

int32_t WeatherXMModule::runOnce()
{
    if (Throttle::hasElapsed(lastSensorPollMs, 3000)) {
        lastSensorPollMs = Time::getMillis();
        updateOnboardSensors();
    }
    updateActiveStationRotation();
    return 2000;
}

void WeatherXMModule::nextStation()
{
    concurrency::LockGuard guard(&dataLock);
    std::vector<uint32_t> pool = getActiveRotationPool();
    if (pool.size() > 1) {
        currentPoolIndex = (currentPoolIndex + 1) % pool.size();
        lastFlipMs = Time::getMillis();
        showingStationNode = pool[currentPoolIndex];
        auto it = activeStations.find(showingStationNode);
        if (it != activeStations.end()) {
            currentData = it->second.data;
            if (currentData.barometric_pressure <= 0.0f && hasOnboardBmp390) {
                currentData.barometric_pressure = onboardPressureHpa;
                currentData.has_bmp390 = true;
            }
        }
        UIFrameEvent e;
        e.action = UIFrameEvent::REDRAW_ONLY;
        notifyObservers(&e);
    }
}

void WeatherXMModule::updateOnboardSensors()
{
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_BMP3XX.h>)
    BMP3XXSingleton *bmp = BMP3XXSingleton::GetInstance();
    if (bmp && bmp->performReading()) {
        float press_hpa = bmp->pressure / 100.0f;
        float temp_c = bmp->temperature;

        concurrency::LockGuard guard(&dataLock);
        hasOnboardBmp390 = true;
        onboardPressureHpa = press_hpa;
        onboardTempC = temp_c;

        bool changed = (fabsf(currentData.barometric_pressure - press_hpa) > 0.05f);

        if (!currentData.has_station_data) {
            currentData.barometric_pressure = press_hpa;
            currentData.has_bmp390 = true;
            currentData.temperature = temp_c;
            currentData.feels_like = temp_c;
            currentData.humidity = 0.0f;
            currentData.dew_point = temp_c;
            currentData.updateExtremes();
        } else if (currentData.barometric_pressure <= 0.0f) {
            currentData.barometric_pressure = press_hpa;
            currentData.has_bmp390 = true;
        }

        if (changed && !currentData.has_station_data) {
            UIFrameEvent e;
            e.action = UIFrameEvent::REDRAW_ONLY;
            notifyObservers(&e);
        }
    }
#endif
}

void WeatherXMModule::syncWithNodeDB()
{
    if (!nodeDB)
        return;

    auto envNodes = nodeDB->snapshotEnvironmentNodeNums(0);
    for (uint32_t n : envNodes) {
        if (activeStations.find(n) == activeStations.end()) {
            meshtastic_EnvironmentMetrics env = meshtastic_EnvironmentMetrics_init_zero;
            if (nodeDB->copyNodeEnvironment(n, env)) {
                StationRecord &rec = activeStations[n];
                rec.nodeNum = n;
                rec.lastHeardMs = Time::getMillis();
                mapEnvironmentMetricsToWeatherData(n, env, rec.data);
                const auto *node = nodeDB->getMeshNode(n);
                if (node && nodeInfoLiteHasUser(node) && node->long_name[0]) {
                    snprintf(rec.data.station_name, sizeof(rec.data.station_name), "%s", node->long_name);
                } else if (node && nodeInfoLiteHasUser(node) && node->short_name[0]) {
                    snprintf(rec.data.station_name, sizeof(rec.data.station_name), "%s", node->short_name);
                } else {
                    snprintf(rec.data.station_name, sizeof(rec.data.station_name), "!%08x", (unsigned int)n);
                }
                rec.data.station_id = n;
                rec.data.has_station_data = true;
                rec.data.last_packet_time_ms = rec.lastHeardMs;
            }
        }
    }
}

std::vector<uint32_t> WeatherXMModule::getActiveRotationPool()
{
    syncWithNodeDB();

    std::vector<uint32_t> favorites;
    std::vector<uint32_t> allStations;

    for (auto it = activeStations.begin(); it != activeStations.end();) {
        if (Throttle::hasElapsed(it->second.lastHeardMs, STATION_STALE_TIMEOUT_MS)) {
            it = activeStations.erase(it);
            continue;
        }

        uint32_t nodeNum = it->first;
        allStations.push_back(nodeNum);
        if (nodeDB && nodeDB->isFavorite(nodeNum)) {
            favorites.push_back(nodeNum);
        }
        ++it;
    }

    if (!favorites.empty()) {
        return favorites;
    }

    return allStations;
}

void WeatherXMModule::updateActiveStationRotation()
{
    concurrency::LockGuard guard(&dataLock);
    std::vector<uint32_t> pool = getActiveRotationPool();

    if (pool.empty()) {
        showingStationNode = 0;
        totalPoolSize = 0;
        currentPoolIndex = 0;
        currentData.has_station_data = false;
        currentData.station_name[0] = '\0';
        currentData.station_id = 0;
        if (hasOnboardBmp390) {
            currentData.barometric_pressure = onboardPressureHpa;
            currentData.temperature = onboardTempC;
            currentData.feels_like = onboardTempC;
            currentData.dew_point = onboardTempC;
            currentData.has_bmp390 = true;
        }
        return;
    }

    bool flipped = false;
    totalPoolSize = pool.size();

    if (totalPoolSize > 1) {
        if (lastFlipMs == 0) {
            lastFlipMs = Time::getMillis();
        } else if (Throttle::hasElapsed(lastFlipMs, FLIP_INTERVAL_MS)) {
            lastFlipMs = Time::getMillis();
            currentPoolIndex = (currentPoolIndex + 1) % totalPoolSize;
            flipped = true;
        }
    }

    if (currentPoolIndex >= totalPoolSize) {
        currentPoolIndex = 0;
        flipped = true;
    }

    uint32_t selectedNode = pool[currentPoolIndex];
    if (selectedNode != showingStationNode || flipped) {
        showingStationNode = selectedNode;
        auto it = activeStations.find(showingStationNode);
        if (it != activeStations.end()) {
            currentData = it->second.data;
            if (currentData.barometric_pressure <= 0.0f && hasOnboardBmp390) {
                currentData.barometric_pressure = onboardPressureHpa;
                currentData.has_bmp390 = true;
            }
        }
        if (flipped) {
            UIFrameEvent e;
            e.action = UIFrameEvent::REDRAW_ONLY;
            notifyObservers(&e);
        }
    }
}

void WeatherXMModule::mapEnvironmentMetricsToWeatherData(uint32_t fromNode, const meshtastic_EnvironmentMetrics &env,
                                                         weatherxm::WeatherData &data)
{
    if (env.has_temperature) {
        data.temperature = env.temperature;
    }
    if (env.has_relative_humidity) {
        data.humidity = env.relative_humidity;
    }
    if (env.has_barometric_pressure) {
        data.barometric_pressure = env.barometric_pressure;
    } else if (hasOnboardBmp390) {
        data.barometric_pressure = onboardPressureHpa;
    }
    if (env.has_wind_speed) {
        data.wind_speed = env.wind_speed;
    }
    if (env.has_wind_gust) {
        data.wind_gust = env.wind_gust;
    }
    if (env.has_wind_direction) {
        data.wind_direction = (uint16_t)(env.wind_direction % 360);
    }
    if (env.has_rainfall_1h) {
        data.precipitation_rate = env.rainfall_1h;
    }
    if (env.has_rainfall_24h) {
        data.precipitation_accum = env.rainfall_24h;
    }
    if (env.has_lux) {
        data.solar_radiation = env.lux / 126.7f;
    } else if (env.has_white_lux) {
        data.solar_radiation = env.white_lux / 126.7f;
    }
    if (env.has_uv_lux) {
        data.uv_index = (uint8_t)fminf(fmaxf(env.uv_lux, 0.0f), 15.0f);
    }

    data.dew_point = weatherxm::WeatherData::computeDewPoint(data.temperature, data.humidity);
    data.feels_like = weatherxm::WeatherData::computeFeelsLike(data.temperature, data.humidity, data.wind_speed);
    data.updateExtremes();
}

void WeatherXMModule::ingestMeshTelemetry(const meshtastic_MeshPacket &mp, const meshtastic_EnvironmentMetrics &env)
{
    uint32_t fromNode = getFrom(&mp);
    if (!fromNode)
        return;

    concurrency::LockGuard guard(&dataLock);
    StationRecord &rec = activeStations[fromNode];
    rec.nodeNum = fromNode;
    rec.lastHeardMs = Time::getMillis();

    mapEnvironmentMetricsToWeatherData(fromNode, env, rec.data);

    rec.data.station_id = fromNode;
    const auto *node = nodeDB ? nodeDB->getMeshNode(fromNode) : nullptr;
    if (node && nodeInfoLiteHasUser(node) && node->long_name[0]) {
        snprintf(rec.data.station_name, sizeof(rec.data.station_name), "%s", node->long_name);
    } else if (node && nodeInfoLiteHasUser(node) && node->short_name[0]) {
        snprintf(rec.data.station_name, sizeof(rec.data.station_name), "%s", node->short_name);
    } else {
        snprintf(rec.data.station_name, sizeof(rec.data.station_name), "!%08x", (unsigned int)fromNode);
    }

    rec.data.rssi = mp.rx_rssi;
    rec.data.snr = mp.rx_snr;
    rec.data.last_packet_time_ms = rec.lastHeardMs;
    rec.data.packet_count++;
    rec.data.has_station_data = true;

    LOG_INFO("WeatherXM received telemetry from %s (!%08x): T=%.1f C, H=%.0f %%, Wind=%.1f m/s", rec.data.station_name,
             (unsigned int)fromNode, rec.data.temperature, rec.data.humidity, rec.data.wind_speed);

    if (showingStationNode == fromNode || showingStationNode == 0) {
        showingStationNode = fromNode;
        currentData = rec.data;
        if (currentData.barometric_pressure <= 0.0f && hasOnboardBmp390) {
            currentData.barometric_pressure = onboardPressureHpa;
            currentData.has_bmp390 = true;
        }
        UIFrameEvent e;
        e.action = UIFrameEvent::REDRAW_ONLY;
        notifyObservers(&e);
    }
}

void WeatherXMModule::toggleUnits()
{
    useImperial = !useImperial;
    LOG_INFO("WeatherXM units toggled: %s", useImperial ? "Imperial" : "Metric");
    UIFrameEvent e;
    e.action = UIFrameEvent::REDRAW_ONLY;
    notifyObservers(&e);
}

void WeatherXMModule::processRawWsPacket(const uint8_t *payload, size_t length, int16_t rssi, float snr)
{
    if (!payload || length == 0)
        return;

    concurrency::LockGuard guard(&dataLock);
    bool decoded = false;
    weatherxm::WeatherData wsData;

    if (length >= 26 || (length >= 16 && payload[0] <= length)) {
        decoded = weatherxm::WsDecoders::decodeWs1001(payload, length, wsData, rssi, snr);
    }
    if (!decoded && length >= 15) {
        decoded = weatherxm::WsDecoders::decodeWs1300(payload, length, wsData, rssi, snr);
    }

    if (decoded) {
        uint32_t stationKey = wsData.station_id ? wsData.station_id : 0x57530001;
        StationRecord &rec = activeStations[stationKey];
        rec.nodeNum = stationKey;
        rec.lastHeardMs = Time::getMillis();
        rec.data = wsData;
        rec.data.rssi = rssi;
        rec.data.snr = snr;
        rec.data.last_packet_time_ms = rec.lastHeardMs;
        rec.data.packet_count++;
        rec.data.has_station_data = true;
        rec.data.updateExtremes();

        LOG_INFO("WeatherXM packet #%lu decoded: %s (T=%.1f C, H=%.0f %%, Wind=%.1f m/s)", (unsigned long)rec.data.packet_count,
                 rec.data.station_name, rec.data.temperature, rec.data.humidity, rec.data.wind_speed);

        if (showingStationNode == stationKey || showingStationNode == 0) {
            showingStationNode = stationKey;
            currentData = rec.data;
            if (currentData.barometric_pressure <= 0.0f && hasOnboardBmp390) {
                currentData.barometric_pressure = onboardPressureHpa;
                currentData.has_bmp390 = true;
            }
            UIFrameEvent e;
            e.action = UIFrameEvent::REDRAW_ONLY;
            notifyObservers(&e);
        }
    } else {
        LOG_WARN("WeatherXM packet CRC/checksum failed (len=%zu)", length);
    }
}

void WeatherXMModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    updateActiveStationRotation();

    display->setColor(WHITE);
    drawHeader(display, x, y);
    drawTempCard(display, x, y);
    drawRainCard(display, x, y);
    drawBarometerCard(display, x, y);
    drawHumidityCard(display, x, y);
    drawSolarCard(display, x, y);
    drawWindCard(display, x, y);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + 240, y + 458, useImperial ? "[ Imperial Units ]" : "[ Metric Units ]");
}

void WeatherXMModule::drawHeader(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->setFont(FONT_MEDIUM);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 12, y + 8, "WEATHERXM");

    display->setFont(FONT_SMALL);
    if (currentData.has_station_data && currentData.station_name[0]) {
        bool isFav = (showingStationNode && nodeDB && nodeDB->isFavorite(showingStationNode));
        char nameBuf[48];
        if (totalPoolSize > 1) {
            snprintf(nameBuf, sizeof(nameBuf), "%s%s (%u/%u)", isFav ? "[FAV] " : "", currentData.station_name,
                     (unsigned int)(currentPoolIndex + 1), (unsigned int)totalPoolSize);
        } else if (isFav) {
            snprintf(nameBuf, sizeof(nameBuf), "[FAV] %s", currentData.station_name);
        } else {
            snprintf(nameBuf, sizeof(nameBuf), "%s", currentData.station_name);
        }
        display->drawString(x + 175, y + 14, nameBuf);
    } else {
        display->drawString(x + 175, y + 14, "MESHTASTIC NODE");
    }

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    if (currentData.has_station_data) {
        char pktBuf[28];
        snprintf(pktBuf, sizeof(pktBuf), "#%lu", (unsigned long)currentData.packet_count);
        display->drawString(x + 468, y + 14, pktBuf);
    } else {
        display->drawString(x + 468, y + 14, currentData.has_bmp390 ? "BMP390 ACTIVE" : "STANDBY");
    }

    display->drawLine(x + 10, y + 42, x + 470, y + 42);
}

void WeatherXMModule::drawTempCard(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawRect(x + 10, y + 50, 295, 230);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 22, y + 60, "TEMPERATURE");

    float disp_temp = useImperial ? weatherxm::WeatherData::toFahrenheit(currentData.temperature) : currentData.temperature;
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", disp_temp);

    display->setFont(FONT_LARGE);
    display->drawString(x + 25, y + 88, tempBuf);

    display->setFont(FONT_MEDIUM);
    display->drawString(x + 175, y + 92, useImperial ? "°F" : "°C");

    display->drawLine(x + 20, y + 145, x + 295, y + 145);

    display->setFont(FONT_SMALL);
    float disp_fl = useImperial ? weatherxm::WeatherData::toFahrenheit(currentData.feels_like) : currentData.feels_like;
    float disp_dp = useImperial ? weatherxm::WeatherData::toFahrenheit(currentData.dew_point) : currentData.dew_point;

    char flBuf[32];
    snprintf(flBuf, sizeof(flBuf), "Feels like: %.1f %s", disp_fl, useImperial ? "°F" : "°C");
    display->drawString(x + 22, y + 155, flBuf);

    char dpBuf[32];
    snprintf(dpBuf, sizeof(dpBuf), "Dew point:  %.1f %s", disp_dp, useImperial ? "°F" : "°C");
    display->drawString(x + 22, y + 180, dpBuf);

    if (currentData.temp_min < 900.0f && currentData.temp_max > -900.0f) {
        float disp_min = useImperial ? weatherxm::WeatherData::toFahrenheit(currentData.temp_min) : currentData.temp_min;
        float disp_max = useImperial ? weatherxm::WeatherData::toFahrenheit(currentData.temp_max) : currentData.temp_max;
        char mmBuf[40];
        snprintf(mmBuf, sizeof(mmBuf), "Min: %.1f°  Max: %.1f°", disp_min, disp_max);
        display->drawString(x + 22, y + 215, mmBuf);
    }
}

void WeatherXMModule::drawRainCard(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawRect(x + 315, y + 50, 155, 110);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 325, y + 58, "PRECIPITATION");

    float disp_rate =
        useImperial ? weatherxm::WeatherData::toInches(currentData.precipitation_rate) : currentData.precipitation_rate;
    float disp_acc =
        useImperial ? weatherxm::WeatherData::toInches(currentData.precipitation_accum) : currentData.precipitation_accum;

    char rateBuf[24];
    snprintf(rateBuf, sizeof(rateBuf), "Rate: %.1f %s", disp_rate, useImperial ? "in/h" : "mm/h");
    display->drawString(x + 325, y + 84, rateBuf);

    char accBuf[24];
    snprintf(accBuf, sizeof(accBuf), "Total: %.1f %s", disp_acc, useImperial ? "in" : "mm");
    display->drawString(x + 325, y + 115, accBuf);
}

void WeatherXMModule::drawBarometerCard(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawRect(x + 315, y + 170, 155, 110);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 325, y + 178, "BAROMETER");

    float disp_press =
        useImperial ? weatherxm::WeatherData::toInHg(currentData.barometric_pressure) : currentData.barometric_pressure;

    char pressBuf[24];
    snprintf(pressBuf, sizeof(pressBuf), "%.1f %s", disp_press, useImperial ? "inHg" : "hPa");
    display->setFont(FONT_MEDIUM);
    display->drawString(x + 325, y + 202, pressBuf);

    display->setFont(FONT_SMALL);
    display->drawString(x + 325, y + 242, currentData.has_bmp390 ? "BMP390 ACTIVE" : "NO SENSOR");
}

void WeatherXMModule::drawHumidityCard(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawRect(x + 10, y + 290, 145, 155);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 20, y + 298, "HUMIDITY");

    char humBuf[16];
    snprintf(humBuf, sizeof(humBuf), "%.0f %%", currentData.humidity);
    display->setFont(FONT_MEDIUM);
    display->drawString(x + 20, y + 328, humBuf);

    display->setFont(FONT_SMALL);
    const char *comfort = "Normal";
    if (currentData.humidity < 30.0f)
        comfort = "Dry";
    else if (currentData.humidity > 70.0f)
        comfort = "Humid";
    display->drawString(x + 20, y + 368, comfort);

    if (currentData.hum_min < 900.0f && currentData.hum_max > -900.0f) {
        char hExtBuf[24];
        snprintf(hExtBuf, sizeof(hExtBuf), "%.0f%% - %.0f%%", currentData.hum_min, currentData.hum_max);
        display->drawString(x + 20, y + 402, hExtBuf);
    }
}

void WeatherXMModule::drawSolarCard(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawRect(x + 165, y + 290, 140, 155);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 175, y + 298, "SOLAR & UV");

    char radBuf[24];
    snprintf(radBuf, sizeof(radBuf), "%.0f W/m²", currentData.solar_radiation);
    display->drawString(x + 175, y + 328, radBuf);

    char uvBuf[16];
    snprintf(uvBuf, sizeof(uvBuf), "UV: %u", currentData.uv_index);
    display->setFont(FONT_MEDIUM);
    display->drawString(x + 175, y + 360, uvBuf);

    display->setFont(FONT_SMALL);
    const char *uvText = "Low";
    if (currentData.uv_index >= 3 && currentData.uv_index <= 5)
        uvText = "Moderate";
    else if (currentData.uv_index >= 6 && currentData.uv_index <= 7)
        uvText = "High";
    else if (currentData.uv_index >= 8)
        uvText = "Very High";
    display->drawString(x + 175, y + 402, uvText);
}

void WeatherXMModule::drawWindCard(OLEDDisplay *display, int16_t x, int16_t y)
{
    display->drawRect(x + 315, y + 290, 155, 155);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 325, y + 298, "WIND");

    float disp_speed = useImperial ? weatherxm::WeatherData::toMph(currentData.wind_speed) : currentData.wind_speed;
    char speedBuf[24];
    snprintf(speedBuf, sizeof(speedBuf), "%.1f %s", disp_speed, useImperial ? "mph" : "m/s");
    display->setFont(FONT_MEDIUM);
    display->drawString(x + 325, y + 328, speedBuf);

    display->setFont(FONT_SMALL);
    float disp_gust = useImperial ? weatherxm::WeatherData::toMph(currentData.wind_gust) : currentData.wind_gust;
    char gustBuf[24];
    snprintf(gustBuf, sizeof(gustBuf), "Gust: %.1f %s", disp_gust, useImperial ? "mph" : "m/s");
    display->drawString(x + 325, y + 368, gustBuf);

    char dirBuf[24];
    snprintf(dirBuf, sizeof(dirBuf), "Dir: %s (%u°)", weatherxm::WeatherData::degreesToCardinal(currentData.wind_direction),
             currentData.wind_direction);
    display->drawString(x + 325, y + 402, dirBuf);
}

#endif
