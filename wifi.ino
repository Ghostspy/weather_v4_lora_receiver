//=======================================================================
//  wifi_connect: connect to WiFi. Replys on WDT to reset unit if no connection exists.
//=======================================================================
long wifi_connect(void) {
  const unsigned long timeoutMs = 30000;
  unsigned long startMs = millis();
  long wifi_signal = 0;

  MonPrintf("Connecting to WiFi\n");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMs >= timeoutMs) {
      MonPrintf("WiFi connect timeout after 30s - continuing without WiFi\n");
      return 0;
    }
    MonPrintf("WiFi - attempting to connect\n");
    delay(500);
  }
  MonPrintf("WiFi connected\n");
  wifi_signal = WiFi.RSSI();
  return wifi_signal;
}