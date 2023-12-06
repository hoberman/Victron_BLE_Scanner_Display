void loop() {

  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

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
  
}
