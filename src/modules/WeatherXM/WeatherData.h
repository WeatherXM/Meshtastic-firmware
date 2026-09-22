#pragma once

#include <Arduino.h>
#include <cmath>
#include <stdint.h>
#include <string>

namespace weatherxm
{

enum class TemperatureUnit : uint8_t { Celsius, Fahrenheit };

enum class SpeedUnit : uint8_t { MetersPerSecond, KilometersPerHour, MilesPerHour };

enum class PressureUnit : uint8_t {
    HectoPascal,    // hPa / mbar
    InchesOfMercury // inHg
};

enum class RainUnit : uint8_t { Millimeters, Inches };

struct WeatherData {
    // Ambient conditions
    float temperature = 0.0f;         // °C
    float humidity = 0.0f;            // %RH (0 - 100)
    float dew_point = 0.0f;           // °C
    float feels_like = 0.0f;          // °C
    float barometric_pressure = 0.0f; // hPa

    // Wind
    float wind_speed = 0.0f;     // m/s
    float wind_gust = 0.0f;      // m/s
    uint16_t wind_direction = 0; // degrees (0 - 359)

    // Solar / UV
    float solar_radiation = 0.0f; // W/m²
    uint8_t uv_index = 0;         // 0 - 15

    // Rain
    float precipitation_rate = 0.0f;  // mm/h
    float precipitation_accum = 0.0f; // mm

    // Station metadata
    uint32_t station_id = 0;
    char station_name[24] = {0};
    uint16_t battery_mv = 0;
    int16_t rssi = 0;
    float snr = 0.0f;
    uint32_t last_packet_time_ms = 0;
    uint32_t packet_count = 0;
    bool has_station_data = false;
    bool has_bmp390 = false;

    // Daily records / extremes
    float temp_min = 999.0f;
    float temp_max = -999.0f;
    float press_min = 9999.0f;
    float press_max = -9999.0f;
    float hum_min = 999.0f;
    float hum_max = -999.0f;
    float wind_gust_max = 0.0f;

    void updateExtremes()
    {
        if (temperature > -80.0f && temperature < 80.0f) {
            if (temperature < temp_min)
                temp_min = temperature;
            if (temperature > temp_max)
                temp_max = temperature;
        }
        if (barometric_pressure > 300.0f && barometric_pressure < 1200.0f) {
            if (barometric_pressure < press_min)
                press_min = barometric_pressure;
            if (barometric_pressure > press_max)
                press_max = barometric_pressure;
        }
        if (humidity >= 0.0f && humidity <= 100.0f) {
            if (humidity < hum_min)
                hum_min = humidity;
            if (humidity > hum_max)
                hum_max = humidity;
        }
        if (wind_gust > wind_gust_max)
            wind_gust_max = wind_gust;
    }

    // Computes Magnus-Tetens dew point (°C)
    static float computeDewPoint(float temp_c, float hum_pct)
    {
        if (hum_pct <= 0.0f)
            return temp_c;
        const float a = 17.27f;
        const float b = 237.7f;
        float alpha = ((a * temp_c) / (b + temp_c)) + logf(hum_pct / 100.0f);
        return (b * alpha) / (a - alpha);
    }

    // Computes Feels Like (°C) combining Heat Index (warm) and Wind Chill (cold)
    static float computeFeelsLike(float temp_c, float hum_pct, float wind_ms)
    {
        float temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;
        float wind_mph = wind_ms * 2.23694f;

        if (temp_f <= 50.0f && wind_mph > 3.0f) {
            float wc_f =
                35.74f + (0.6215f * temp_f) - (35.75f * powf(wind_mph, 0.16f)) + (0.4275f * temp_f * powf(wind_mph, 0.16f));
            return (wc_f - 32.0f) * 5.0f / 9.0f;
        }

        if (temp_f >= 80.0f && hum_pct >= 40.0f) {
            float hi_f = -42.379f + (2.04901523f * temp_f) + (10.14333127f * hum_pct) - (0.22475541f * temp_f * hum_pct) -
                         (0.00683783f * temp_f * temp_f) - (0.05481717f * hum_pct * hum_pct) +
                         (0.00122874f * temp_f * temp_f * hum_pct) + (0.00085282f * temp_f * hum_pct * hum_pct) -
                         (0.00000199f * temp_f * temp_f * hum_pct * hum_pct);
            return (hi_f - 32.0f) * 5.0f / 9.0f;
        }

        return temp_c;
    }

    // Direction string from azimuth degrees (0 - 359)
    static const char *degreesToCardinal(uint16_t deg)
    {
        static const char *cardinals[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                          "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        int idx = (int)((deg + 11.25f) / 22.5f) % 16;
        return cardinals[idx];
    }

    // Unit conversion helpers
    static float toFahrenheit(float celsius) { return (celsius * 9.0f / 5.0f) + 32.0f; }
    static float toKmh(float ms) { return ms * 3.6f; }
    static float toMph(float ms) { return ms * 2.23694f; }
    static float toInHg(float hpa) { return hpa * 0.02953f; }
    static float toInches(float mm) { return mm * 0.0393701f; }

    std::string toJson(bool imperial = false) const
    {
        char buf[1024];
        float disp_temp = imperial ? toFahrenheit(temperature) : temperature;
        float disp_fl = imperial ? toFahrenheit(feels_like) : feels_like;
        float disp_press = imperial ? toInHg(barometric_pressure) : barometric_pressure;
        float disp_wind = imperial ? toMph(wind_speed) : wind_speed;
        float disp_gust = imperial ? toMph(wind_gust) : wind_gust;
        float disp_rain = imperial ? toInches(precipitation_accum) : precipitation_accum;
        float disp_rate = imperial ? toInches(precipitation_rate) : precipitation_rate;

        const char *u_temp = imperial ? "F" : "C";
        const char *u_wind = imperial ? "mph" : "m/s";
        const char *u_press = imperial ? "inHg" : "hPa";
        const char *u_rain = imperial ? "in" : "mm";

        snprintf(buf, sizeof(buf),
                 "{"
                 "\"timestamp\":%lu,"
                 "\"has_weather\":%s,"
                 "\"has_bmp390\":%s,"
                 "\"station_id\":%lu,"
                 "\"station_name\":\"%s\","
                 "\"temp\":\"%.1f\","
                 "\"feels_like\":\"%.1f\","
                 "\"dew_point\":\"%.1f\","
                 "\"humidity\":\"%.0f\","
                 "\"pressure\":\"%.1f\","
                 "\"wind_speed\":\"%.1f\","
                 "\"wind_gust\":\"%.1f\","
                 "\"wind_dir_deg\":%u,"
                 "\"wind_dir_cardinal\":\"%s\","
                 "\"precip_rate\":\"%.1f\","
                 "\"precip_daily\":\"%.1f\","
                 "\"precip_cml\":\"%.1f\","
                 "\"uv\":%u,"
                 "\"irradiance\":\"%.1f\","
                 "\"battery_mv\":%u,"
                 "\"rssi\":%d,"
                 "\"snr\":%.1f,"
                 "\"packet_count\":%lu,"
                 "\"unit_temp\":\"%s\","
                 "\"unit_wind\":\"%s\","
                 "\"unit_precip\":\"%s\","
                 "\"unit_press\":\"%s\","
                 "\"units\":{\"temp\":\"%s\",\"wind\":\"%s\",\"precip\":\"%s\",\"press\":\"%s\"}"
                 "}",
                 (unsigned long)millis() / 1000, (has_station_data || has_bmp390) ? "true" : "false",
                 has_bmp390 ? "true" : "false", (unsigned long)station_id, station_name, disp_temp, disp_fl, dew_point, humidity,
                 disp_press, disp_wind, disp_gust, wind_direction, degreesToCardinal(wind_direction), disp_rate, disp_rain,
                 disp_rain, uv_index, solar_radiation, battery_mv, rssi, snr, (unsigned long)packet_count, u_temp, u_wind, u_rain,
                 u_press, u_temp, u_wind, u_rain, u_press);
        return std::string(buf);
    }
};

} // namespace weatherxm
