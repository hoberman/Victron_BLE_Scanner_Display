void loop() {

  BLEScanResults * foundDevices = pBLEScan->start(scanTime, false);

  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory

  // This is mostly for use with modules that don't have a display and also don't have
  // a power-is-on LED. I want some sort of indicator telling me it's powered on and
  // doing something.
  #if defined LED_PIN
    time_t timeNow=time(nullptr);
    if (timeNow != lastLEDBlinkTime) {
      digitalWrite(LED_PIN, LED_ON);
      delay(1);
      digitalWrite(LED_PIN, LED_OFF);
      lastLEDBlinkTime=timeNow;
    }
  #endif
  
  // for now I'll flip the display orientation using BUTTON_1. I really ought
  // to code this to use the accelerometer to sense the board's position...
  #if defined BUTTON_1
    if (digitalRead(BUTTON_1)==LOW) {
      while (digitalRead(BUTTON_1)==LOW) {
        delay(100);
      }

      if (displayRotation==3) {
        displayRotation=1;
      } else {
        displayRotation=3;
      }
      Serial.printf("Setting display rotation to %d\n",displayRotation);
      display.setRotation(displayRotation);
      lastTick=0;
      packetReceived=false;
    }
  #endif

  time_t timeNow=time(nullptr);
  if (!packetReceived && timeNow!=lastTick) {
    lastTick=timeNow;
    Serial.println("BLE listening...");
    #if defined M5STICKC || defined M5STICKCPLUS
      display.fillScreen(COLOR_BACKGROUND);
      display.setCursor(0, 0);

      display.printf("   Waiting   \n");
      display.printf("     for     \n");
      display.printf("   Victron   \n");
      display.printf("  SmartSolar \n");
    #endif
  }
}
