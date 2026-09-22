#pragma once

#ifdef __cplusplus
#if __has_include(<LovyanGFX.hpp>) && (!defined(CUSTOM_TOUCH_DRIVER) || __has_include(<bb_captouch.h>))

#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

#ifndef IO_EXPANDER
#define IO_EXPANDER 0x40
#endif

#ifdef CUSTOM_TOUCH_DRIVER
#include <bb_captouch.h>

#define TOUCH_SDA 39
#define TOUCH_SCL 40
#define TOUCH_INT -1
#define TOUCH_RST -1

// Custom touch driver redirecting getTouch() to BBCapTouch for FT6336U on 0x48
class LGFX_Touch : public lgfx::LGFX_Device
{
  public:
    bool init_impl(bool use_reset, bool use_clear) override
    {
        bool result = LGFX_Device::init_impl(use_reset, use_clear);
        bbct.init(TOUCH_SDA, TOUCH_SCL);
        bbct.setOrientation(180, 480, 480);
        return result;
    }

    LGFX_Touch *touch(void) { return this; }

    int8_t getTouchInt(void) { return TOUCH_INT; }

    bool getTouchXY(uint16_t *touchX, uint16_t *touchY)
    {
        TOUCHINFO ti;

        if (bbct.getSamples(&ti)) {
            if ((ti.x[0] || ti.y[0]) && ti.x[0] < 480 && ti.y[0] < 480) {
                *touchX = ti.x[0];
                *touchY = ti.y[0];
                return true;
            }
        }
        return false;
    };

    void wakeup(void) {}
    void sleep(void) {}

  private:
    BBCapTouch bbct;
};
#endif

class Panel_WG1200 : public lgfx::Panel_ST7701
{
  public:
    bool init(bool use_reset) override
    {
        pinMode(5 | IO_EXPANDER, OUTPUT);
        digitalWrite(5 | IO_EXPANDER, LOW);
        delay(10);
        digitalWrite(5 | IO_EXPANDER, HIGH);
        delay(20);
        pinMode(7 | IO_EXPANDER, OUTPUT);
        digitalWrite(7 | IO_EXPANDER, LOW);
        delay(10);
        digitalWrite(7 | IO_EXPANDER, HIGH);
        delay(20);
        pinMode(41, OUTPUT);
        pinMode(48, OUTPUT);
        pinMode(4 | IO_EXPANDER, OUTPUT);
        digitalWrite(4 | IO_EXPANDER, LOW);
        bool res = lgfx::Panel_ST7701::init(use_reset);
        digitalWrite(4 | IO_EXPANDER, HIGH);
        return res;
    }

    const uint8_t *getInitCommands(uint8_t listno) const override
    {
        // Exact 39 commands from WG1200 / SenseCAP Indicator BSP lcd_panel_config.c
        static constexpr const uint8_t list0[] = {
            0x11, CMD_INIT_DELAY,
            120, // Sleep Out
            0xFF, 5,
            0x77, 0x01,
            0x00, 0x00,
            0x10, 0xC0,
            2,    0x3B,
            0x00, // 480 lines (LNSET)
            0xC1, 2,
            0x0D, 0x02,
            0xC2, 2,
            0x31, 0x05,
            0xC7, 1,
            0x04, 0xCD,
            1,    0x08,
            0xB0, 16,
            0x00, 0x11,
            0x18, 0x0E,
            0x11, 0x06,
            0x07, 0x08,
            0x07, 0x22,
            0x04, 0x12,
            0x0F, 0xAA,
            0x31, 0x18,
            0xB1, 16,
            0x00, 0x11,
            0x19, 0x0E,
            0x12, 0x07,
            0x08, 0x08,
            0x08, 0x22,
            0x04, 0x11,
            0x11, 0xA9,
            0x32, 0x18,
            0xFF, 5,
            0x77, 0x01,
            0x00, 0x00,
            0x11, 0xB0,
            1,    0x60,
            0xB1, 1,
            0x32, 0xB2,
            1,    0x07,
            0xB3, 1,
            0x80, 0xB5,
            1,    0x49,
            0xB7, 1,
            0x85, 0xB8,
            1,    0x21,
            0xC1, 1,
            0x78, 0xC2,
            1,    0x78,
            0xE0, 3,
            0x00, 0x1B,
            0x02, 0xE1,
            11,   0x08,
            0xA0, 0x00,
            0x00, 0x07,
            0xA0, 0x00,
            0x00, 0x00,
            0x44, 0x44,
            0xE2, 12,
            0x11, 0x11,
            0x44, 0x44,
            0xED, 0xA0,
            0x00, 0x00,
            0xEC, 0xA0,
            0x00, 0x00,
            0xE3, 4,
            0x00, 0x00,
            0x11, 0x11,
            0xE4, 2,
            0x44, 0x44,
            0xE5, 16,
            0x0A, 0xE9,
            0xD8, 0xA0,
            0x0C, 0xEB,
            0xD8, 0xA0,
            0x0E, 0xED,
            0xD8, 0xA0,
            0x10, 0xEF,
            0xD8, 0xA0,
            0xE6, 4,
            0x00, 0x00,
            0x11, 0x11,
            0xE7, 2,
            0x44, 0x44,
            0xE8, 16,
            0x09, 0xE8,
            0xD8, 0xA0,
            0x0B, 0xEA,
            0xD8, 0xA0,
            0x0D, 0xEC,
            0xD8, 0xA0,
            0x0F, 0xEE,
            0xD8, 0xA0,
            0xEB, 7,
            0x02, 0x00,
            0xE4, 0xE4,
            0x88, 0x00,
            0x40, 0xEC,
            2,    0x3C,
            0x00, 0xED,
            16,   0xAB,
            0x89, 0x76,
            0x54, 0x02,
            0xFF, 0xFF,
            0xFF, 0xFF,
            0xFF, 0xFF,
            0x20, 0x45,
            0x67, 0x98,
            0xBA, 0x36,
            1,    0x10,
            0xFF, 5,
            0x77, 0x01,
            0x00, 0x00,
            0x13, 0xE5,
            1,    0xE4,
            0xFF, 5,
            0x77, 0x01,
            0x00, 0x00,
            0x00, 0x3A,
            1,    0x60,
            0x21, 0, // Display Inversion On
            0x11, CMD_INIT_DELAY,
            120,     // Sleep Out
            0x29, 0, // Display On
            0xFF, 0xFF,
        };
        switch (listno) {
        case 0:
            return list0;
        default:
            return nullptr;
        }
    }
};

#ifdef CUSTOM_TOUCH_DRIVER
class LGFX_WG1200 : public LGFX_Touch
#else
class LGFX_WG1200 : public lgfx::LGFX_Device
#endif
{
    Panel_WG1200 _panel_instance;
    lgfx::Bus_RGB _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

  public:
    const uint16_t screenWidth = 480;
    const uint16_t screenHeight = 480;

    bool hasButton(void) { return true; }

    LGFX_WG1200(void)
    {
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = 480;
            cfg.memory_height = 480;
            cfg.panel_width = screenWidth;
            cfg.panel_height = screenHeight;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _panel_instance.config_detail();
            cfg.pin_cs = -1;
            cfg.pin_sclk = 41;
            cfg.pin_mosi = 48;
            cfg.use_psram = 1;
            _panel_instance.config_detail(cfg);
        }

        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0 = GPIO_NUM_15; // B0
            cfg.pin_d1 = GPIO_NUM_14; // B1
            cfg.pin_d2 = GPIO_NUM_13; // B2
            cfg.pin_d3 = GPIO_NUM_12; // B3
            cfg.pin_d4 = GPIO_NUM_11; // B4

            cfg.pin_d5 = GPIO_NUM_10; // G0
            cfg.pin_d6 = GPIO_NUM_9;  // G1
            cfg.pin_d7 = GPIO_NUM_8;  // G2
            cfg.pin_d8 = GPIO_NUM_7;  // G3
            cfg.pin_d9 = GPIO_NUM_6;  // G4
            cfg.pin_d10 = GPIO_NUM_5; // G5

            cfg.pin_d11 = GPIO_NUM_4; // R0
            cfg.pin_d12 = GPIO_NUM_3; // R1
            cfg.pin_d13 = GPIO_NUM_2; // R2
            cfg.pin_d14 = GPIO_NUM_1; // R3
            cfg.pin_d15 = GPIO_NUM_0; // R4

            cfg.pin_henable = GPIO_NUM_18;
            cfg.pin_vsync = GPIO_NUM_17;
            cfg.pin_hsync = GPIO_NUM_16;
            cfg.pin_pclk = GPIO_NUM_21;
            cfg.freq_write = 6000000;

            // WeatherXM WG1200 ST7701 panel timings
            cfg.hsync_polarity = 0;
            cfg.hsync_front_porch = 10;
            cfg.hsync_pulse_width = 8;
            cfg.hsync_back_porch = 50;

            cfg.vsync_polarity = 0;
            cfg.vsync_front_porch = 10;
            cfg.vsync_pulse_width = 8;
            cfg.vsync_back_porch = 20;

            cfg.pclk_active_neg = 0;
            cfg.de_idle_high = 1;
            cfg.pclk_idle_high = 0;

            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;
            _light_instance.config(cfg);
        }
        _panel_instance.light(&_light_instance);

#ifndef CUSTOM_TOUCH_DRIVER
        {
            auto cfg = _touch_instance.config();
            cfg.pin_cs = GPIO_NUM_NC;
            cfg.x_min = 0;
            cfg.x_max = 479;
            cfg.y_min = 0;
            cfg.y_max = 479;
            cfg.pin_int = GPIO_NUM_NC;
            cfg.bus_shared = true;
            cfg.offset_rotation = 0;

            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x48;
            cfg.pin_sda = -1;
            cfg.pin_scl = -1;
            cfg.freq = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
#endif
        setPanel(&_panel_instance);
    }
};

#endif // __has_include(<LovyanGFX.hpp>)
#endif // __cplusplus
