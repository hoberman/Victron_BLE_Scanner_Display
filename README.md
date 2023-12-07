# Victron_BLE_Scanner_Display
 
This software builds on the stripped-down Victron BLE Advertising example found at:

   https://github.com/hoberman/Victron_BLE_Advertising_example

If you haven't already managed to receive and decode the advertising data from a Victron SmartSolar charger, you should start with that much-simpler code and make sure you can get it to work.

This program extends the original example code in several ways. First, it can be configured with the AES keys for multiple SmartSolar devices and can receive, decrypt, and decode the advertising data from each of them. When used on a board with integrated graphics hardware, the software will automatically display the SmartSolar data from the one with the strongest signal. If the graphics hardware isn't enabled, then data from all configured SmartSolar devices is sent to the Serial interface as it is received.

Finally, to make decryption data easier to enter, this code allows you to simply paste each device's MAC address and AES key into quoted character array strings. A function called during setup() uses the character array data to construct the byte array entries needed by the decryption code.

Notes:

I'm new to Github, so don't be surprised if you see wonky things going on here while I play with it.

I'm a hobbyist, not an experienced software developer, so be kind where I've made mistakes or done things in a weird way.

Yes, I've included my Victron's encryption key in my source code. I understand that's a bad practice, but in this case I'm willing to accept the risk that you might drive by and be able to decode my SmartSolar data. Oh, the horror!

Be aware that Espressif appears to be changing the types of some of the BLE Advertising methods we use from std::string to String. You'll need to check your build system's BLEAdvertisedDevice.h file and comment/uncomment the USE_String #define accordingly.
