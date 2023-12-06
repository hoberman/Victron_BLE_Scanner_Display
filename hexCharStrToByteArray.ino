// Decryption keys and MAC addresses obtained from the VictronConnect app will be
// a string of hex digits like this:
//
//   f4116784732a
//   dc73cb155351cf950f9f3a958b5cd96f
//
// Split that up and turn it into an array whose equivalent definition would be like this:
//
//   byte key[]={ 0xdc, 0x73, 0xcb, ... 0xd9, 0x6f };
//
void hexCharStrToByteArray(char * hexCharStr, byte * byteArray) {
  bool returnVal=false;

  int hexCharStrLength=strlen(hexCharStr);

  // There are simpler ways of doing this without the fancy nibble-munching,
  // but I do it this way so I parse things like colon-separated MAC addresses.
  // BUT: be aware that this expects digits in pairs and byte values need to be
  // zero-filled. i.e., a MAC address like 8:0:2b:xx:xx:xx won't come out the way
  // you want it.
  int byteArrayIndex=0;
  bool oddByte=true;
  byte hiNibble;
  for (int i=0; i<hexCharStrLength; i++) {
    byte nibble=hexCharToByte(hexCharStr[i]);
    if (nibble!=255) {
      if (oddByte) {
        hiNibble=nibble;
      } else {
        byteArray[byteArrayIndex]=(hiNibble<<4) | nibble;
        byteArrayIndex++;
      }
      oddByte=!oddByte;
    }
  }
  // do we have a leftover nibble? I guess we'll assume it's a low nibble?
  if (! oddByte) {
    byteArray[byteArrayIndex]=hiNibble;
  }
}

byte hexCharToByte(char hexChar) {
  if (hexChar >= '0' && hexChar <='9') {          // 0-9
    hexChar=hexChar - '0';
  } else if (hexChar >= 'a' && hexChar <= 'f') {   // a-f
    hexChar=hexChar - 'a' + 10;
  } else if (hexChar >= 'A' && hexChar <= 'F') {  // A-F
    hexChar=hexChar - 'A' + 10;
  } else {
    hexChar=255;
  }

  return hexChar;
}
