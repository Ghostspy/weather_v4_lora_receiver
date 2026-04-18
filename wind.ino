
//=======================================================
//  setWindDirection: map ADC value to wind direction
//=======================================================
void setWindDirection(int DirectionADC) {
  static const int analogCompare[15] = { 150, 300, 450, 600, 830, 1100, 1500, 1700, 2250, 2350, 2700, 3000, 3200, 3400, 3800 };
  static const char* windDirText[15]         = { "157.5", "180", "247.5", "202.5", "225", "270", "292.5", "112.5", "135", "337.5", "315", "67.5", "90", "22.5", "45" };
  static const char* windDirCardinalText[15] = { "SSE", "S", "WSW", "SSW", "SW", "W", "WNW", "ESE", "SE", "NNW", "NW", "ENE", "E", "NNE", "NE" };

  const char* windDirection = "0";
  const char* windCardinalDirection = "N";

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
