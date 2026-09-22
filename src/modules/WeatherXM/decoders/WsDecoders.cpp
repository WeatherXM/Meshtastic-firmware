#include "modules/WeatherXM/decoders/WsDecoders.h"
#include "UptimeClock.h"
#include <cstdio>
#include <cstring>

namespace weatherxm
{

uint8_t WsDecoders::calcCrc8(const uint8_t *buff, size_t len, uint8_t poly, uint8_t init)
{
    uint8_t remainder = init;
    for (size_t i = 0; i < len; ++i) {
        remainder ^= buff[i];
        for (uint8_t j = 0; j < 8; ++j) {
            if (remainder & 0x80)
                remainder = (remainder << 1) ^ poly;
            else
                remainder = (remainder << 1);
        }
    }
    return remainder;
}

uint8_t WsDecoders::calcChecksum(const uint8_t *buff, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += buff[i];
    }
    return sum;
}

bool WsDecoders::decodeWs1001(const uint8_t *pl, size_t len, WeatherData &data, int16_t rssi, float snr)
{
    if (!pl || len < 26)
        return false;

    uint8_t pkt_size = pl[0];
    if (pkt_size < 26 || pkt_size > len)
        return false;

    uint8_t calced_crc = calcCrc8(pl, pkt_size - 2, 0x31, 0x00);
    uint8_t calced_sum = calcChecksum(pl, pkt_size - 1);
    uint8_t pkt_crc = (pl[12] == 0x04) ? pl[pkt_size - 2] : pl[25];
    uint8_t pkt_sum = pl[pkt_size - 1];

    if (calced_crc != pkt_crc || calced_sum != pkt_sum)
        return false;

    data.station_id = ((uint32_t)pl[3] << 24) | ((uint32_t)pl[4] << 16) | ((uint32_t)pl[5] << 8) | pl[6];
    snprintf(data.station_name, sizeof(data.station_name), "WS-%06X", (unsigned int)(data.station_id & 0xFFFFFF));
    data.packet_count = (pl[7] << 8) | pl[8];
    data.battery_mv = (uint16_t)(pl[11] * 20);

    // Temperature: bits 15..4 in pl[14..15]
    uint16_t temp_raw = (((uint16_t)pl[14] << 8) | pl[15]) >> 4;
    if (temp_raw != 0x07FF) {
        float temp_f;
        if (temp_raw & 0x0800)
            temp_f = (float)((int16_t)(temp_raw | 0xF000)) / 10.0f;
        else
            temp_f = temp_raw / 10.0f;
        data.temperature = (temp_f - 32.0f) * (5.0f / 9.0f);
    }

    // Humidity: lowest 10 bits in pl[15..16]
    uint16_t hum_raw = (((uint16_t)pl[15] << 8) | pl[16]) & 0x03FF;
    if (hum_raw != 0x03FF) {
        data.humidity = hum_raw / 10.0f;
    }

    // Wind direction: pl[19] + top bit of pl[20]
    uint16_t wind_dir_raw = pl[19] | ((pl[20] & 0x80) << 1);
    if (wind_dir_raw != 0x01FF) {
        data.wind_direction = wind_dir_raw;
    }

    // Wind speed & gust: bit 3 & 2 in pl[15] + pl[17] & pl[18]
    uint8_t fw_ver = pl[1];
    float wind_divisor = (fw_ver < 0x40) ? 2.0f : 10.0f;

    uint16_t wind_speed_raw = ((pl[15] & 0x08) << 5) | pl[17];
    if (wind_speed_raw != 0x01FF) {
        data.wind_speed = wind_speed_raw / wind_divisor;
    }

    uint16_t wind_gust_raw = ((pl[15] & 0x04) << 6) | pl[18];
    if (wind_gust_raw != 0x01FF) {
        data.wind_gust = wind_gust_raw / wind_divisor;
    }

    // Solar radiation: 15-bit illuminance in pl[20..21]
    uint16_t illum_raw = ((pl[20] & 0x7F) << 8) | pl[21];
    if (illum_raw != 0x7FFF) {
        data.solar_radiation = (illum_raw * 10) * 0.007892f;
    }

    // UV index
    data.uv_index = (uint8_t)(pl[22] * 0.1f);

    // Accumulated rain
    if (len > 24) {
        data.precipitation_accum = (((uint16_t)pl[23] << 8) | pl[24]) * 0.254f;
    }

    data.dew_point = WeatherData::computeDewPoint(data.temperature, data.humidity);
    data.feels_like = WeatherData::computeFeelsLike(data.temperature, data.humidity, data.wind_speed);

    data.rssi = rssi;
    data.snr = snr;
    data.last_packet_time_ms = Time::getMillis();
    data.has_station_data = true;
    data.updateExtremes();

    return true;
}

bool WsDecoders::decodeWs1300(const uint8_t *pl, size_t len, WeatherData &data, int16_t rssi, float snr)
{
    if (!pl || len < 20)
        return false;

    // Byte offsets for WS1300 sensors
    int16_t temp_raw = (int16_t)(((uint16_t)pl[0] << 8) | pl[1]);
    if (temp_raw != 0x7FFF) {
        data.temperature = temp_raw / 10.0f;
    }

    if (pl[2] != 0xFF) {
        data.humidity = (float)pl[2];
    }

    uint16_t wind_dir_raw = ((uint16_t)pl[3] << 8) | pl[4];
    if (wind_dir_raw != 0xFFFF) {
        data.wind_direction = wind_dir_raw;
    }

    uint16_t wind_speed_raw = ((uint16_t)pl[5] << 8) | pl[6];
    if (wind_speed_raw != 0xFFFF) {
        data.wind_speed = wind_speed_raw / 10.0f;
    }

    uint16_t wind_gust_raw = ((uint16_t)pl[7] << 8) | pl[8];
    if (wind_gust_raw != 0xFFFF) {
        data.wind_gust = wind_gust_raw / 10.0f;
    }

    uint32_t illum = ((uint32_t)pl[9] << 16) | ((uint32_t)pl[10] << 8) | pl[11];
    if (illum != 0x00FFFFFF) {
        data.solar_radiation = illum * 0.007892f;
    }

    if (pl[12] != 0xFF) {
        data.uv_index = pl[12];
    }

    uint16_t press_raw = ((uint16_t)pl[13] << 8) | pl[14];
    if (press_raw != 0xFFFF) {
        data.barometric_pressure = press_raw / 10.0f;
    }

    if (len >= 17) {
        uint16_t rain_rate_raw = ((uint16_t)pl[15] << 8) | pl[16];
        if (rain_rate_raw != 0xFFFF)
            data.precipitation_rate = rain_rate_raw / 10.0f;
    }

    if (len >= 20) {
        uint32_t rain_accum_raw = ((uint32_t)pl[17] << 16) | ((uint32_t)pl[18] << 8) | pl[19];
        if (rain_accum_raw != 0x00FFFFFF)
            data.precipitation_accum = rain_accum_raw / 10.0f;
    }

    data.dew_point = WeatherData::computeDewPoint(data.temperature, data.humidity);
    data.feels_like = WeatherData::computeFeelsLike(data.temperature, data.humidity, data.wind_speed);

    data.rssi = rssi;
    data.snr = snr;
    data.last_packet_time_ms = Time::getMillis();
    data.has_station_data = true;
    data.updateExtremes();

    return true;
}

} // namespace weatherxm
