
//=======================================================
//  setWindDirection: map ADC value to wind direction
//=======================================================
void setWindDirection(int DirectionADC) {
  // Thresholds are midpoints between adjacent SparkFun vane ADC values.
  // Calculated for 10kΩ pull-up to 3.3V, 12-bit ADC (4095 max).
  // ADC = 4095 * R_vane / (R_vane + 10000). Sorted ascending by ADC value.
  // Default (ADC >= 3665) = W (highest resistance, 120kΩ).
  static const int analogCompare[15] = { 300, 354, 439, 622, 858, 1064, 1387, 1734, 2120, 2458, 2666, 2977, 3225, 3429, 3665 };
  static const char* windDirText[15]         = { "112.5", "67.5", "90", "157.5", "135", "202.5", "180", "22.5", "45", "247.5", "225", "337.5", "0", "292.5", "315" };
  static const char* windDirCardinalText[15] = { "ESE",   "ENE",  "E",  "SSE",   "SE",  "SSW",   "S",   "NNE",  "NE", "WSW",   "SW",  "NNW",   "N",  "WNW",  "NW"  };

  const char* windDirection = "270";
  const char* windCardinalDirection = "W";

  for (int windPosition = 0; windPosition < 15; windPosition++) {
    if (DirectionADC < analogCompare[windPosition]) {
      windDirection = windDirText[windPosition];
      windCardinalDirection = windDirCardinalText[windPosition];
      break;
    }
  }

  MonPrintf("Analog value: %i Wind direction: %s  \n", DirectionADC, windDirection);
  wind.degrees = atof(windDirection);
  strncpy(wind.cardinalDirection, windCardinalDirection, sizeof(wind.cardinalDirection) - 1);
  wind.cardinalDirection[sizeof(wind.cardinalDirection) - 1] = '\0';
}
