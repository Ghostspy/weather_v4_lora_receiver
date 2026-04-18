//LoRa baseed weather station - receiver node
//Hardware design by Debasish Dutta - opengreenenergy@gmail.com
//Software design by James Hughes - jhughes1010@gmail.com

/* History
   0.9.0 10-02-22 Initial development for Heltec ESP32 LoRa v2 devkit

   1.0.2 11-17-22 Remapping all mqtt topics

   1.1.0 11-24-22 sensor structure expanded to receive max wind speed also
                  LED display online for Heltec board

   1.1.1 12-13-22 Error in code where non-Heltec build would not compile properly
                  Missing #ifdef statements were added to correct this 

   1.1.2 01-27-23 Packet type details added to display.
                  Explicitly checking both packet sizes before memcpy
                  Added WDT of 30 sec in case of hang up in loop() 
                  Set sender and receiver sync word to filter correct transmissions
                  Add 10 sec heartbeat LED for Heltec PCB   

   1.1.3 07-08-23 Removed Blynk header file reference and replace esp_wifi with WiFi

   1.2.0 07-30-23 Support for e-paper (4.2in Waveshare)
                  Basic, raw data output, nothing fancy
                  Output hardware and environment packet sizes as they need to match TX
                  #define for optional e-paper display

   1.2.1 08-06-23 Addition of Battery and Solar voltages in addition to ADC values

   1.2.2 09-09-23 Correctly calculate inHg
                  Hearbeat tweaks '.' on Serial port

   1.2.3 12-06-24 Added support for Waveshare 4.2" e-Paper Rev 2.2
                  Added displaying values in Farhenheit and inHg
                  Requires ESP32 Board Manager 2.0.12

   1.3.0 04-17-26 ESP32 Arduino 3.x.x compatibility (ESP-IDF 5.x)
                  esp_task_wdt_init -> esp_task_wdt_reconfigure (TWDT pre-initialized by framework)
                  Moved SPI.h to top-level include, removed duplicate
                  Removed stale 2.x.x WDT commented code
                  Fixed bitwise OR bug in display update condition
                  vsprintf/sprintf -> vsnprintf/snprintf (overflow safety)
                  Replaced String heap allocations in wind.ino with const char*
                  Removed unused rssi/packSize/packet/rssi_wifi globals

   1.3.3 04-17-26 reconnect() was missing credentials (rc=5 UNAUTHORIZED) causing infinite retry loop
                  reconnect() now limited to 3 attempts with esp_task_wdt_reset() between retries
                  MQTTPublish overloads bail out early if reconnect fails

   1.3.2 04-17-26 esp_task_wdt_reset() added inside e-paper do/while loop and eTitle()
                  Waveshare 4.2" full refresh (~15-22s) was exceeding 30s WDT between loop() calls
                  MQTT while(1) hang replaced with return on connect failure (prevents WDT reboot)

   1.3.1 04-17-26 Receiver BME280 support (GPIO21 SDA / GPIO22 SCL)
                  Reads temp, humidity, pressure every 10s; publishes to receiver/ MQTT topics
                  Fills receiver enclosure temperature on e-paper display
*/

//Hardware build target: ESP32
#define VERSION "1.3.3"

//#include "heltec.h"
#include "conf.h"
#include <SPI.h>
#include <LoRa.h>

// e-paper pins mapping
#define CS 5
#define DC 27
#define DISP_RST 26
#define BUSY 25

#ifdef WAVESHARE_R22
// GxEPD2 - Required for Waveshare 4.2" Rev. 2.2
#include <GxEPD2_BW.h>

// Define the driver class for your specific e-paper model
#define GxEPD2_DRIVER_CLASS GxEPD2_420_GDEY042T81

// Declare the display object using the driver class
GxEPD2_BW<GxEPD2_DRIVER_CLASS, GxEPD2_DRIVER_CLASS::HEIGHT> display(GxEPD2_DRIVER_CLASS(CS, DC, DISP_RST, BUSY));
#else
// GxEPD
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h> // 4.2" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

GxIO_Class io(SPI, CS, DC, DISP_RST);
GxEPD_Class display(io, DISP_RST, BUSY);
#endif

#include <Wire.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <PubSubClient.h>
#ifdef RECEIVER_BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#endif
#ifdef DEV_HELTEC_RECEIVER
#include <U8g2lib.h>
#endif

byte packetBinary[512];
float rssi_lora;

#ifdef RECEIVER_BME280
Adafruit_BME280 receiverBME;
float receiverTempC    = 0.0;
float receiverHumidity = 0.0;
float receiverPressureHPa = 0.0;
bool  receiverBMEok    = false;
#endif

#ifdef DEV_HELTEC_RECEIVER
U8G2_SSD1306_128X64_NONAME_F_SW_I2C led(U8G2_R0, /* clock=*/15, /* data=*/4, /* reset=*/16);
#endif

//===========================================
// Weather-environment structure
//===========================================
struct __attribute__((packed)) sensorData {
  int deviceID;
  int windDirectionADC;
  int rainTicks24h;
  int rainTicks60m;
  float temperatureC;
  float windSpeed;
  float windSpeedMax;
  float barometricPressure;
  float humidity;
  float UVIndex;
  float lux;
};

struct __attribute__((packed)) diagnostics {
  int deviceID;
  float BMEtemperature;
  int batteryADC;
  int solarADC;
  int coreC;
  int bootCount;
  bool chargeStatusB;
};

struct derived {
  char cardinalDirection[5];
  float degrees;
};

struct sensorData environment;
struct diagnostics hardware;
struct derived wind;

//===========================================
// LoRaData: acknowledge LoRa packet received on OLED
//===========================================
void LoRaData() {
  static int count = 1;
  char buffer[20];
  sprintf(buffer, "Count: %i", count);
  MonPrintf("%s\n", buffer);
  count++;
}

//===========================================
// cbk: retreive contents of the received packet
//===========================================
void cbk(int packetSize) {
  for (int i = 0; i < packetSize; i++) {
    packetBinary[i] = (char)LoRa.read();
  }
  rssi_lora = LoRa.packetRssi();
  if (packetSize == sizeof(environment)) {
    memcpy(&environment, &packetBinary, packetSize);
  } else if (packetSize == sizeof(hardware)) {
    memcpy(&hardware, &packetBinary, packetSize);
  }
  LoRaData();
}

//===========================================
// setup:
//===========================================
void setup() {
  Serial.begin(115200);

  // In ESP32 Arduino 3.x.x the framework initializes TWDT before setup() runs,
  // so reconfigure it (not reinitialize) then subscribe the main task.
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WDT_TIMEOUT * 1000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
  };

  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);

#ifdef DEV_HELTEC_RECEIVER
  led.begin();
  LEDTitle();
#endif

  Serial.println("LoRa Receiver");
  Serial.println(VERSION);
#ifdef DEV_HELTEC_RECEIVER
  LoRa.setPins(18, 14, 26);
  pinMode(LED, OUTPUT);
#else
  LoRa.setPins(15, 17, 13);
#endif
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
  LoRa.receive();
  wifi_connect();
#ifdef DEV_HELTEC_RECEIVER
  OLEDConnectWiFi();
#endif
  //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  LoRa.enableCrc();
  LoRa.setSyncWord(SYNC);
  Serial.printf("LoRa receiver is online\n");

#ifdef RECEIVER_BME280
  Wire.begin(BME280_SDA, BME280_SCL);
  receiverBMEok = receiverBME.begin(BME280_ADDR, &Wire);
  if (receiverBMEok) {
    Serial.printf("Receiver BME280 online (0x%02X)\n", BME280_ADDR);
  } else {
    Serial.printf("Receiver BME280 not found at 0x%02X - check wiring and SDO pin\n", BME280_ADDR);
  }
#endif

#ifdef E_PAPER
  // Initialize the display
  display.init();
  Serial.println("Display initialized");  
  display.setRotation(2);             // Set the rotation if needed (0, 1, 2, or 3)
  display.setTextColor(GxEPD_BLACK);  // Set the text color to black
  eTitle();
#endif

  //data structure stats
  MonPrintf("Hardware size: %i\n", sizeof(hardware));
  MonPrintf("Sensor size: %i\n", sizeof(environment));
}

//===========================================
// loop:
//===========================================
void loop() {
  static bool firstUpdate = true;
  esp_task_wdt_reset();
  static int count = 0, Scount = 0, Hcount = 0, Xcount = 0;
  int upTimeSeconds = 0;
  int packetSize = LoRa.parsePacket();

  environment.deviceID = 0;
  hardware.deviceID = 0;

  if (packetSize) {
    count++;
    cbk(packetSize);
    Serial.printf("Packet size: %i\n", packetSize);

    MonPrintf("Environment deviceID %x\n", environment.deviceID);
    MonPrintf("Hardware deviceID %x\n", hardware.deviceID);
    //check for weather data packet
    if (packetSize == sizeof(environment) && environment.deviceID == DEVID) {
      PrintEnvironment(environment);
      SendDataMQTT(environment);
      Scount++;
    }
    //check for hardware data packet
    else if (packetSize == sizeof(hardware) && hardware.deviceID == DEVID) {
      PrintHardware(hardware);
      SendDataMQTT(hardware);
      Hcount++;
      hardware.bootCount = hardware.bootCount % 1440;
    } else {
      Xcount++;
    }
#ifdef DEV_HELTEC_RECEIVER
    LEDStatus(count, Scount, Hcount, Xcount);
#endif
  }
  delay(10);
  if (firstUpdate || millis() % 10000 < 5) {
    MonPrintf(".");
    upTimeSeconds = millis() / 60000;

#ifdef RECEIVER_BME280
    if (receiverBMEok) {
      receiverTempC         = receiverBME.readTemperature();
      receiverHumidity      = receiverBME.readHumidity();
      receiverPressureHPa   = receiverBME.readPressure() / 100.0;
      SendReceiverBME280MQTT();
    }
#endif

#ifdef E_PAPER
  #ifdef WAVESHARE_R22
    // GxEPD2 - Required for Waveshare 4.2" Rev. 2.2
    if (firstUpdate || millis() % 60000 < 500) {
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            eSensors();
            eHardware();
            esp_task_wdt_reset(); // e-paper full refresh can exceed 15s; reset WDT each page
        } while (display.nextPage());
        firstUpdate = false;
    }
  #else
    // GxEPD
    if (firstUpdate || millis() % 60000 < 500) {
      display.fillScreen(GxEPD_WHITE);
      eUpdate(count, Hcount, Scount, Xcount, upTimeSeconds);
      eSensors();
      eHardware();
      esp_task_wdt_reset();
      display.update();
    }
  #endif  
#endif
    firstUpdate = false;
  }
}

//===========================================
// HexDump: output hex data of the environment structure - going away
//===========================================
void HexDump(int size) {
  //int size = 28;
  int x;
  char ch;
  char* p = (char*)&environment;

  for (x = 0; x < size; x++) {
    Serial.printf("%02X ", p[x]);
  }
  Serial.println();
}

//===========================================
// PrintEnvironment: Dump environment structure to console
//===========================================
void PrintEnvironment(struct sensorData environment) {
  MonPrintf("Rain Ticks 24h: %i\n", environment.rainTicks24h);
  MonPrintf("Rain Ticks 60m: %i\n", environment.rainTicks60m);
  MonPrintf("Temperature: %f\n", environment.temperatureC);
  MonPrintf("Wind speed: %f\n", environment.windSpeed);
  // TODO Serial.printf("Wind direction: %f\n", environment.windDirection);
  MonPrintf("barometer: %f\n", environment.barometricPressure);
  MonPrintf("Humidity: %f\n", environment.humidity);
  MonPrintf("UV Index: %f\n", environment.UVIndex);
  MonPrintf("Lux: %f\n", environment.lux);
}

//===========================================
// PrintEnvironment: Dump hardware structure to console
//===========================================
void PrintHardware(struct diagnostics hardware) {
  MonPrintf("Boot count: %i\n", hardware.bootCount);
  MonPrintf("Case Temperature: %f\n", hardware.BMEtemperature);
  //Serial.printf("Battery voltage: %f\n", hardware.batteryVoltage);
  MonPrintf("Battery ADC: %i\n", hardware.batteryADC);
  MonPrintf("Solar ADC: %i\n", hardware.solarADC);
  MonPrintf("ESP32 core temp C: %i\n", hardware.coreC);
}

//===========================================
// MonPrintf: diagnostic printf to terminal
//===========================================
void MonPrintf(const char* format, ...) {
  char buffer[200];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
#ifdef SerialMonitor
  Serial.printf("%s", buffer);
#endif
}

#ifdef WAVESHARE_R22
// GxEPD2
void eTitle() {
  display.firstPage();
  do {
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print(" Weather Station v4.0 ");
    display.setCursor(60, 180);
    display.print("Debasish Dutta");
    display.setCursor(60, 210);
    display.println("James Hughes 2023");
    display.setCursor(60, 230);
    display.print("Ver: ");
    display.println(VERSION);
    esp_task_wdt_reset();
  } while (display.nextPage());
}
#else
// GxEPD
void eTitle(void) {

  // Set the text size and cursor position
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print(" Weather Station V4.0 ");

  display.setCursor(60, 180);
  display.print("Debasish Dutta");
  display.setCursor(60, 210);
  display.println("James Hughes 2023");
  display.setCursor(60, 230);
  display.print("Ver: ");
  display.println(VERSION);

  // Update the display
  display.update();
  delay(2000);
}
#endif

void eUpdate(int count, int hardware, int sensor, int ignore, int upTimeSeconds) {
  int xStart, yStart;
  //int x, xOffset;

  display.setCursor(5, 281);
  display.setTextSize(2);
  display.print(count);
  display.print(" ");
  display.print(sensor);
  display.print(" ");
  display.print(hardware);
  display.print(" ");
  display.print(ignore);
  display.print(" ");
  display.print(upTimeSeconds);
}

void eSensors(void) {
  int xS, yS, y, yOffset;

  xS = 205;
  yS = 5;
  yOffset = 22;
  y = yS;

  display.setTextSize(2);
  display.drawRect(200, 0, 200, 280, GxEPD_BLACK);
  display.setCursor(xS, yS);
  display.print("Sensors:");
  //display.update();

  y += yOffset;
#ifdef IMPERIAL // Display in US
  // Display temperature in Fahrenheit
  float tempF = (environment.temperatureC * 9.0 / 5.0) + 32;
  display.setCursor(xS, y);
  display.print("Temp:");
  display.print(tempF);
  display.print("F");
#else // display in Metric
  display.setCursor(xS, y);
  display.print("Temp:");
  display.print(environment.temperatureC);
  display.print("C");
#endif

  y += yOffset;
  display.setCursor(xS, y);
  display.print("Rel Hum:");
  display.print(environment.humidity);
  display.print("%");

  y += yOffset;
#ifdef IMPERIAL
  // Convert mbar to US inHg
  float pressureInHg = (environment.barometricPressure / 100) * 0.02953;
  display.setCursor(xS, y);
  display.print("inHg:");
  display.print(pressureInHg, 2); // Display with 2 decimal places
#else
  display.setCursor(xS, y);
  display.print("mbar:");
  display.print(environment.barometricPressure / 100);
#endif

  y += yOffset;
  display.setCursor(xS, y);
  display.print("Wind:");

  y += yOffset;
  display.setCursor(xS, y);
  display.print(" Direction:");
  // display.print(environment.windDirectionADC);
  display.print(wind.cardinalDirection);
  
  y += yOffset;
  display.setCursor(xS, y);
  display.print(" Speed:");
  // display.print(environment.windSpeedMax);
  display.print(environment.windSpeed);

  y += yOffset;
  display.setCursor(xS, y);
  display.print("Rain:");

  y += yOffset;
  display.setCursor(xS, y);
  display.print("  1h:");
  display.print(environment.rainTicks60m);

  y += yOffset;
  display.setCursor(xS, y);
  display.print(" 24h:");
  display.print(environment.rainTicks24h);

  y += yOffset;
  display.setCursor(xS, y);
  display.print("Lux:");
  display.print(environment.lux);
}

void eHardware(void) {
  int xS, yS, y, yOffset;

  xS = 5;
  yS = 5;
  yOffset = 22;
  y = yS;

  display.setTextSize(2);
  display.drawRect(0, 0, 200, 280, GxEPD_BLACK);
  display.setCursor(xS, yS);
  display.print("Station:");

  // y += yOffset;
  // display.setCursor(xS, y);
  // display.print("Solar:");
  // display.print(hardware.solarADC);

  y += yOffset;
  display.setCursor(xS, y);
  display.print(" Solar:");
  float vSolar = (float)hardware.solarADC / ADCSolar;
  display.print(vSolar);
  display.print("v");

  // y += yOffset;
  // display.setCursor(xS, y);
  // display.print("Battery:");
  // display.print(hardware.batteryADC);

  y += yOffset;
  display.setCursor(xS, y);
  display.print(" Battery:");
  float vBat = (float)hardware.batteryADC / ADCBattery;
  display.print(vBat);
  display.print("v");

  y += yOffset;
#ifdef IMPERIAL // Display in US
  // Display temperature in Fahrenheit
  float bmetempF = (hardware.BMEtemperature * 9.0 / 5.0) + 32;
  display.setCursor(xS, y);
  display.print(" Temp:");
  display.print(bmetempF);
  display.print("F");
#else // display in Metric
  display.setCursor(xS, y);
  display.print(" Temp:");
  display.print(hardware.BMEtemperature);
  display.print("C");
#endif

  y += yOffset;
  display.setCursor(xS, y);
  display.print("");

  y += yOffset;
  display.setTextSize(2);
  display.drawRect(0, 0, 200, 280, GxEPD_BLACK);
  display.setCursor(xS, y);
  display.print("Receiver:");

  y += yOffset;
  display.setCursor(xS, y);
  display.print(" Battery:");
  int rawADC = analogRead(BATTERY_PIN);
  float voltage = (rawADC / ADC_MAX) * VOLTAGE_REF;
  float batteryVoltage = voltage / (R2 / (R1 + R2)); // Reverse voltage divider formula
  // float vBat = (float)hardware.batteryADC / ADCBattery;
  display.print(batteryVoltage);
  display.print("v");

  y += yOffset;
  display.setCursor(xS, y);
#ifdef RECEIVER_BME280
  if (receiverBMEok) {
    #ifdef IMPERIAL
      display.print(" Temp:");
      display.print((receiverTempC * 9.0 / 5.0) + 32.0, 1);
      display.print("F");
    #else
      display.print(" Temp:");
      display.print(receiverTempC, 1);
      display.print("C");
    #endif
  } else {
    display.print(" Temp:N/A");
  }
#else
  display.print(" Temp:--");
#endif

//   y += yOffset;
//   display.setCursor(xS, y);
// #ifdef RECEIVER_BME280
//   if (receiverBMEok) {
//     display.print(" Rel Hum:");
//     display.print(receiverHumidity);
//     display.print("%");
// #else
//   display.print(" Hum:--%");
// #endif

}
