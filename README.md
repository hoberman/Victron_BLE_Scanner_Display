# Victron_BLE_Scanner_Display
 
This software builds on the stripped-down Victron BLE Advertising example found at:

   https://github.com/hoberman/Victron_BLE_Advertising_example

If you haven't already managed to receive and decode the advertising data from a Victron SmartSolar charger, you should start with that much-simpler code and make sure you can get it to work.

This program extends the original example code in several ways. First, it can be configured with the AES keys for multiple SmartSolar devices and can receive, decrypt, and decode the advertising data from each of them. When used on a board with integrated graphics hardware, the software will automatically display the SmartSolar data from the one with the strongest signal. If the graphics hardware isn't enabled, then data from all configured SmartSolar devices is sent to the Serial interface as it is received.

Finally, to make decryption data easier to enter, this code allows you to simply paste each device's MAC address and AES key into quoted character array strings. A function called during setup() uses the character array data to construct the byte array entries needed by the decryption code.
