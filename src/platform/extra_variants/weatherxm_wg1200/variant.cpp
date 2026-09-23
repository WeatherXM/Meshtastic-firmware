#include "configuration.h"

#ifdef WG1200

#include "Arduino.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "esp_ota_ops.h"

// Note: earlyInitVariant() runs before consoleInit(), so do NOT use LOG_* macros here!

static void wg1200_safe_restart_handler()
{
    // CRITICAL DEFENSE FOR HISTORICAL BOOTLOADERS:
    // Bootloaders built before mid-2024 (e.g. May 27 2024) configured GPIO 10 as the factory reset pin.
    // GPIO 10 is ST7701 LCD G0. If left low during reboot, the bootloader wipes otadata and NVS after 5s.
    // Driving GPIO 10 HIGH and locking pad hold prevents accidental factory resets on warm reboot.
    gpio_reset_pin(GPIO_NUM_10);
    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(GPIO_NUM_10, 1);
    gpio_hold_en(GPIO_NUM_10);
}

void earlyInitVariant()
{
    // Protect against historical bootloader GPIO 10 factory reset bug
    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(GPIO_NUM_10, 1);
    gpio_hold_dis(GPIO_NUM_10);
    esp_register_shutdown_handler(wg1200_safe_restart_handler);

    // On WeatherXM WG1200, user button is on physical GPIO 38, active LOW (normally open, pulled high).
    // Use native ESP-IDF GPIO API to bypass Arduino's uninitialized IO expander hook.
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << 38);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // If button is physically held down at boot (active LOW)
    if (gpio_get_level(GPIO_NUM_38) == 0) {
        esp_rom_printf("\n[WG1200] Hardware button held at boot - checking 10s rollback threshold...\n");

        uint32_t start = millis();
        bool held = true;

        // Sample for 10 full seconds (10,000 ms)
        while (millis() - start < 10000) {
            if (gpio_get_level(GPIO_NUM_38) != 0) {
                held = false;
                break;
            }
            delay(50);
        }

        if (held) {
            esp_rom_printf("[WG1200] Button held for 10s! Rolling back to factory WeatherXM firmware...\n");

            // Erasing the otadata partition forces the ESP-IDF bootloader to fall back
            // to the immutable WeatherXM factory slot (0x20000).
            const esp_partition_t *otadata = esp_partition_find_first(
                ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
            if (otadata != NULL) {
                esp_partition_erase_range(otadata, 0, otadata->size);
                esp_rom_printf("[WG1200] otadata erased. Rebooting to factory slot now.\n");
            } else {
                esp_rom_printf("[WG1200] ERROR: otadata partition not found!\n");
            }

            // Reboot into WeatherXM factory
            esp_restart();
        } else {
            esp_rom_printf("[WG1200] Button released before 10s. Continuing normal Meshtastic boot.\n");
        }
    }

    // Cancel automatic bootloader rollback and confirm this slot as valid and permanent
    esp_ota_mark_app_valid_cancel_rollback();
}

#endif
