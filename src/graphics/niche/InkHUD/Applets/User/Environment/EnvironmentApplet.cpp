#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "./EnvironmentApplet.h"
#include "NodeDB.h"
#include "gps/RTC.h"
#include "pb_decode.h"
#include <stdio.h>
#include <time.h>

extern int32_t getTZOffset();

using namespace NicheGraphics;

InkHUD::EnvironmentApplet::EnvironmentApplet()
    : SinglePortModule("EnvironmentApplet", meshtastic_PortNum_TELEMETRY_APP)
{
}

void InkHUD::EnvironmentApplet::onActivate()
{
    loopbackOk = true;
    isPromiscuous = true;
}

void InkHUD::EnvironmentApplet::onDeactivate()
{
    loopbackOk = false;
    isPromiscuous = false;
}

ProcessMessage InkHUD::EnvironmentApplet::handleReceived(const meshtastic_MeshPacket &mp)
{
    meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(mp.decoded.payload.bytes, mp.decoded.payload.size);

    if (pb_decode(&stream, meshtastic_Telemetry_fields, &telemetry)) {
        if (telemetry.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
            envData = telemetry.variant.environment_metrics;
            hasData = true;
            senderNodeNum = mp.from ? mp.from : nodeDB->getNodeNum();
            lastRxTime = mp.rx_time ? mp.rx_time : getValidTime(RTCQuality::RTCQualityDevice, true);

            if (senderNodeNum == nodeDB->getNodeNum() || senderNodeNum == 0) {
                senderLongName = owner.long_name[0] ? owner.long_name : "";
                senderShortName = owner.short_name[0] ? owner.short_name : "";
            } else {
                meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(senderNodeNum);
                if (node) {
                    senderLongName = node->long_name[0] ? node->long_name : "";
                    senderShortName = node->short_name[0] ? node->short_name : "";
                } else {
                    senderLongName = "";
                    senderShortName = "";
                }
            }

            requestAutoshow();
            requestUpdate();
        }
    }

    return ProcessMessage::CONTINUE;
}

void InkHUD::EnvironmentApplet::onRender(bool full)
{
    drawHeader("Weather");

    if (!hasData) {
        setFont(fontMedium);
        if (width() < 180) {
            uint16_t midY = Y(0.48);
            uint16_t lh = fontMedium.lineHeight() + 2;
            printAt(X(0.5), midY - lh / 2, "No Weather", CENTER, MIDDLE);
            printAt(X(0.5), midY + lh / 2, "Telemetry Yet", CENTER, MIDDLE);
        } else {
            printAt(X(0.5), Y(0.5), "No Weather Telemetry", CENTER, MIDDLE);
        }
        return;
    }

    setFont(fontSmall);

    uint16_t startY = getHeaderHeight() + 2;
    uint16_t lineHeight = fontSmall.lineHeight() + 2;
    char buf[64];

    // Build Timestamp & Date in LOCAL time using getTZOffset()
    std::string timeStr = "";
    if (lastRxTime > 0) {
        uint32_t localTime = lastRxTime + getTZOffset();
        time_t rawTime = (time_t)localTime;
        struct tm *ti = gmtime(&rawTime);
        if (ti) {
            char timeBuf[32];
            if (config.display.use_12h_clock) {
                uint8_t h = ti->tm_hour % 12;
                if (h == 0) h = 12;
                snprintf(timeBuf, sizeof(timeBuf), "%u:%02u %s %04d-%02d-%02d",
                         h, ti->tm_min, ti->tm_hour >= 12 ? "PM" : "AM",
                         ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
            } else {
                snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %04d-%02d-%02d",
                         ti->tm_hour, ti->tm_min,
                         ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
            }
            timeStr = timeBuf;
        }
    }

    bool isWide = (width() >= 180);

    if (isWide) {
        // ---- LANDSCAPE / WIDE LAYOUT (2 Columns) ----

        // Line 1: Timestamp & Date, followed by blank row
        if (!timeStr.empty()) {
            printAt(0, startY, timeStr);
            startY += lineHeight + lineHeight; // 1st line + blank row
        }

        uint16_t col2X = width() / 2 + 2;
        uint16_t leftY = startY;
        uint16_t rightY = startY;

        // Left Col: Temp, Humidity, Pressure, Lux/UV
        if (envData.has_temperature) {
            bool useF = moduleConfig.telemetry.environment_display_fahrenheit;
            float t = useF ? (envData.temperature * 1.8f + 32.0f) : envData.temperature;
            snprintf(buf, sizeof(buf), "Temp: %.1f%s", t, useF ? "F" : "C");
            printAt(0, leftY, buf);
            leftY += lineHeight;
        }
        if (envData.has_relative_humidity) {
            snprintf(buf, sizeof(buf), "RH: %.0f%%", envData.relative_humidity);
            printAt(0, leftY, buf);
            leftY += lineHeight;
        }
        if (envData.has_barometric_pressure) {
            snprintf(buf, sizeof(buf), "P: %.1fhPa", envData.barometric_pressure);
            printAt(0, leftY, buf);
            leftY += lineHeight;
        }
        if (envData.has_lux || envData.has_uv_lux) {
            if (envData.has_lux) {
                snprintf(buf, sizeof(buf), "Lux: %.0f", envData.lux);
            } else {
                snprintf(buf, sizeof(buf), "UV: %.1f", envData.uv_lux);
            }
            printAt(0, leftY, buf);
            leftY += lineHeight;
        }

        // Right Col: Wind, Rain, Location
        if (envData.has_wind_speed || envData.has_wind_gust) {
            std::string wStr = "Wind: ";
            if (envData.has_wind_speed) {
                snprintf(buf, sizeof(buf), "%.1fm/s", envData.wind_speed);
                wStr += buf;
            }
            if (envData.has_wind_gust) {
                snprintf(buf, sizeof(buf), " g:%.1f", envData.wind_gust);
                wStr += buf;
            }
            printAt(col2X, rightY, wStr);
            rightY += lineHeight;
        }
        if (envData.has_wind_direction) {
            snprintf(buf, sizeof(buf), "Dir: %udeg", (unsigned)envData.wind_direction);
            printAt(col2X, rightY, buf);
            rightY += lineHeight;
        }
        if (envData.has_rainfall_1h || envData.has_rainfall_24h) {
            std::string rStr = "Rain: ";
            if (envData.has_rainfall_1h) {
                snprintf(buf, sizeof(buf), "1h:%.1fm", envData.rainfall_1h);
                rStr += buf;
            }
            if (envData.has_rainfall_24h) {
                snprintf(buf, sizeof(buf), " 24h:%.1fm", envData.rainfall_24h);
                rStr += buf;
            }
            printAt(col2X, rightY, rStr);
            rightY += lineHeight;
        }

        meshtastic_PositionLite pos = meshtastic_PositionLite_init_zero;
        if (senderNodeNum != 0 && nodeDB->copyNodePosition(senderNodeNum, pos) && (pos.latitude_i != 0 || pos.longitude_i != 0)) {
            float lat = pos.latitude_i * 1e-7f;
            float lon = pos.longitude_i * 1e-7f;
            snprintf(buf, sizeof(buf), "%.4f,%.4f", lat, lon);
            printAt(col2X, rightY, buf);
        }

    } else {
        // ---- PORTRAIT / NARROW LAYOUT (Single Column) ----
        uint16_t currentY = startY;

        // Line 1: Timestamp & Date (YYYY-MM-DD), followed by blank row
        if (!timeStr.empty()) {
            printAt(0, currentY, timeStr);
            currentY += lineHeight + lineHeight; // 1st line + blank row
        }

        if (envData.has_temperature || envData.has_relative_humidity) {
            std::string tempStr = "";
            if (envData.has_temperature) {
                bool useF = moduleConfig.telemetry.environment_display_fahrenheit;
                float t = useF ? (envData.temperature * 1.8f + 32.0f) : envData.temperature;
                snprintf(buf, sizeof(buf), "Temp: %.1f%s", t, useF ? "F" : "C");
                tempStr += buf;
            }
            if (envData.has_relative_humidity) {
                snprintf(buf, sizeof(buf), " RH: %.0f%%", envData.relative_humidity);
                tempStr += buf;
            }
            printAt(0, currentY, tempStr);
            currentY += lineHeight;
        }

        // Pressure
        if (envData.has_barometric_pressure) {
            snprintf(buf, sizeof(buf), "P: %.1fhPa", envData.barometric_pressure);
            printAt(0, currentY, buf);
            currentY += lineHeight;
        }

        // Lux on its own line below pressure
        if (envData.has_lux || envData.has_uv_lux) {
            if (envData.has_lux) {
                snprintf(buf, sizeof(buf), "Lux: %.0f", envData.lux);
            } else {
                snprintf(buf, sizeof(buf), "UV: %.1f", envData.uv_lux);
            }
            printAt(0, currentY, buf);
            currentY += lineHeight;

            // Blank line after lux
            currentY += lineHeight;
        }

        // Wind & Gust under wind, then blank row
        if (envData.has_wind_speed || envData.has_wind_gust || envData.has_wind_direction) {
            std::string windStr = "Wind: ";
            if (envData.has_wind_speed) {
                snprintf(buf, sizeof(buf), "%.1fm/s", envData.wind_speed);
                windStr += buf;
            }
            if (envData.has_wind_direction) {
                snprintf(buf, sizeof(buf), " %udeg", (unsigned)envData.wind_direction);
                windStr += buf;
            }
            printAt(0, currentY, windStr);
            currentY += lineHeight;

            if (envData.has_wind_gust) {
                snprintf(buf, sizeof(buf), "Gust: %.1fm/s", envData.wind_gust);
                printAt(0, currentY, buf);
                currentY += lineHeight;
            }

            // Blank row after gust
            currentY += lineHeight;
        }

        // Rain 1h & 24h rain under 1h
        if (envData.has_rainfall_1h || envData.has_rainfall_24h) {
            if (envData.has_rainfall_1h) {
                snprintf(buf, sizeof(buf), "Rain 1h: %.1fmm", envData.rainfall_1h);
                printAt(0, currentY, buf);
                currentY += lineHeight;
            }
            if (envData.has_rainfall_24h) {
                snprintf(buf, sizeof(buf), "Rain 24h: %.1fmm", envData.rainfall_24h);
                printAt(0, currentY, buf);
                currentY += lineHeight;
            }
        }

        meshtastic_PositionLite pos = meshtastic_PositionLite_init_zero;
        if (senderNodeNum != 0 && nodeDB->copyNodePosition(senderNodeNum, pos) && (pos.latitude_i != 0 || pos.longitude_i != 0)) {
            float lat = pos.latitude_i * 1e-7f;
            float lon = pos.longitude_i * 1e-7f;
            snprintf(buf, sizeof(buf), "Loc: %.4f, %.4f", lat, lon);
            printAt(0, currentY, buf);
            currentY += lineHeight;
        }
    }

    // Resolve Long Name and Short Name / Identifier
    std::string longNameStr = senderLongName;
    std::string shortNameStr = senderShortName;

    if (senderNodeNum == nodeDB->getNodeNum() || senderNodeNum == 0) {
        if (longNameStr.empty() && owner.long_name[0]) {
            longNameStr = owner.long_name;
        }
        if (shortNameStr.empty() && owner.short_name[0]) {
            shortNameStr = owner.short_name;
        }
    }

    std::string hexId = hexifyNodeNum(senderNodeNum ? senderNodeNum : nodeDB->getNodeNum());
    if (shortNameStr.empty()) {
        shortNameStr = hexId;
    }
    if (longNameStr.empty()) {
        longNameStr = shortNameStr;
    }

    if (isWide) {
        // Landscape Mode: Single line footer
        printAt(0, Y(1.0) - fontSmall.lineHeight(), longNameStr, LEFT, BOTTOM);
    } else {
        // Portrait Mode: 2 Footer lines
        // Line above footer: Full node name
        printAt(0, Y(1.0) - (fontSmall.lineHeight() * 2 + 1), longNameStr, LEFT, BOTTOM);

        // Footer line: Short name or Node ID
        printAt(0, Y(1.0) - fontSmall.lineHeight(), shortNameStr, LEFT, BOTTOM);
    }
}

#endif
