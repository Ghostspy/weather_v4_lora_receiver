//mqtt data send



WiFiClient espClient;
PubSubClient client(espClient);


//=======================================================================
//  SendDataMQTT: send MQTT data to broker with 'retain' flag set to TRUE
//=======================================================================

void SendDataMQTT(struct sensorData environment) {
  char bufferTempF[5];
  char bufferTempC[5];
  char bufferRain[10];
  char bufferRain24[10];
  float temperatureF;
  float windSpeedMPH;
  float windSpeedMaxMPH;
  float inHg, mmHg;
  float vBatttery, vSolar;

  client.setServer(mqttServer, mqttPort);

  if (!client.connected()) {
    MonPrintf("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
    } else {
      Serial.printf("MQTT connect failed, state %d - skipping publish\n", client.state());
      return;
    }
  }
  temperatureF = environment.temperatureC * 9.0f / 5.0f + 32.0f;
  windSpeedMPH = environment.windSpeed * 1 / 1.609;
  windSpeedMaxMPH = environment.windSpeedMax / 1.609;
  // barometricPressure is in Pa (BME280 PresUnit_Pa). 1 Pa = 0.000295333 inHg.
  // OFFSET_IN/OFFSET_MM add altitude correction for sea-level pressure.
  inHg = environment.barometricPressure * 0.0002953 + OFFSET_IN;
  mmHg = environment.barometricPressure * 0.0002953 * 25.4 + OFFSET_MM;
  setWindDirection(environment.windDirectionADC);

  MQTTPublish("sensors/imperial/temperature/", (int)temperatureF, RETAIN);
  MQTTPublish("sensors/imperial/windSpeed/", (float)windSpeedMPH, RETAIN);
  MQTTPublish("sensors/imperial/windSpeedMax/", (float)windSpeedMaxMPH, RETAIN);
  MQTTPublish("sensors/imperial/rainfall/rainfall24/", (float)(environment.rainTicks24h * 0.011), RETAIN);
  MQTTPublish("sensors/imperial/rainfall/rainfall60m/", (float)(environment.rainTicks60m * 0.011), RETAIN);
  MQTTPublish("sensors/imperial/pressure/", inHg, RETAIN);

  MQTTPublish("sensors/metric/windSpeed/", (float)environment.windSpeed, RETAIN);
  MQTTPublish("sensors/metric/windSpeedMax/", (float)environment.windSpeedMax, RETAIN);
  MQTTPublish("sensors/metric/temperature/", (int)environment.temperatureC, RETAIN);
  MQTTPublish("sensors/metric/rainfall/rainfall24/", (float)(environment.rainTicks24h * 0.011 * 25.4), RETAIN);
  MQTTPublish("sensors/metric/rainfall/rainfall60m/", (float)(environment.rainTicks60m * 0.011 * 25.4), RETAIN);
  MQTTPublish("sensors/metric/pressure/", mmHg, RETAIN);

  MQTTPublish("sensors/windDirection/", (float)wind.degrees, RETAIN);
  MQTTPublish("sensors/windCardinalDirection/", wind.cardinalDirection, RETAIN);


  MQTTPublish("sensors/lux/", environment.lux, RETAIN);
  MQTTPublish("sensors/UVIndex/", environment.UVIndex, RETAIN);
  MQTTPublish("sensors/relHum/", environment.humidity, RETAIN);
  MonPrintf("Issuing mqtt disconnect\n");
  client.disconnect();
  MonPrintf("Disconnected\n");
}

//=======================================================================
//  SendDataMQTT: send MQTT data to broker with 'retain' flag set to TRUE
//=======================================================================
void SendDataMQTT(struct diagnostics hardware) {
  char bufferTempF[5];
  char bufferTempC[5];
  char bufferRain[10];
  char bufferRain24[10];
  float vBattery, vSolar;


  client.setServer(mqttServer, mqttPort);

  if (!client.connected()) {
    MonPrintf("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
    } else {
      Serial.printf("MQTT connect failed, state %d - skipping publish\n", client.state());
      return;
    }
  }

  vSolar = (float)hardware.solarADC / ADCSolar;
  vBattery = (float)hardware.batteryADC / ADCBattery;

  MQTTPublish("hardware/boot/", (int)hardware.bootCount, RETAIN);
  MQTTPublish("hardware/rssi/", (int)rssi_lora, RETAIN);
  MQTTPublish("hardware/vBattery/", (float)vBattery, RETAIN);
  MQTTPublish("hardware/vSolar/", (float)vSolar, RETAIN);
  MQTTPublish("hardware/chargeStatusB/", (bool)hardware.chargeStatusB, RETAIN);
  MQTTPublish("hardware/caseTemperature/", hardware.BMEtemperature, RETAIN);
  MQTTPublish("hardware/ADCbattery/", (int)hardware.batteryADC, RETAIN);
  MQTTPublish("hardware/ADCsolar/", (int)hardware.solarADC, RETAIN);
  //MQTTPublish("ESPcoreF/", (int)hardware->coreF, RETAIN);
  MQTTPublish("hardware/ESPcoreC/", (int)hardware.coreC, RETAIN);
  //MQTTPublish("timeEnabled/", (int)elapsedTime, RETAIN);
  MQTTPublish("hardware/lowBattery/", false, RETAIN);  //TODO
  MonPrintf("Issuing mqtt disconnect\n");
  client.disconnect();
  MonPrintf("Disconnected\n");
}

//=======================================================================
//  MQTTPublishString: routine to publish string
//=======================================================================
void MQTTPublish(const char topic[], const char *value, bool retain) {
  char topicBuffer[256];
  char payload[256];

  strcpy(topicBuffer, mainTopic);
  strcat(topicBuffer, topic);
  if (!client.connected() && !reconnect()) return;
  client.loop();
  snprintf(payload, sizeof(payload), "%s", value);
  MQTTSend(topicBuffer, payload, retain);
}

//=======================================================================
//  MQTTPublishInt: routine to publish int values as strings
//=======================================================================
void MQTTPublish(const char topic[], int value, bool retain) {
  char topicBuffer[256];
  char payload[256];

  strcpy(topicBuffer, mainTopic);
  strcat(topicBuffer, topic);
  if (!client.connected() && !reconnect()) return;
  client.loop();
  snprintf(payload, sizeof(payload), "%i", value);
  MQTTSend(topicBuffer, payload, retain);
}


//=======================================================================
//  MQTTPublish Long: routine to publish int values as strings
//=======================================================================
void MQTTPublish(const char topic[], long value, bool retain) {
  char topicBuffer[256];
  char payload[256];

  strcpy(topicBuffer, mainTopic);
  strcat(topicBuffer, topic);
  if (!client.connected() && !reconnect()) return;
  client.loop();
  snprintf(payload, sizeof(payload), "%li", value);
  MQTTSend(topicBuffer, payload, retain);
}

//=======================================================================
//  MQTTPublishFloat: routine to publish float values as strings
//=======================================================================
void MQTTPublish(const char topic[], float value, bool retain) {
  char topicBuffer[256];
  char payload[256];

  strcpy(topicBuffer, mainTopic);
  strcat(topicBuffer, topic);
  if (!client.connected() && !reconnect()) return;
  client.loop();
  snprintf(payload, sizeof(payload), "%6.3f", value);
  MQTTSend(topicBuffer, payload, retain);
}

//=======================================================================
//  MQTTPublishBool: routine to publish bool values as strings
//=======================================================================
void MQTTPublish(const char topic[], bool value, bool retain) {
  char topicBuffer[256];
  char payload[256];

  strcpy(topicBuffer, mainTopic);
  strcat(topicBuffer, topic);
  if (!client.connected() && !reconnect()) return;
  client.loop();
  snprintf(payload, sizeof(payload), "%s", value ? "true" : "false");
  MQTTSend(topicBuffer, payload, retain);
}

//=======================================================================
//  reconnect: MQTT reconnect with credentials, limited retries
//=======================================================================
bool reconnect() {
  const int maxRetries = 3;
  for (int i = 0; i < maxRetries && !client.connected(); i++) {
    Serial.print("Attempting MQTT reconnect...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
      return true;
    }
    Serial.printf("failed rc=%d (%d/%d)\n", client.state(), i + 1, maxRetries);
    esp_task_wdt_reset();
    delay(2000);
    esp_task_wdt_reset();
  }
  return client.connected();
}

//=======================================================================
//  MQTTSend: MQTT send topic with value to broker
//=======================================================================
void MQTTSend(char *topicBuffer, char *payload, bool retain) {
  int status = 0;
  int retryCount = 0;
#ifdef ExtendedMQTT
  MonPrintf("%s: %s\n", topicBuffer, payload);
#endif
  while (!status && retryCount < 5) {
    status = client.publish(topicBuffer, payload, retain);
#ifdef ExtendedMQTT
    MonPrintf("MQTT status: %i\n", status);
#endif
    delay(50);
    retryCount++;
  }
}

#ifdef RECEIVER_BME280
//=======================================================================
//  SendReceiverBME280MQTT: publish receiver enclosure BME280 readings
//=======================================================================
void SendReceiverBME280MQTT() {
  client.setServer(mqttServer, mqttPort);

  if (!client.connected()) {
    MonPrintf("Connecting to MQTT (receiver BME280)...\n");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
    } else {
      Serial.printf("MQTT connect failed, state %d - skipping publish\n", client.state());
      return;
    }
  }

  float tempF  = (receiverTempC * 9.0 / 5.0) + 32.0;
  float inHg   = (receiverPressureHPa / 33.8639);

  MQTTPublish("receiver/temperature/C/",    receiverTempC,       RETAIN);
  MQTTPublish("receiver/temperature/F/",    tempF,               RETAIN);
  MQTTPublish("receiver/humidity/",         receiverHumidity,    RETAIN);
  MQTTPublish("receiver/pressure/hPa/",     receiverPressureHPa, RETAIN);
  MQTTPublish("receiver/pressure/inHg/",    inHg,                RETAIN);

  client.disconnect();
}
#endif
