# WeatherXM WG1200 (D1 Gateway) Meshtastic Variant & Dual-Firmware Architecture

## 1. Executive Summary

The WeatherXM WG1200 (D1 Gateway) features an ESP32-S3 microcontroller with hardware Secure Boot V2 and a 16MB SPI flash. To allow community exploration without risking bricked devices or losing factory WeatherXM identity, this variant implements a **non-destructive dual-firmware system**:

$$\text{WeatherXM (Factory)} \longleftrightarrow \text{Meshtastic (OTA)}$$

A production WG1200 can transition back and forth between WeatherXM and Meshtastic indefinitely, retaining hardware device certificates, cryptographic identity, and factory sensor calibrations.

### Hardware Identity & Provenance

The WG1200 is treated as a **distinct WeatherXM hardware product**, not as a Seeed Studio SenseCAP Indicator variant.

- **Manufacturer:** WeatherXM AG
- **Product model:** WG1200 (D1 Gateway)
- **Meshtastic hardware identity:** `PRIVATE_HW` (255) until the Meshtastic protobuf assigns WG1200 a dedicated `HardwareModel` value.
- **Build/device slug:** `WEATHERXM_WG1200`.
- **Source provenance:** the first Meshtastic port used the Seeed Studio SenseCAP Indicator/D1L implementation as a starting reference because the products share the ESP32-S3, SX1262 and ST7701-class display architecture. The WG1200 PCB, pin routing, radio/peripheral integration, flash layout and production identity are WeatherXM-specific.

The Seeed origin is therefore retained as **source attribution**, while firmware metadata and on-air hardware identity describe the product that is actually being programmed. Once Meshtastic assigns a permanent WG1200 hardware-model ID, replace `PRIVATE_HW`/255 with that assigned value without changing the WeatherXM board namespace.

---

## 2. Firmware Features & Capabilities

This firmware variant turns the WeatherXM WG1200 into a high-capability standalone Meshtastic node and weather hub:

- **Complete WG1200 Hardware Integration**:
  - **ESP32-S3 Dual-Core MCU**: 240 MHz, 8MB embedded Octal PSRAM, 16MB SPI Flash.
  - **Semtech SX1262 LoRa Transceiver**: Connected via SPI2 with control pins (CS, RESET, DIO1 IRQ, BUSY, Antenna Switch) routed through an onboard PCA9535 I2C IO expander (`0x40`).
  - **Onboard Bosch Sensortec BMP390**: High-precision barometric pressure and temperature sensor on I2C bus (`0x77`), powered through an expander-gated power switch (`IO1_2`).
  - **Hardware User Button**: Physical push button on GPIO 38 with dual functions: short press navigation / screen wake, and long 10-second hold at boot for instant factory rollback.
- **Circular ST7701 480x480 RGB TFT Display & Touch MUI**:
  - High-resolution circular color panel driven via custom LovyanGFX initialization (`LGFX_WG1200.h`) and `bb_captouch` capacitive touch driver (`0x48`).
  - Fully integrated Meshtastic Device UI (MUI) featuring customized WeatherXM TFT telemetry views and status screens.
- **Concurrent TFT Networking & Independent Display Queues**:
  - Standard Meshtastic COLOR builds disable HTTP/HTTPS and TCP servers because the MUI acts as a single-consumer client of the internal packet pipeline.
  - This firmware introduces bounded, thread-safe display value queues (`PacketApiQueue`), enabling **concurrent Wi-Fi networking, HTTP/HTTPS web servers, and REST APIs alongside the live TFT display** (`-DMESHTASTIC_ENABLE_TFT_NETWORK=1`).
- **Mesh-Wide Weather Telemetry Engine**:
  - Automatically ingests `meshtastic_EnvironmentMetrics` packets from all neighboring nodes on the mesh into the `WeatherXMModule` station database.
  - Tracks node names, node IDs, boot-time reception freshness, and multi-node rotation pools.
- **Embedded Real-Time Web Weather Dashboard**:
  - Serves a lightweight, zero-dependency dark-mode web app at `/weather` for monitoring weather telemetry across all nodes in the mesh.
- **Full REST API Suite**:
  - Standardized JSON endpoints at `/api/v1/weather/nodes`, `/api/v1/weather`, and `/api/v1/info` for local integrations (Home Assistant, Prometheus, custom dashboards).

---

## 3. Web Weather Dashboard & REST APIs

### Embedded Web Weather Dashboard (`/weather`)

The WG1200 hosts an embedded, responsive real-time weather monitoring interface reachable directly from any browser at `http://<device-ip>/weather` (or `http://meshtastic.local/weather`):

![Weather across your mesh - Web Dashboard](assets/weather_dashboard.png)

#### Key Dashboard Capabilities

- **Mesh Station Sidebar**: Dynamically lists every reporting environmental node heard across the LoRa mesh with active station counts, node IDs, and names.
- **10-Second Auto-Rotation**: Automatically cycles through reporting stations every 10 seconds; tapping any station pauses rotation for inspection.
- **Freshness & Age Badging**: Distinguishes between live readings heard in the current session ("Updated 12s ago") and cached readings retained across reboots.
- **Unit Conversion Toggle**: Instantly toggles between Metric and Imperial units across all cards:
  - Temperature: Celsius (°C) $\longleftrightarrow$ Fahrenheit (°F)
  - Pressure: Hectopascals (hPa) $\longleftrightarrow$ Inches of Mercury (inHg)
  - Wind Speed / Gusts: Meters per second (m/s) $\longleftrightarrow$ Miles per hour (mph)
  - Rainfall: Millimeters (mm) $\longleftrightarrow$ Inches (in)
- **Comprehensive Meteorological Coverage**:
  - **Atmospheric**: Temperature, Relative Humidity, Barometric Pressure, Gas Resistance, IAQ.
  - **Wind**: Speed, Direction (degrees), Gust, Lull.
  - **Precipitation**: Last 1 hour rain, last 24 hours rainfall accumulation.
  - **Solar & Radiation**: Ambient Illuminance (lx), White light, Infrared light, UV index, Ionizing Radiation ($\mu\text{R/h}$).
  - **Soil & Specialized**: Soil Moisture (%), Soil Temperature, Lightning strike count (1h), Storm distance (km).
  - **Probes & ADC**: 8-channel One-Wire temperatures, 8-channel ADC voltages.
- **Resilient & Secure**: Strict XSS prevention using DOM text nodes (`textContent`), background poll intervals with timeout abort controllers, and graceful offline banner transitions when communication drops.

---

### REST API Endpoints

#### 1. Multi-Node Weather Telemetry (`GET /api/v1/weather/nodes`)

Returns an aggregated JSON object listing all environmental stations discovered across the mesh.

```http
GET /api/v1/weather/nodes HTTP/1.1
Host: 192.168.1.177
Accept: application/json
```

**Response (`200 OK`)**:

```json
{
  "nodes": [
    {
      "node_id": "!01628cfa",
      "name": "WeatherXM WS1300 8CFA",
      "age_seconds": 14,
      "metrics": {
        "temperature": 22.2,
        "relative_humidity": 54.0,
        "barometric_pressure": 1007.5,
        "lux": 23591,
        "wind_direction": 91,
        "wind_speed": 0.1,
        "wind_gust": 0.5,
        "rainfall_1h": 0.0,
        "rainfall_24h": 2.54
      }
    },
    {
      "node_id": "!909e603c",
      "name": "WeatherXM WG1200 603C",
      "age_seconds": null,
      "metrics": {
        "temperature": 21.0,
        "relative_humidity": 60.2,
        "barometric_pressure": 1009.1,
        "lux": 1701,
        "wind_direction": 276,
        "wind_speed": 1.8,
        "wind_gust": 2.5,
        "rainfall_1h": 0.0,
        "rainfall_24h": 2.29
      }
    }
  ]
}
```

> [!NOTE]
> `age_seconds` returns `null` when telemetry data was restored from the non-volatile node database cache across reboots rather than heard off LoRa in the current boot session.

#### 2. Local Weather Observation (`GET /api/v1/weather` or `GET /api/v1/observations`)

Returns the current telemetry reading for the primary paired station or onboard sensors:

```http
GET /api/v1/weather HTTP/1.1
Host: 192.168.1.177
```

**Response (`200 OK`)**:

```json
{
  "timestamp": 1790434800,
  "temperature": 21.0,
  "humidity": 60,
  "pressure": 1009.1,
  "wind_speed": 1.8,
  "wind_direction": 276,
  "uv": 1.0,
  "solar_irradiance": 1701.0
}
```

#### 3. Gateway Device Info (`GET /api/v1/info`)

Returns gateway hardware identity, firmware version, LoRa region, and Wi-Fi link parameters:

```http
GET /api/v1/info HTTP/1.1
Host: 192.168.1.177
```

**Response (`200 OK`)**:

```json
{
  "id": "C2:B9:3C:9C:2D:AE:8D:0C:0F:E1",
  "gateway_id": "C2:B9:3C:9C:2D:AE:8D:0C:0F:E1",
  "model": "WXM-WG1200",
  "hardware": "WXM-WG1200",
  "firmware": "2.8.1.fed150c",
  "api_version": "1.0.0",
  "device_name": "WeatherXM WG1200",
  "mac": "24:58:7C:E3:EF:C0",
  "uptime_sec": 360,
  "edition": "wxm",
  "freq_region": "868",
  "wifi": {
    "status": "CONNECTED",
    "ssid": "m",
    "rssi": -52,
    "ip": "192.168.1.177"
  }
}
```

---

## 4. Core Safety Invariants

1. **Immutable Factory Partition (`0x020000`)**:
   The stock WeatherXM firmware is located at partition `factory` (`0x020000`, 4MB). Neither the Meshtastic flasher nor runtime code ever overwrites or erases this partition.
2. **Alternating Inactive OTA Targets**:
   Meshtastic is flashed strictly to the unused OTA partition:
   - When running `factory`: Flashes to `ota_0` (`0x420000`).
   - When running `ota_0`: Flashes to `ota_1` (`0x820000`).
   - When running `ota_1`: Flashes to `ota_0` (`0x420000`).
3. **Partition Geometry Verification**:
   Before writing any byte to flash, the flasher tool reads the binary partition table from `0x0C000` and validates that `factory`, `ota_0`, `ota_1`, and `otadata` exist at exact designated offsets.
4. **Hardware Button Rollback (10 Seconds)**:
   Holding the external user button (**GPIO 38**) for **10 seconds** at power-on triggers `earlyInitVariant()` in Meshtastic, which erases `otadata` and restarts the device directly into the factory WeatherXM firmware.
5. **NVS Namespace Isolation**:
   Performing a "Factory Reset" in Meshtastic does **not** call `nvs_flash_erase()`. Instead, it purges only Meshtastic-specific namespaces (`meshtastic`, `nimble_bond`, `nimble_sec`, `nvs.net80211`), keeping WeatherXM device identity, provisioning certificates, and sensor calibration untouched.

---

## 5. Flash Memory Geometry (16MB)

| Partition             | Subtype              | Offset         | Length                | Description                                 | Protection Level                    |
| :-------------------- | :------------------- | :------------- | :-------------------- | :------------------------------------------ | :---------------------------------- |
| **`esp_secure_cert`** | Custom (0x3F)        | `0x00D000`     | `0x002000` (8 KB)     | Factory Device Certificate & DS Identity    | **READ-ONLY / PREFLIGHT PROTECTED** |
| `nvs`                 | NVS (0x02)           | `0x00F000`     | `0x004000` (16 KB)    | Shared Non-Volatile Storage                 | Namespaced writes only              |
| `otadata`             | OTA Selection (0x00) | `0x013000`     | `0x002000` (8 KB)     | 2x 4KB sectors controlling active boot slot | Managed by bootloader & flasher     |
| **`factory`**         | App Factory (0x00)   | **`0x020000`** | **`0x400000`** (4 MB) | **Original WeatherXM Stock Firmware**       | **READ-ONLY / IMMUTABLE**           |
| **`ota_0`**           | App OTA 0 (0x10)     | **`0x420000`** | **`0x400000`** (4 MB) | Primary Meshtastic Boot Slot                | Overwritten only when inactive      |
| **`ota_1`**           | App OTA 1 (0x11)     | **`0x820000`** | **`0x400000`** (4 MB) | Secondary Meshtastic Boot Slot              | Overwritten only when inactive      |
| `spiffs`              | SPIFFS (0x82)        | `0xC21000`     | `0x3DF000` (~3.8 MB)  | LittleFS / Storage filesystem               | Meshtastic storage                  |

---

## 6. How Boot Switching Works

### ESP-IDF `otadata` Mechanics

The ESP-IDF bootloader determines which partition to boot by reading the 8KB `otadata` partition at `0x013000`:

- Sector 0 (`0x013000 - 0x013FFF`)
- Sector 1 (`0x014000 - 0x014FFF`)

Each sector contains an `esp_ota_select_entry_t` (32 bytes) with:

- `ota_seq`: 32-bit integer incremented with each update.
- `crc`: 32-bit CRC over `ota_seq` (`binascii.crc32(pack('<I', seq), 0xFFFFFFFF) % 2^32`).

```mermaid
stateDiagram-v2
    [*] --> Factory: Blank/Erased otadata (0xFF)
    Factory --> Meshtastic_OTA_0: Flash Meshtastic (seq=1 -> ota_0)
    Meshtastic_OTA_0 --> Meshtastic_OTA_1: Update Meshtastic (seq=2 -> ota_1)
    Meshtastic_OTA_1 --> Meshtastic_OTA_0: Update Meshtastic (seq=3 -> ota_0)
    Meshtastic_OTA_0 --> Factory: Hold Button 10s OR --rollback (Erase otadata)
    Meshtastic_OTA_1 --> Factory: Hold Button 10s OR --rollback (Erase otadata)
```

1. **Factory State**: When `otadata` contains `0xFF`s (empty/uninitialized), the bootloader automatically boots `factory` (`0x020000`).
2. **First Meshtastic Flash**:
   - Flasher checks `otadata`, detects `factory` is active.
   - Signs binary with Secure Boot V2.
   - Writes binary to `ota_0` (`0x420000`).
   - Writes Sector 0 in `otadata` with `ota_seq = 1` and valid CRC32.
   - On reboot, bootloader boots `ota_0`.
3. **Subsequent Meshtastic Updates**:
   - Flasher queries `otadata`. If `ota_0` is active (`seq = 1`), it writes to `ota_1` (`0x820000`) with `seq = 2`.
4. **Returning to WeatherXM**:
   - Rollback clears `otadata` with `0xFF`.
   - On the next boot, ESP-IDF bootloader sees no valid OTA slot and boots `factory` (`0x020000`).

---

## 7. Hardware Button Rollback (10 Seconds)

The WG1200 user push button is connected to **GPIO 38** (active LOW, normally pulled HIGH).

During early boot, `earlyInitVariant()` in `src/platform/extra_variants/weatherxm_wg1200/variant.cpp`:

1. Samples GPIO 38 before higher-level subsystems start.
2. If held `LOW`, it loops for **10 full seconds (10,000 ms)**.
3. If released before 10s, rollback cancels and Meshtastic boots normally.
4. If held for the full 10s:
   - Erases the `otadata` partition (`0x013000`).
   - Calls `esp_restart()`.
   - ESP-IDF bootloader boots stock WeatherXM from `factory` (`0x020000`).

---

## 8. Flasher Tool Reference (`wg1200_upload.py`)

The upload script is located at `variants/esp32s3/weatherxm-wg1200/wg1200_upload.py`.

### PlatformIO Target Environments

| Environment                    | Description                                                                                                           |
| :----------------------------- | :-------------------------------------------------------------------------------------------------------------------- |
| **`weatherxm-wg1200-tft`**     | **Recommended.** Full ST7701 480x480 circular TFT UI, LovyanGFX, capacitive touch, and concurrent Web/API networking. |
| `weatherxm-wg1200`             | Base WG1200 build with Meshtastic Device UI.                                                                          |
| `weatherxm-wg1200-standard-ui` | Headless / non-TFT variant for headless gateway deployments.                                                          |

### Build & Upload Commands

```bash
# Build TFT firmware
pio run -e weatherxm-wg1200-tft

# Safe Flash & Switch active slot via USB
pio run -e weatherxm-wg1200-tft -t upload --upload-port <port>
```

PlatformIO automatically invokes `wg1200_upload.py`, which finds the signing key, verifies the partition layout, targets the inactive OTA slot, and updates `otadata`.

### Inspect Boot Status

To inspect active boot slot and verify partition table without modifying flash:

```bash
python variants/esp32s3/weatherxm-wg1200/wg1200_upload.py --status [port]
```

### Revert to Factory WeatherXM Firmware via Software

```bash
python variants/esp32s3/weatherxm-wg1200/wg1200_upload.py --rollback [port]
```

---

## 9. Meshtastic NVS Factory Reset Isolation

In standard Meshtastic, node factory reset calls `nvs_flash_erase()`. On WeatherXM hardware, this would wipe claiming certificates, MAC keys, and barometric/thermal calibration.

On WG1200 (`src/mesh/NodeDB.cpp`), NVS reset is scoped strictly to Meshtastic namespaces:

```cpp
#if defined(WG1200)
    Preferences preferences;
    if (preferences.begin("meshtastic", false)) {
        preferences.clear();
        preferences.end();
    }
    const char *meshtastic_nvs_namespaces[] = {"nimble_bond", "nimble_sec", "nvs.net80211", NULL};
    for (int i = 0; meshtastic_nvs_namespaces[i] != NULL; i++) {
        nvs_handle_t handle = 0;
        if (nvs_open(meshtastic_nvs_namespaces[i], NVS_READWRITE, &handle) == ESP_OK) {
            nvs_erase_all(handle);
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
#else
    nvs_flash_erase();
#endif
```

This safely clears node channels, BLE bonds, and WiFi credentials, while leaving WeatherXM credentials and factory sensor calibration completely intact.

---

## 10. Historical Bootloader Analysis & Hardware Safety (Pin 10 vs Pin 38)

### The GPIO 10 Bootloader

Disassembly analysis of the stock 2nd-stage bootloader on early production units (compiled **May 27, 2024 at 11:52:34**) revealed that `CONFIG_BOOTLOADER_FACTORY_RESET` was enabled on **GPIO 10** with a 5-second hold threshold:

```assembly
403c99f1: movi.n a12, 0      ; active level = LOW
403c99f3: movi.n a11, 5      ; hold time = 5 seconds
403c99f5: movi.n a10, 10     ; pin = GPIO 10!
403c99f7: call8  bootloader_common_check_long_hold_gpio_level
403c99fc: bnei   a10, 1, 0x403c9a54
403c9a07: l32r   a10, "Detect a condition of the factory reset"
```

On WG1200 hardware, **GPIO 10 is the ST7701 LCD RGB data line (G0)**. When the display is active or during warm reset, this line can be pulled low by the display controller or uninitialized pad circuitry. If held low for 5 seconds at boot, the bootloader triggers a hardware factory reset, wiping `otadata` and `nvs`.

### Defense-in-Depth in Meshtastic

To ensure total immunity against false factory resets on units with this bootloader:

1. **Early Boot Pin Conditioning (`earlyInitVariant`)**:
   GPIO 10 is immediately configured as `GPIO_MODE_INPUT_OUTPUT`, driven HIGH, and its pad hold is released.
2. **Warm Reset Pad-Hold (`esp_register_shutdown_handler`)**:
   A shutdown hook drives GPIO 10 HIGH and engages hardware pad hold (`gpio_hold_en(GPIO_NUM_10)`) across warm resets and software restarts.
3. **OTA Rollback Cancellation (`esp_ota_mark_app_valid_cancel_rollback`)**:
   Meshtastic explicitly cancels the ESP-IDF bootloader rollback timer upon successful initialization, confirming the active OTA slot as verified and permanent.

---

## 11. Runtime Meshtastic OTA Disabling on Secure Boot V2

On the WeatherXM WG1200, **Secure Boot V2 is permanently enabled in hardware eFuses**. The ROM bootloader mandates that any executable image in `factory`, `ota_0`, or `ota_1` be signed with the authentic WeatherXM RSA private key.

Standard Meshtastic OTA (via Web Bluetooth, mobile apps, or Admin protobuf messages) is designed for non-secure-boot devices and writes unsigned binary chunks into `ota_1`. If triggered on WG1200, this would:

1. Overwrite the backup/secondary signed Meshtastic slot.
2. Cause the ROM bootloader to immediately reject the unsigned binary on next boot (`ESP_ERR_IMAGE_INVALID`), breaking the dual-firmware state.

### Implementation Guards

To completely eliminate this risk:

- **`AdminModule.cpp`**: Guards `meshtastic_AdminMessage_ota_request_tag` with `#if defined(WG1200)`. Any incoming OTA packet is immediately rejected, and a diagnostic message is printed to the serial console.
- **`MeshtasticOTA.cpp`**:
  - `getAppPartition()` returns `NULL` on `WG1200`, ensuring `ota_1` is never treated as a scratch/disposable OTA loader.
  - `checkOTACapability()` and `trySwitchToOTA()` strictly return `false`.

---

## 12. Developer Tooling: Preflight Checks, Validated Backups & Safe Erase

The companion upload utility `variants/esp32s3/weatherxm-wg1200/wg1200_upload.py` provides end-to-end safety checks:

### Step 0: Preflight `esp_secure_cert` Verification

Before every upload, the tool automatically reads the 8 KB certificate partition (`0x00D000`):

1. Verifies the authentic WeatherXM TLV magic `0xBA5EBA11` (`\x11\xba\x5e\xba`).
2. Extracts and displays the hardware device serial number (e.g. `WXM_SERIAL_123456789`).
3. Automatically archives a timestamped snapshot to `backups/secure_cert/secure_cert_<SERIAL>_<TIMESTAMP>.bin`.
4. If the magic is missing or corrupted, the tool warns the developer and prompts for explicit confirmation before proceeding.

### Full 16MB Validated Backup

For engineering devices, create a full 16MB snapshot with cryptographic and structural validation:

```bash
python variants/esp32s3/weatherxm-wg1200/wg1200_upload.py --backup [PORT] [BAUD] [OUTPUT_FILE]
```

The backup validator confirms:

- Exact 16,777,216 bytes length.
- Valid ESP32 bootloader magic byte (`0xE9` at `0x0000`).
- Valid partition table magic (`0x50AA` at `0x00C000`) and validates `factory`, `ota_0`, `ota_1`, `otadata`.
- Intact `esp_secure_cert` partition containing `0xBA5EBA11` TLV magic.

### Safe Flash Erase (`--erase`)

Standard `esptool.py erase_flash` destroys factory device identity. The WG1200 safe erase command strictly enforces:

```bash
python variants/esp32s3/weatherxm-wg1200/wg1200_upload.py --erase [PORT] [BAUD]
```

- **Mandatory Preflight**: Checks `0x00D000` and ensures a validated backup of `esp_secure_cert` is stored on disk before any erase operation is allowed.
- If `esp_secure_cert` is invalid or cannot be backed up, the erase is blocked immediately.

### Certificate Restore (`--restore-cert`)

If an engineering device had its flash wiped, restore the verified certificate partition:

```bash
python variants/esp32s3/weatherxm-wg1200/wg1200_upload.py --restore-cert <file_path> [PORT] [BAUD]
```

Validates the TLV magic `0xBA5EBA11` within the backup file before writing it back to `0x00D000`.

---

## 13. Release Versioning & Artifact Packaging

The WeatherXM WG1200 firmware uses a **SemVer Vendor-Tagged** convention (`<upstream-base>-wxm.<revision>`):

- **Single Source of Truth**: [`version.properties`](file:///Users/manos/Documents/mesh/meshtastic/version.properties)
  ```ini
  [VERSION]
  major = 2
  minor = 8
  build = 1
  rev = wxm.1
  ```
- **Clean Release Version**: `2.8.1-wxm.1` (11 characters).
- **Protobuf 17-Character Invariant**: The Nanopb wire protocol defines `MyNodeInfo.firmware_version` as a fixed 18-byte buffer (`char[18]`). In development builds, `bin/readprops.py` trims the commit hash (e.g. `2.8.1-wxm.1.fed15`) so the total string never exceeds 17 characters, eliminating wire-format truncation and buffer overflows. In formal release builds (`RELEASE_BUILD=1` or Git tag builds), the commit hash is omitted entirely (`2.8.1-wxm.1`).
- **Release Signed Binary Artifact**:
  `build/weatherxm-wg1200-tft/firmware-weatherxm-wg1200-tft-2.8.1-wxm.1-signed.bin`
- **Bumping Revisions**:
  - To bump the WeatherXM revision (`wxm.1` &rarr; `wxm.2`):
    ```bash
    python3 bin/bump_version.py --rev
    ```
  - To bump the upstream Meshtastic base build number:
    ```bash
    python3 bin/bump_version.py
    ```
- **Git Release Tags**:
  Official release tags in Git follow the pattern:
  ```bash
  git tag v2.8.1-wxm.1
  git push origin v2.8.1-wxm.1
  ```
