#include "WeatherXMModule.h"

#if (defined(WG1200) || defined(HAS_WEATHERXM)) && !MESHTASTIC_EXCLUDE_WEATHERXM

#include "DebugConfiguration.h"
#include "UptimeClock.h"
#include "decoders/WsDecoders.h"
#include "graphics/ScreenFonts.h"
#include "mesh/Throttle.h"
#include <cstdio>

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_BMP3XX.h>)
#include "modules/Telemetry/Sensor/BMP3XXSensor.h"
#endif

WeatherXMModule *weatherXMModule = nullptr;

WeatherXMModule::WeatherXMModule()
    : SinglePortModule("Weather", meshtastic_PortNum_PRIVATE_APP), concurrency::OSThread("WeatherXM")
{
}

void WeatherXMModule::setup()
{
    LOG_INFO("WeatherXMModule initialized");
    updateOnboardSensors();
    setInterval(2000);
}

int32_t WeatherXMModule::runOnce()
{
    uint32_t now = Time::getMillis();
    if (Throttle::hasElapsed(lastSensorPollMs, 3000)) {
        lastSensorPollMs = now;
        updateOnboardSensors();
    }
    return 2000;
}

void WeatherXMModule::updateOnboardSensors()
{
#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_BMP3XX.h>)
    BMP3XXSingleton *bmp = BMP3XXSingleton::GetInstance();
    if (bmp && bmp->performReading()) {
        float press_hpa = bmp->pressure / 100.0f;
        float temp_c = bmp->temperature;

        bool changed = (fabsf(currentData.barometric_pressure - press_hpa) > 0.05f);
        currentData.barometric_pressure = press_hpa;
        currentData.has_bmp390 = true;

        if (!currentData.has_station_data) {
            currentData.temperature = temp_c;
            currentData.feels_like = temp_c;
            currentData.humidity = 0.0f;
            currentData.dew_point = temp_c;
        }

        currentData.updateExtremes();

        if (changed) {
            UIFrameEvent e;
            e.action = UIFrameEvent::REDRAW_ONLY;
            notifyObservers(&e);
        }
    }
#endif
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

    bool decoded = false;
    if (length >= 26 || (length >= 16 && payload[0] <= length)) {
        decoded = weatherxm::WsDecoders::decodeWs1001(payload, length, currentData, rssi, snr);
    }
    if (!decoded && length >= 15) {
        decoded = weatherxm::WsDecoders::decodeWs1300(payload, length, currentData, rssi, snr);
    }

    if (decoded) {
        currentData.rssi = rssi;
        currentData.snr = snr;
        currentData.last_packet_time_ms = Time::getMillis();
        currentData.packet_count++;
        currentData.has_station_data = true;
        currentData.updateExtremes();

        LOG_INFO("WeatherXM packet #%lu decoded: T=%.1f C, H=%.0f %%, Wind=%.1f m/s", (unsigned long)currentData.packet_count,
                 currentData.temperature, currentData.humidity, currentData.wind_speed);

        UIFrameEvent e;
        e.action = UIFrameEvent::REDRAW_ONLY;
        notifyObservers(&e);
    } else {
        LOG_WARN("WeatherXM packet CRC/checksum failed (len=%zu)", length);
    }
}

void WeatherXMModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
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
        display->drawString(x + 185, y + 14, currentData.station_name);
    } else {
        display->drawString(x + 175, y + 14, "MESHTASTIC NODE");
    }

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    if (currentData.has_station_data) {
        char pktBuf[24];
        snprintf(pktBuf, sizeof(pktBuf), "PKT #%lu", (unsigned long)currentData.packet_count);
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
