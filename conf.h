//rename to config.h

#include "secrets.h"

#define SerialMonitor

#define SYNC 0x54
//===========================================
//LoRa band
//===========================================
#define BAND 915E6  //you can set band here directly,e.g. 868E6,915E6,433E6

#define WDT_TIMEOUT 30   //watchdog timer

#define DEVID 0x11223344

const char mainTopic[] = "PriorLake/";
#define RETAIN false

//===========================================
//Timezone information
//===========================================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -7 * 3600;
const int daylightOffset_sec = 3600;

//===========================================
//General defines
//===========================================
#define RSSI_INVALID -9999

//===========================================
//ADC calibration
//===========================================
#define ADCBattery 371 
#define ADCSolar 195

//===========================================
//Altitude offsets
//===========================================
#define OFFSET_IN 5.58 
#define OFFSET_MM 141.7

//===========================================
//James Hughes is using a Heltec_LoRa_v2 for receiver
//===========================================
//#define DEV_HELTEC_RECEIVER
#define E_PAPER

//===========================================
//Chad Bittner added to display US Temp and Pressure
//Also allows use of the Waveshare 4.2" Rev 2.2
//===========================================
#define IMPERIAL
#define WAVESHARE_R22

//============================================
//Chad Bittner added to display Recevier Battery Voltage
//============================================
#define BATTERY_PIN 34  // GPIO34 (D34)

//============================================
// Receiver BME280 (ambient/enclosure temp)
// I2C header on receiver PCB: SDA=GPIO21, SCL=GPIO22
// BME280 SDO pin: LOW=0x76, HIGH=0x77
//============================================
#define RECEIVER_BME280
#define BME280_SDA  21
#define BME280_SCL  22
#define BME280_ADDR 0x76

const float R1 = 100000.0;  // 100kΩ
const float R2 = 100000.0;  // 100kΩ
const float ADC_MAX = 4095.0;
const float VOLTAGE_REF = 3.65;  // ESP32 ADC reference voltage