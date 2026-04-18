# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an Arduino sketch for an ESP32-based LoRa weather station **receiver** node. It receives binary-packed structs over LoRa radio from a `weather_v4_lora` transmitter, then publishes the data to an MQTT broker and renders it on a Waveshare 4.2" e-paper display.

## Build & Flash

This project uses the **Arduino IDE**. There is no Makefile or CMake build system.

- Open `weather_v4_lora_receiver.ino` in Arduino IDE
- Required board: **ESP32** — install via Board Manager, version **3.3.8** (ESP-IDF 5.x)
- Flash via Arduino IDE Upload button

Key libraries required (install via Library Manager):
- `LoRa` (Sandeep Mistry)
- `GxEPD2` — for `WAVESHARE_R22` builds
- `GxEPD` + `GxGDEW042T2` — for legacy e-paper builds
- `PubSubClient` — MQTT
- `BME280I2C` (Tyler Glenn) — receiver enclosure sensor
- `U8g2` — only for `DEV_HELTEC_RECEIVER` builds

## Configuration (`conf.h`)

**Credentials are in `secrets.h` (git-ignored).** Copy `secrets.h.template` → `secrets.h` and fill in real values. `conf.h` includes `secrets.h` at the top.

| Define | Purpose |
|---|---|
| `WAVESHARE_R22` | Selects GxEPD2 driver for Waveshare 4.2" Rev 2.2 (vs legacy GxEPD) |
| `DEV_HELTEC_RECEIVER` | Enables Heltec ESP32 LoRa v2 pin mapping + OLED display |
| `E_PAPER` | Enables e-paper display output |
| `IMPERIAL` | Displays °F and inHg instead of °C and mbar |
| `SerialMonitor` | Enables `MonPrintf()` output to Serial |
| `RECEIVER_BME280` | Enables BME280 mounted in receiver enclosure |
| `DEVID` | Device ID filter — must match transmitter's `DEVID` |
| `SYNC` | LoRa sync word — must match transmitter (`0x54`) |
| `BAND` | LoRa frequency (915E6 for US, 868E6 EU, 433E6) |
| `ADCBattery` / `ADCSolar` | Calibration divisors for transmitter voltage ADC conversion |
| `R1`, `R2`, `VOLTAGE_REF` | Receiver battery voltage divider constants |
| `mainTopic` | MQTT topic prefix string (e.g. `"PriorLake/"`) |

## Architecture

The sketch is split across multiple `.ino` files (Arduino IDE compiles all `.ino` files in the sketch folder together):

- **`weather_v4_lora_receiver.ino`** — main file: `setup()`, `loop()`, struct definitions (`sensorData`, `diagnostics`, both `__attribute__((packed))`), LoRa packet parsing in `cbk()`, e-paper display functions (`eTitle`, `eSensors`, `eHardware`, `eUpdate`)
- **`mqtt.ino`** — MQTT publishing; overloaded `MQTTPublish()` for int/float/bool/string; `SendDataMQTT()` overloads for `sensorData` and `diagnostics`; connects on each send cycle and disconnects after; uses `snprintf(topicBuffer, sizeof(topicBuffer), "%s%s", mainTopic, topic)` to build topics
- **`wifi.ino`** — WiFi connection with 30-second timeout; returns 0 on failure
- **`wind.ino`** — `setWindDirection()`: maps raw ADC to cardinal direction and degrees via lookup table using `const char*` arrays; populates global `wind` struct with `strncpy`
- **`display.ino`** — OLED functions for `DEV_HELTEC_RECEIVER` builds only (`LEDTitle`, `LEDStatus`, `blink`)
- **`time.ino`** — NTP time sync (present but not called in main loop)
- **`conf.h`** — all configuration and hardware defines (no credentials)
- **`secrets.h`** — WiFi SSID/password, MQTT server/port (git-ignored)
- **`secrets.h.template`** — committed placeholder template

### Data Flow

1. Transmitter sends one of two binary structs over LoRa: `sensorData` (environment) or `diagnostics` (hardware)
2. `cbk()` bounds-checks `packetSize` against `sizeof(packetBinary)`, reads raw bytes as `(byte)` casts, and `memcpy`s into the appropriate global struct after verifying `deviceID == DEVID`
3. Packet type is identified by `packetSize` matching `sizeof(sensorData)` or `sizeof(diagnostics)`
4. `SendDataMQTT()` publishes all fields under `mainTopic` + `"sensors/..."` or `"hardware/..."`
5. Heartbeat MQTT publish fires every 10 seconds (tracked via `static unsigned long lastHeartbeatMs`)
6. E-paper display refreshes every 60 seconds (tracked via `static unsigned long lastDisplayMs`)

### Packed structs

Both `sensorData` and `diagnostics` are `__attribute__((packed))` — must exactly match the transmitter's struct layout. The receiver only ever accesses these structs via `memcpy` (not library calls), so the packed-field reference issue that affects the transmitter's `bme.read()` calls does not apply here.

### Display Driver Divergence

Two incompatible e-paper driver APIs are supported, selected by `#ifdef WAVESHARE_R22`:
- **GxEPD2** (Rev 2.2): uses `display.firstPage()` / `display.nextPage()` loop pattern
- **GxEPD** (legacy): uses `display.fillScreen()` + `display.update()` pattern

When modifying display code, changes often need to be duplicated in both `#ifdef` branches.

### MQTT Topic Layout

All topics are prefixed with `mainTopic` (default: `PriorLake/`):
- `sensors/imperial/` — temperature (°F), wind speed (MPH), rainfall (in), pressure (inHg)
- `sensors/metric/` — same fields in metric units
- `sensors/windDirection/`, `sensors/windCardinalDirection/`
- `sensors/lux/`, `sensors/UVIndex/`, `sensors/relHum/`
- `hardware/` — boot count, RSSI, battery/solar voltages, case temp, ESP32 core temp, receiver battery, receiver BME280

### Unit Conversions

All conversions use float literals to avoid implicit double promotion:
- Temperature: `temperatureC * 9.0f / 5.0f + 32.0f`
- Wind speed: `windSpeed / 1.609f` (km/h → MPH)
- Pressure: `barometricPressure * 0.000295333f` (Pa → inHg) — value is in Pa from BME280 `PresUnit_Pa`

### Receiver Battery Voltage

GPIO 34 reads a resistor divider (R1 = R2 = 100 kΩ, `VOLTAGE_REF` = 3.65 V):
```cpp
float voltage = (adcRaw / ADC_MAX) * VOLTAGE_REF * ((R1 + R2) / R2);
```

## Debugging

Enable/disable serial output globally with `#define SerialMonitor` in `conf.h`. Use `MonPrintf()` for all debug output. WiFi and MQTT connection status is printed on each connect attempt.
