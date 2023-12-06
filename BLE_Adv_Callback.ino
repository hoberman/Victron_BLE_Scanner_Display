// Some understanding of this stuff might be found by digging around in the ESP32 library's
// file BLEAdvertisedDevice.h

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {

      #define manDataSizeMax 31     // BLE specs say no more than 31 bytes, but see comments below!

      // See if we have manufacturer data and then look to see if it's coming from a Victron device.
      if (advertisedDevice.haveManufacturerData() == true) {

        // Here's the thing: BLE specs say our manufacturer data can be a max of 31 bytes.
        //
        // Note: these comments need to be reworked because of the way different versions of
        // the ESP32 libraries use String-vs-std::string.
        //
        // But: The library code puts this data into a String, which we will then copy to
        // a character (i.e., byte) buffer using String.toCharArray(). Assuming we have the
        // full 31 bytes of manufacturer data allowed by the BLE spec, we'll need to size our
        // buffer with an extra byte for a null terminator. Our toCharArray() call will need
        // to specify *32* bytes so it will copy 31 bytes of data with a null terminator
        // at the end.
        uint8_t manCharBuf[manDataSizeMax+1];

        #ifdef USE_String
          String manData = advertisedDevice.getManufacturerData();      // lib code returns String.
        #else
          std::string manData = advertisedDevice.getManufacturerData(); // lib code returns std::string
        #endif
        int manDataSize=manData.length(); // This does not include a null at the end.

        // Limit size just in case we get a malformed packet.
        if (manDataSize > manDataSizeMax) {
          Serial.printf("  Note: Truncating malformed %2d byte manufacturer data to max %d byte array size\n",manDataSize,manDataSizeMax);
          manDataSize=manDataSizeMax;
        }
        // Now copy the data from the String to a byte array. Must have the +1 so we
        // don't lose the last character to the null terminator.
        #ifdef USE_String
          manData.toCharArray((char *)manCharBuf,manDataSize+1);
        #else
          manData.copy((char *)manCharBuf, manDataSize + 1);
        #endif
        
        // Now let's use a struct to get to the data more cleanly.
        victronManufacturerData * vicData=(victronManufacturerData *)manCharBuf;

        // ignore this packet if the Vendor ID isn't Victron.
        if (vicData->vendorID!=0x02e1) {
          return;
        }

        // ignore this packet if it isn't type 0x01 (Solar Charger).
        if (vicData->victronRecordType != 0x01) {
          return;
        }

        // Get the MAC address of the device we're hearing, and then use that to look up the encryption key
        // for the device.
        //
        // We go through a bit of trouble here to turn the String MAC address that we get from the BLE
        // code ("08:00:2b:xx:xx:xx") into a byte array. I'm divided on this... I could have just (and still might!)
        // left this as a string and just done a strcmp() match. This would have saved me some coding and execution time
        // in exchange for having to format the MAC addresses in my solarControllers list using the embedded colons.
        char receivedMacStr[18];
        strcpy(receivedMacStr,advertisedDevice.getAddress().toString().c_str());

        byte receivedMacByte[6];
        hexCharStrToByteArray(receivedMacStr,receivedMacByte);

        int solarControllerIndex=-1;
        for (int trySolarControllerIndex=0; trySolarControllerIndex<knownSolarControllerCount; trySolarControllerIndex++) {
          bool matchedMac=true;
          for (int i=0; i<6; i++) {
            if (receivedMacByte[i] != solarControllers[trySolarControllerIndex].byteMacAddr[i]) {
              matchedMac=false;
              break;
            }
          }
          if (matchedMac) {
            solarControllerIndex=trySolarControllerIndex;
            break;
          }
        }

        // Get the device name (if there's one in this packet).
        char deviceName[32]; // 31 characters + \0
        strcpy(deviceName,"(unknown device name)");
        bool deviceNameFound=false;
        if (advertisedDevice.haveName()) {
          // This works the same whether getName() returns String or std::string.
          strcpy(deviceName,advertisedDevice.getName().c_str());
          
          // This is prone to breaking because it's not very sophisticated. It's meant to
          // strip off "SmartSolar" if it's at the beginning of the name, but will do
          // ugly things if someone has put it elsewhere like "My SmartSolar Charger".
          if (strstr(deviceName,"SmartSolar ") == deviceName) {
            strcpy(deviceName,deviceName+11);
          }
          deviceNameFound=true;
        }

        // We didn't do this test earlier because we want to print out a name - if we got one.
        if (solarControllerIndex == -1) {
          Serial.printf("Discarding packet from unconfigured Victron SmartSolar %s at MAC %s\n",deviceName,receivedMacStr);
          delay(5000);
          return;
        }

        // If we found a device name, cache it for later display.
        if (deviceNameFound) {
          strcpy(solarControllers[solarControllerIndex].cachedDeviceName,deviceName);
        }

        // The manufacturer data from Victron contains a byte that's supposed to match the first byte
        // of the device's encryption key. If they don't match, when we don't have the right key for
        // this device and we just have to throw the data away. ALTERNATELY, we can go ahead and decrypt
        // the data - incorrectly - and use the crazy values to indicate that we have a problem.
        if (vicData->encryptKeyMatch != solarControllers[solarControllerIndex].byteKey[0]) {
          Serial.printf("Encryption key mismatch for %s at MAC %s\n",
            solarControllers[solarControllerIndex].cachedDeviceName,receivedMacStr);
          return;
        }

        // Get the beacon's RSSI (signal strength). If it's stronger than other beacons we've received,
        // then lock on to this SmartSolar and don't display beacons from others anymore.
        int RSSI=advertisedDevice.getRSSI();
        if (selectedSolarControllerIndex==solarControllerIndex) {
          if (RSSI > bestRSSI) {
            bestRSSI=RSSI;
          }
        } else {
          if (RSSI > bestRSSI) {
            selectedSolarControllerIndex=solarControllerIndex;
            Serial.printf("Selected Victon SmartSolar %s at MAC %s as preferred device based on RSSI %d\n",
              solarControllers[solarControllerIndex].cachedDeviceName,receivedMacStr,RSSI);
          } else {
            Serial.printf("Discarding RSSI-based non-selected Victon SmartSolar %s at MAC %s\n",
              solarControllers[solarControllerIndex].cachedDeviceName,receivedMacStr);
            return;
          }
        }

        // Now that the packet received has met all the criteria for being displayed,
        // let's decrypt and decode the manufacturer data.
        
        byte inputData[16];
        byte outputData[16]={0};
        victronPanelData * victronData = (victronPanelData *) outputData;

        // The number of encrypted bytes is given by the number of bytes in the manufacturer
        // data as a while minus the number of bytes (10) in the header part of the data.
        int encrDataSize=manDataSize-10;
        for (int i=0; i<encrDataSize; i++) {
          inputData[i]=vicData->victronEncryptedData[i];   // copy for our decrypt below while I figure this out.
        }

        esp_aes_context ctx;
        esp_aes_init(&ctx);

        auto status = esp_aes_setkey(&ctx, solarControllers[solarControllerIndex].byteKey, AES_KEY_BITS);
        if (status != 0) {
          Serial.printf("  Error during esp_aes_setkey operation (%i).\n",status);
          esp_aes_free(&ctx);
          return;
        }
        
        byte data_counter_lsb=(vicData->nonceDataCounter) & 0xff;
        byte data_counter_msb=((vicData->nonceDataCounter) >> 8) & 0xff;
        u_int8_t nonce_counter[16] = {data_counter_lsb, data_counter_msb, 0};
        u_int8_t stream_block[16] = {0};

        size_t nonce_offset=0;
        status = esp_aes_crypt_ctr(&ctx, encrDataSize, &nonce_offset, nonce_counter, stream_block, inputData, outputData);
        if (status != 0) {
          Serial.printf("Error during esp_aes_crypt_ctr operation (%i).",status);
          esp_aes_free(&ctx);
          return;
        }
        esp_aes_free(&ctx);

        byte deviceState=victronData->deviceState;  // this is really more like "Charger State"
        byte errorCode=victronData->errorCode;
        float batteryVoltage=float(victronData->batteryVoltage)*0.01;
        float batteryCurrent=float(victronData->batteryCurrent)*0.1;
        float todayYield=float(victronData->todayYield)*0.01*1000;
        float inputPower=float(victronData->inputPower)*0.01;

        // Getting the output current takes some magic.
        int integerOutputCurrent=((victronData->outputCurrentHi & 0x01)<<9) | victronData->outputCurrentLo;
        float outputCurrent=float(integerOutputCurrent)*0.1;

        // I don't know why, but every so often we'll get half-corrupted data from the Victron.
        // Towards the goal of filtering out this noise, I've found that I've rarely (if ever) seen
        // corrupted data when the 'unused' bits of the outputCurrent MSB equal 0xfe. We'll use this
        // as a litmus test here.
        byte unusedBits=victronData->outputCurrentHi & 0xfe;
        if (unusedBits != 0xfe) {
          return;
        }

        // The Victron docs say Device State but it's really a Charger State.
        char chargeStateName[6];
        sprintf(chargeStateName,"%4d?",deviceState);
        if (deviceState >=0 && deviceState<=7) {
          strcpy(chargeStateName,chargeStateNames[deviceState]);
        }

        #if defined M5STICKC || defined M5STICKCPLUS
          int chargeStateColor=COLOR_UNKNOWN;
          if (deviceState >=0 && deviceState<=7) {
            chargeStateColor=chargeStateColors[deviceState];
          }
        #endif

        Serial.printf("%-31s  Battery: %6.2f Volts %6.2f Amps  Solar: %6.2f Watts  Yield: %4.0f Wh  Load: %5.1f Amps  Charger: %-13s Err: %2d RSSI: %d\n",
          solarControllers[solarControllerIndex].cachedDeviceName,
          batteryVoltage, batteryCurrent,
          inputPower, todayYield,
          outputCurrent, chargeStateName, errorCode, RSSI
        );

        #if defined M5STICKC || defined M5STICKCPLUS
          display.fillScreen(COLOR_BACKGROUND);
          display.setCursor(0, 0);

          // We don't want to overflow our screen's display width (in this cast 13 characters) so
          // we'll limit the size of what we display. Sorry about that.
          char screenDeviceName[14]; // 13 characters plus /0
          strncpy(screenDeviceName,solarControllers[solarControllerIndex].cachedDeviceName,13);
          screenDeviceName[13]='\0'; // make sure we have a null byte at the end.
          display.printf("%s\n",screenDeviceName);

          // Special handling for this line of output: if our battery current is negative then print
          // the value in red.
          display.printf("%5.2fV",batteryVoltage);
          if (batteryCurrent<0.0) {
            display.setTextColor(COLOR_NEGATIVE, COLOR_BACKGROUND);
          }
          display.printf("%6.1fA\n",batteryCurrent);
          display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);

          display.printf("%5.2fW %4.0fWh\n",inputPower,todayYield);
          display.printf("Load: %6.1fA\n",outputCurrent);

          display.printf("Charge: ");
          // Special handling for chargeState coloring
          display.setTextColor(chargeStateColor, COLOR_BACKGROUND);
          display.printf("%s",chargeStateName);
          display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
        #endif
      }
    }
};
