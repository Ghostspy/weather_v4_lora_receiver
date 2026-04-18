# weather_v4_lora_receiver

ESP32-based LoRa weather station receiver. Receives binary-packed sensor data
from the `weather_v4_lora` transmitter, publishes all fields to an MQTT broker,
and renders a summary on a Waveshare 4.2" e-paper display.

---

## Requirements

- **ESP32 Arduino core 3.x** (ESP-IDF 5.x) — tested on **3.3.8**
- Arduino IDE
- WiFi network with MQTT broker
- Waveshare 4.2" e-paper Rev 2.2 (or Heltec ESP32 LoRa v2 for the alternate build)

---

## Hardware

### Default build (Waveshare e-paper receiver)

| Pin | GPIO | Purpose |
|-----|------|---------|
| LoRa CS | 18 | LoRa SPI chip select |
| LoRa RST | 14 | LoRa reset |
| LoRa IRQ | 26 | LoRa interrupt |
| BATTERY_PIN | 34 | Receiver battery voltage divider ADC |
| BME280_SDA | 21 | Receiver BME280 I2C data |
| BME280_SCL | 22 | Receiver BME280 I2C clock |
| BME280_ADDR | 0x76 | BME280 I2C address (SDO LOW) |

### Alternate build (`DEV_HELTEC_RECEIVER`)

Uses Heltec ESP32 LoRa v2 pin mapping. Enable by uncommenting `#define DEV_HELTEC_RECEIVER` in `conf.h`.

---

## Required Libraries

Install via the Arduino Library Manager:

- **LoRa** (by Sandeep Mistry)
- **GxEPD2** — e-paper driver for Waveshare Rev 2.2 (`WAVESHARE_R22` build)
- **PubSubClient** — MQTT client
- **BME280** (BME280I2C — by Tyler Glenn) — receiver enclosure temperature/humidity

For legacy e-paper builds (without `WAVESHARE_R22`):
- **GxEPD** + **GxGDEW042T2**

For Heltec receiver builds (`DEV_HELTEC_RECEIVER`):
- **U8g2** — OLED display
- **Heltec ESP32 Dev-Boards**

---

## Setup

### Credentials

Credentials are stored in `secrets.h` (git-ignored). Copy the template and fill
in your values:

```bash
cp secrets.h.template secrets.h
```

Edit `secrets.h`:
```cpp
const char* ssid     = "your-wifi-ssid";
const char* password = "your-wifi-password";
const char* mqttServer = "192.168.x.x";
const int   mqttPort   = 1883;
```

### Configuration (`conf.h`)

| Define | Default | Description |
|--------|---------|-------------|
| `DEVID` | `0x11223344` | Transmitter device ID filter — must match transmitter |
| `SYNC` | `0x54` | LoRa sync word — must match transmitter |
| `BAND` | `915E6` | LoRa frequency (915E6 US, 868E6 EU, 433E6) |
| `IMPERIAL` | defined | Display °F and inHg; comment out for metric |
| `WAVESHARE_R22` | defined | Use GxEPD2 driver for Waveshare 4.2" Rev 2.2 |
| `E_PAPER` | defined | Enable e-paper display output |
| `DEV_HELTEC_RECEIVER` | undefined | Heltec ESP32 LoRa v2 pin mapping + OLED |
| `RECEIVER_BME280` | defined | Read BME280 mounted in receiver enclosure |
| `SerialMonitor` | defined | Comment out to silence all serial debug output |
| `ADCBattery` | `371` | Calibration divisor for battery voltage ADC |
| `ADCSolar` | `195` | Calibration divisor for solar voltage ADC |
| `OFFSET_IN` | `5.58` | Altitude pressure offset (inHg) |
| `OFFSET_MM` | `141.7` | Altitude pressure offset (mm Hg) |
| `WDT_TIMEOUT` | `30` | Watchdog timeout in seconds |
| `mainTopic` | `"PriorLake/"` | MQTT topic prefix |

---

## Building and Flashing

1. Install the ESP32 Arduino core **3.3.8** via the Arduino Boards Manager
2. Install all required libraries
3. Copy `secrets.h.template` → `secrets.h` and fill in credentials
4. Open `weather_v4_lora_receiver.ino` in the Arduino IDE
5. Select your ESP32 board and port
6. Compile and upload
7. Open the Serial Monitor at **115200 baud** to verify reception

---

## How It Works

The receiver runs a standard Arduino `loop()`:

1. `LoRa.onReceive(cbk)` registers the packet callback
2. On each LoRa packet, `cbk()` reads raw bytes, bounds-checks against the
   larger of the two expected struct sizes, then `memcpy`s into the appropriate
   global struct after verifying `deviceID == DEVID`
3. A heartbeat MQTT publish fires every 10 seconds
4. The e-paper display refreshes every 60 seconds
5. WiFi reconnects automatically if the connection drops

### Packet identification

Both transmitter structs (`sensorData` and `diagnostics`) are `__attribute__((packed))` and have `deviceID` at offset 0. The receiver distinguishes them by `packetSize`:
- `sizeof(sensorData)` → environmental data → `SendDataMQTT(sensorData&)`
- `sizeof(diagnostics)` → hardware data → `SendDataMQTT(diagnostics&)`

### MQTT topic layout

All topics are prefixed with `mainTopic` (default `PriorLake/`):

| Topic suffix | Content |
|---|---|
| `sensors/imperial/temperature` | °F |
| `sensors/imperial/windSpeed` | MPH |
| `sensors/imperial/rainfall24h` | inches |
| `sensors/imperial/rainfall60min` | inches |
| `sensors/imperial/pressure` | inHg (altitude-corrected) |
| `sensors/metric/temperature` | °C |
| `sensors/metric/windSpeed` | km/h |
| `sensors/metric/rainfall24h` | mm |
| `sensors/metric/rainfall60min` | mm |
| `sensors/metric/pressure` | mm Hg (altitude-corrected) |
| `sensors/windDirection/` | raw ADC |
| `sensors/windCardinalDirection/` | e.g. `NNE` |
| `sensors/lux/` | lux |
| `sensors/UVIndex/` | UV index |
| `sensors/relHum/` | % |
| `hardware/bootCount` | transmitter boot count |
| `hardware/RSSI` | dBm |
| `hardware/batteryVoltage` | V |
| `hardware/solarVoltage` | V |
| `hardware/caseTemperature` | °C (BME280 in transmitter) |
| `hardware/ESPCoreTemperature` | °C (ESP32 die temp) |
| `hardware/receiverBattery` | V (receiver battery) |
| `hardware/receiverTemperature` | °C (receiver BME280) |
| `hardware/receiverHumidity` | % (receiver BME280) |

### Receiver battery voltage

Calculated from a resistor divider (R1 = R2 = 100 kΩ) on GPIO 34:

```
voltage = (adcRaw / ADC_MAX) × VOLTAGE_REF × ((R1 + R2) / R2)
```

### Wind direction

`setWindDirection()` in `wind.ino` maps raw ADC to a lookup table of 15 entries
covering the standard 16-point compass (excluding due South, which has no
distinct ADC value in the resistor network). It populates a global `wind` struct
with `degrees` (float) and `cardinalDirection` (char array).
