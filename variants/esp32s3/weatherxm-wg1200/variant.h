#define I2C_SDA 39
#define I2C_SCL 40

#define BUTTON_PIN 38
#define BUTTON_ACTIVE_LOW true
#define BUTTON_ACTIVE_PULLUP true

// WG1200 does NOT have an RP2040 coprocessor.
// GPIO19/20 are ESP32-S3 native USB CDC, do not map them to serial sensors.

// LoRa SX1262 via PCA9535 IO Expander (base 0x40)
#define USE_SX1262
#define USE_SX1268

#define LORA_SCK 41
#define LORA_MISO 47
#define LORA_MOSI 48
#define LORA_CS (0 | IO_EXPANDER)

#define LORA_DIO0 -1
#define LORA_RESET (1 | IO_EXPANDER)
#define LORA_DIO1 (3 | IO_EXPANDER) // SX1262 IRQ
#define LORA_DIO2 (2 | IO_EXPANDER) // SX1262 BUSY
#define LORA_DIO1_EXTENDED_IO

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH

#define TCXO_OPTIONAL
#define SX126X_DIO3_TCXO_VOLTAGE 2.4

// Restart SPI bus after LovyanGFX/TFT init
#define RESTART_LORA_SPI_AFTER_TFT_INIT 1

// Onboard BMP390
#define SENSOR_BMP_ADDR 0x77
