void setup() {
  
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.println("Reset.");
  Serial.println();
  Serial.printf("Source file: %s\n",__FILE__);
  Serial.printf(" Build time: %s\n",__TIMESTAMP__);
  Serial.println();
  delay(1000);

  #if defined LED_PIN
    pinMode(LED_PIN,OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);
  #endif

  #if defined BUTTON_1
    pinMode(BUTTON_1, INPUT_PULLUP);
  #endif

  #if defined M5STICKC || defined M5STICKCPLUS
    M5.begin();

    display.init();
    display.setRotation(displayRotation);
    display.fillScreen(COLOR_BACKGROUND);

    display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    # if defined M5STICKC
      display.setTextSize(2);
    #else   // larger size text on M5StickCPlus
      display.setTextSize(3);
    #endif
    display.setTextFont(1);
    display.setCursor(0, 0);

    display.println("SmartSolar");
    display.println("BLE Scanner");
    display.println("Reset.");
  #endif


  delay(2000);

  Serial.printf("Controller count: %d\n", knownSolarControllerCount);

  #if defined M5STICKC || defined M5STICKCPLUS
    display.fillScreen(COLOR_BACKGROUND);
    display.setCursor(0, 0);

    display.println("Controllers");
    //display.println("Configured:");
    display.printf("Configured:%2d\n",knownSolarControllerCount);
  #endif

  for (int i = 0; i < knownSolarControllerCount; i++) {
    hexCharStrToByteArray(solarControllers[i].charMacAddr, solarControllers[i].byteMacAddr);
    hexCharStrToByteArray(solarControllers[i].charKey, solarControllers[i].byteKey);
    strcpy(solarControllers[i].cachedDeviceName, "(unknown)");
  }

  for (int i = 0; i < knownSolarControllerCount; i++) {
    Serial.printf("  %-16s", solarControllers[i].comment);
    Serial.printf("  Mac:   ");
    for (int j = 0; j < 6; j++) {
      Serial.printf(" %2.2x", solarControllers[i].byteMacAddr[j]);
    }

    Serial.printf("    Key: ");
    for (int j = 0; j < 16; j++) {
      Serial.printf("%2.2x", solarControllers[i].byteKey[j]);
    }
    Serial.println();
  }
  Serial.println();
  Serial.println();

  delay(2000);

  #if defined M5STICKC || defined M5STICKCPLUS
    display.fillScreen(COLOR_BACKGROUND);
    display.setCursor(0, 0);

    display.printf("Setup BLE\n",knownSolarControllerCount);
  #endif

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but gets results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value

  delay(2000);

  Serial.println("Done with setup()\n");

  #if defined M5STICKC || defined M5STICKCPLUS
    display.fillScreen(COLOR_BACKGROUND);
    display.setCursor(0, 0);

    display.printf("Ready\n");
  #endif

  delay(2000);
}
