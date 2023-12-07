/*
  Initial BLE code adapted from Examples->BLE->Beacon_Scanner.
  Victron decryption code snippets from:
  
    https://github.com/Fabian-Schmidt/esphome-victron_ble
  
  Information on the "extra manufacturer data" that we're picking up from Victron SmartSolar
  BLE advertising beacons can be found at:
  
    https://community.victronenergy.com/storage/attachments/48745-extra-manufacturer-data-2022-12-14.pdf
  
  Thanks, Victron, for providing both the beacon and the documentation on its contents!
*/

//
// This code receives, decrypts, and decodes Victron SmartSolar BLE beacons.
// The data will be output to the Serial device and shown on the built-in display
// of M5StickC and M5StickCPlus modules if the appropriate #define is uncommented.
//

// Tested with the following boards:
//    M5StickC              ESP32-PICO with integrated  80x160 ST7735S TFT display
//    M5StickCPlus          ESP32-PICO with integrated 135x240 ST7789v2 TFT display
// You can likely get this to work with other boards that use similar display hardware
// and/or display calls. I used to like TTGO boards - some of them have similar display hardware
// with the same display resolution as the M5StickCPlus, but they changed their
// USB-to-Serial chip to one that won't work with my Macbook. 

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <aes/esp_aes.h>  // AES decryption

// Uncomment only one.
///////////////////////////////////
#define M5STICKC
//#define M5STICKCPLUS
///////////////////////////////////

#if defined M5STICKC
  #include <M5StickC.h>
#endif

#if defined M5STICKCPLUS
  #include <M5StickCPlus.h>
#endif

#if defined M5STICKC || defined M5STICKCPLUS
  M5Display display;

  // Note: user-defined colors are:     5 bits (0x1F) Red    6 bits (0x3f) Green    5 bits (0x1f) Blue
  #define COLOR_BACKGROUND              TFT_BLACK
  #define COLOR_TEXT                    TFT_DARKGREEN
  #define COLOR_NEGATIVE                TFT_MAROON
  #define COLOR_UNKNOWN                 TFT_DARKGREY
  #define COLOR_CHARGEROFF              TFT_MAROON                          // maroon = dark red
  #define COLOR_BULK                    (0x00 << 11) + (0x00 << 5) + 0x18   // dark blue
  #define COLOR_ABSORPTION              (0x0f << 11) + (0x1f << 5) + 0x00   // dim yellow
  #define COLOR_FLOAT                   TFT_DARKGREEN
  #define COLOR_EQUALIZATION            (0x15 << 11) + (0x15 << 5) + 0x00   // dim orange

  // The M5Stick modules have an LED but I don't use it since we have that nice TFT display.
  // #define LED_PIN 10
  // #define LED_ON LOW     // weirdly enough, the on/off states for the M5Stick boards are the opposite
  // #define LED_OFF HIGH   // of every other board I've used.

  #define BUTTON_1 37

#endif

// The Espressif people decided to use String instead of std::string in some versions of
// their ESP32 libraries. Check the BLEAdvertisedDevice.h file to see if this is the case
// for getManufacturerData(); if so, then uncomment this line. Ugh.
//#define USE_String

BLEScan *pBLEScan;

#define AES_KEY_BITS 128
int scanTime = 1;  //In seconds


// Victron docs on the manufacturer data in advertisement packets can be found at:
//   https://community.victronenergy.com/storage/attachments/48745-extra-manufacturer-data-2022-12-14.pdf
//

// Usage/style note: I use uint16_t in places where I need to force 16-bit unsigned integers
// instead of whatever the compiler/architecture might decide to use. I might not need to do
// the same with byte variables, but I'll do it anyway just to be at least a little consistent.

// Must use the "packed" attribute to make sure the compiler doesn't add any padding to deal with
// word alignment.
typedef struct {
  uint8_t deviceState;
  uint8_t errorCode;
  int16_t batteryVoltage;
  int16_t batteryCurrent;
  uint16_t todayYield;
  uint16_t inputPower;
  uint8_t outputCurrentLo;  // Low 8 bits of output current (in 0.1 Amp increments)
  uint8_t outputCurrentHi;  // High 1 bit of ourput current (must mask off unused bits)
  uint8_t unused[4];
} __attribute__((packed)) victronPanelData;

typedef struct {
  uint16_t vendorID;                 // vendor ID
  uint8_t beaconType;                // Should be 0x10 (Product Advertisement) for the packets we want
  uint8_t unknownData1[3];           // Unknown data
  uint8_t victronRecordType;         // Should be 0x01 (Solar Charger) for the packets we want
  uint16_t nonceDataCounter;         // Nonce
  uint8_t encryptKeyMatch;           // Should match pre-shared encryption key byte 0
  uint8_t victronEncryptedData[21];  // (31 bytes max per BLE spec - size of previous elements)
  uint8_t nullPad;                   // extra byte because toCharArray() adds a \0 byte.
} __attribute__((packed)) victronManufacturerData;


typedef struct {
  char charMacAddr[13];       // 12 character MAC + \0 (initialized as quoted strings below for convenience)
  char charKey[33];           // 32 character keys + \0 (initialized as quoted strings below for convenience)
  char comment[16];           // 16 character comment (name) for printing during setup()
  byte byteMacAddr[6];        // 6 bytes for MAC - initialized by setup() from quoted strings
  byte byteKey[16];           // 16 bytes for encryption key - initialized by setup() from quoted strings
  char cachedDeviceName[32];  // 31 characters + \0 (filled in as we receive advertisements)
} solarController;

// Here I list the mac address, encryption key, and comment text (displayed only during initialization)
// for one or more Victron SmartSolar controllers. The code will receive beacons from the conmfigured
// controllers and will display the data from the one with the strongest signal. This is presumably
// the one that's physically closest to our ESP32 device. Once a device has been chosen as
// the one with the strongest signal it will remain selected unless it gets an even stronger signal from
// another.
//
// The mac address and encryption key for each known controller is configured as a quoted character string
// literal and then converted to an internal array of bytes during setup.
//
// The mac and encryption key for each controller is obtained from the VictronConnect app. See the extensive
// comments on this in my bare-minimum Victron BLE Advertising example:
//
//   https://github.com/hoberman/Victron_BLE_Advertising_example
//


// extra braces around each "designated initializer" element needed by some compiler versions.
solarController solarControllers[] = {
  { { .charMacAddr = "f4116784732a" }, { .charKey = "dc73cb155351cf950f9f3a958b5cd96f" }, { .comment = "Spare" } },
  { { .charMacAddr = "f944913298e8" }, { .charKey = "40ef2093aa678238147091c7657daa54" }, { .comment = "Gazebo" } },
  { { .charMacAddr = "cc5b284e8ae6" }, { .charKey = "2b6d51d4a74c3b83749303d87fa17bd9" }, { .comment = "Shack" } }
};
int  knownSolarControllerCount = sizeof(solarControllers) / sizeof(solarControllers[0]);

int bestRSSI = -200;
int selectedSolarControllerIndex = -1;

time_t lastLEDBlinkTime=0;
time_t lastTick=0;
int displayRotation=3;
bool packetReceived=false;

char chargeStateNames[][6] = {
  "  off",
  "   1?",
  "   2?",
  " bulk",
  "  abs",
  "float",
  "   6?",
  "equal"
};

#if defined M5STICKC || defined M5STICKCPLUS
  uint16_t chargeStateColors[] = {
    COLOR_CHARGEROFF,
    COLOR_UNKNOWN,
    COLOR_UNKNOWN,
    COLOR_BULK,
    COLOR_ABSORPTION,
    COLOR_FLOAT,
    COLOR_UNKNOWN,
    COLOR_EQUALIZATION
  };
#endif