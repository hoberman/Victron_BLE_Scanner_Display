void loop() {

  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory

  #if defined LED_PIN
    time_t timeNow=time(nullptr);
    if (timeNow != lastLEDBlinkTime) {
      digitalWrite(LED_PIN, LED_ON);
      delay(1);
      digitalWrite(LED_PIN, LED_OFF);
      lastLEDBlinkTime=timeNow;
    }
  #endif
  
  //while (true) { delay(1000); }
  //delay(10000);

  //Serial.println("loop");
  //delay(1000);
  
}
