#pragma once

#include "modules/WeatherXM/WeatherData.h"
#include <stddef.h>
#include <stdint.h>

namespace weatherxm
{

class WsDecoders
{
  public:
    // Decodes a raw WS1001 frame and populates weatherData.
    static bool decodeWs1001(const uint8_t *payload, size_t len, WeatherData &data, int16_t rssi = 0, float snr = 0.0f);

    // Decodes a raw WS1300 frame and populates weatherData.
    static bool decodeWs1300(const uint8_t *payload, size_t len, WeatherData &data, int16_t rssi = 0, float snr = 0.0f);

    // CRC8 calculation with polynomial 0x31 and init 0x00
    static uint8_t calcCrc8(const uint8_t *buff, size_t len, uint8_t poly = 0x31, uint8_t init = 0x00);

    // Checksum: 8-bit sum modulo 256
    static uint8_t calcChecksum(const uint8_t *buff, size_t len);
};

} // namespace weatherxm
