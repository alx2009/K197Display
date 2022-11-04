/**************************************************************************/
/*!
  @file     K197device.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the k197device class, see K197device.h for the class
  definition

  This class is responsible to store and decode the raw information received
  through the base class SPIdevice, and make it available to the rest of the
  sketch

*/
/**************************************************************************/
#include "K197device.h"
#include <Arduino.h>
#include <stdlib.h> // atof()
#include <string.h> // strstr()

#include "dxUtil.h"

#include "debugUtil.h"

// Lookup table to convert from segments to char
// Note: before using the table, shift the 5 most significant bit one position
// to the right (removes DP bit)
static char seg2char[128] = {
    ' ',  '\'', 'i', 'I', '^', '*', '*', 'T',
    '-',  '*',  'r', 'f', '*', '*', '*', 'F', // row 0 (0x00-0x0f)
    '_',  '*',  'e', 'L', '*', '*', '*', 'C',
    '=',  '*',  'c', 't', 'X', '*', 'g', 'E', // row 1 (0x10-0x1f)
    '\'', '"',  '*', '*', '?', '*', '*', '*',
    '*',  '*',  '/', '*', '*', '*', '*', 'P', // row 2 (0x20-0x2f)
    '*',  '*',  '*', '*', '*', 'M', '*', '*',
    '*',  'Y',  '*', '@', '*', 'Q', '2', 'R', // row 3 (0x30-0x3f)
    'i',  '*',  '*', '*', '*', '*', '*', '*',
    '*',  '\\', 'n', 'h', '*', '*', '*', 'K', // row 4 (0x40-0x4f)
    'j',  '*',  'u', '*', ' ', '*', 'W', 'G',
    'a',  '*',  'o', 'b', '*', '5', '*', '6', // row 5 (0x50-0x5f)
    '1',  '*',  '*', '*', '7', '7', '7', 'N',
    '*',  '4',  '*', 'H', '*', '9', '*', 'A', // row 6 (0x60-0x6f)
    'J',  'V',  'J', 'U', 'D', '*', '*', '0',
    '*',  'y',  'd', '&', '3', '9', 'a', '8' // row 7 (0x70-0x7f)
};

/*!
      @brief utility function, return the value of a message up to len
   characters

      this function can only be used within K197device.cpp

      @param s a char array with the message (does not need to be null
   terminated)
      @param len the number of characters in s to consider

      @return the value using atof(), or 0 if s includes only space characters

*/
float getMsgValue(char *s, int len) {
  for (int i = 0; i < len; i++) {
    if (*s != ' ') {
      return atof(s);
    }
    s++;
  }
  return 0.0; // we consider a string of spaces as equivalent to 0.0. TODO: may
              // want to return NaN and have the caller handle that
}

/*!
      @brief process a new reading that has just been received via SPI

      A new reading is not processed automatically to make sure no value can be
   overriden while in use (especially when interrupts are used). Instead,
   SPIdevice::hasNewData() should be called periodically, when it returns true
   getNewReading() can be called to process the new information. The information
   will be available until the next cycle.

      Note that internally this function calls SPIdevice::getNewData(), so
   afterwards SPIdevice::hasNewData() will return false until a new batch of
   data is received via SPI.

      @return true if the data was received correctly, false otherwise

*/
bool K197device::getNewReading() {
  byte spiData[PACKET];
  byte n = getNewReading(spiData);
  return n == 9 ? true : false;
}

/*!
      @brief process a new reading that has just been received via SPI and
   return a copy of the SPI data buffer

      Similar to getNewReading() except it also returns a copy of the SPI data
   bufer. The only reason to use this function is to help in troubleshooting,
      e.g. to print the SPI data buffer to Serial. For normal purposes it is
   enough to call getNewReading() and access the decoded information via the
   other member functions

      If a number != 9 is returned, indicates that the data was NOT correctly
   transmitted

      @param data byte array that will receive the copy of the data.  MUST have
   room for at least 9 elements!
      @return the number of bytes copied into data.
*/
byte K197device::getNewReading(byte *data) {
  byte n = getNewData(data);
  if (n != 9) {
    DebugOut.print(F("Warning, K197 n="));
    DebugOut.println(n);
  }
  if (n > 0)
    annunciators0 = data[0];
  else
    annunciators0 = 0x00;
  if (n > 7)
    annunciators7 = data[7];
  else
    annunciators7 = 0x00;
  if (n > 8)
    annunciators8 = data[8];
  else
    annunciators8 = 0x00;

  for (int i = 0; i < K197_MSG_SIZE; i++)
    message[i] = 0;
  for (int i = 0; i < K197_RAW_MSG_SIZE - 1; i++)
    raw_msg[0] = ' ';
  raw_msg[K197_RAW_MSG_SIZE - 1] = 0;
  int nchar = 0;
  if (n > 0) {
    if ((data[0] & K197_MINUS_bm) >
        0) { // TODO: define inline functions to check for annunciators
      raw_msg[0] = '-';
      message[nchar] = '-';
      nchar++;
    }
  }
  int msg_n = n >= 7 ? 7 : n;
  byte num_dp = 0;
  msg_is_num = true; // assumed true until proven otherwise
  raw_dp = 0x00;
  for (int i = 1; i < msg_n; i++) { // skip i=0 is done on purpose
    if (hasDecimalPoint(data[i])) {
      bitSet(raw_dp, i);
      num_dp++;
      if (num_dp == 1) { // We consider only a single decimal point
        message[nchar] = '.';
        nchar++;
      } else {
        DebugOut.println(F("Warning, K197 dupl. DP"));
      }
    }
    int seg128 = ((data[i] & 0b11111000) >> 1) |
                 (data[i] & 0b00000011); // remove the DP bit and shift right to
                                         // convert to a 7 bits number
    raw_msg[i] = seg2char[seg128];
    message[nchar] = seg2char[seg128]; // lookup the character corresponding to
                                       // the segment combination
    if (!isDigitOrSpace(message[nchar]))
      msg_is_num = false;
    nchar++;
  }

  if (msg_is_num) {
    msg_value = getMsgValue(message, K197_MSG_SIZE);
    msg_is_ovrange = false;
  } else {
    if (strstr(message, "0L") != NULL) {
      msg_is_ovrange = true;
    } else {
      msg_is_ovrange = false;
    }
    msg_value = 0.0;
  }
  if (isTKModeActive() && msg_is_num) {
      tkConvertV2C();  
  }
  return n;
}

/*!
    @brief  convert a Voltage reading to a Temperature reading in Celsius
    @details Assumes the current value is V or mV coming from a K type thermocouple. Cold junction compensation uses the AVR internal temperature sensor and no linear compensation. The result of the conversion replaces the current measurement result
*/
void K197device::tkConvertV2C() {
    if (isOvrange() || (!isTKModeActive()) ) return;
    tcold = dxUtil.getTCelsius();
    float t = msg_value*24.2271538 + tcold; // msg_value = mV
    if (t>2200.0) { // Way more than needed...
         setOverrange();
         return;
    }
    msg_value=t;
    dtostrf(t, K197_MSG_SIZE-1, 2, message);
    int j=0;
    for (int i=0; i<K197_MSG_SIZE; i++) {
        if(message[i]!='.') {
            raw_msg[j] =  message[i];
            j++;       
        }
        if (j>=K197_RAW_MSG_SIZE)
           break;
    }
    raw_msg[K197_RAW_MSG_SIZE-1]=0;
    raw_dp = 0x20;
}

/*!
    @brief  set the displayed message to Overrange
*/
void K197device::setOverrange() {
    msg_is_ovrange = true;
    raw_msg[0] = message[0] = '0';
    raw_msg[1] = message[1] = 'L';
    raw_msg[2] = message[2] = 0;
    
}

/*!
    @brief  return the unit text (V, mV, etc.)
    @return the unit (2 characters + terminating NUL). This is a UTF-8 string
   because it may include Ω or µ
*/
const __FlashStringHelper *K197device::getUnit(bool include_dB) { // Note: includes UTF-8 characters
  if (isV()) {            // Voltage units
    if (tkMode && ismV())
      return F("°C");
    else if (ismV())
      return F("mV");
    else
      return F(" V");
  } else if (isOmega()) { // Resistence units
    if (isM())
      return F("MΩ");
    else if (isk())
      return F("kΩ");
    else
      return F(" Ω");
  } else if (isA()) {     // Current units
    if (ismicro())
      return F("µA");
    else if (ismA())
      return F("mA");
    else
      return F(" A");
  } else {                // No unit found
    if (include_dB && isdB())
      return F("dB");
    else 
      return F("  ");
  }

}

/*!
    @brief  print a summary of the received message to DebugOut for
   troubleshooting purposes
*/
void K197device::debugPrint() {
  DebugOut.print(message);
  if (msg_is_num) {
    DebugOut.print(F(", ("));
    DebugOut.print(msg_value, 6);
    DebugOut.print(')');
  }
  if (msg_is_ovrange)
    DebugOut.print(F(" + Ov.Range"));
  DebugOut.println();
}

/*!
    @brief  data logging to Serial
    @details does the actual data logging when called
*/
void K197device::logData() {
    if (!msg_log) return;
    Serial.print(millis());Serial.print(F(" ms; "));
    Serial.print(getMessage());Serial.print(' ');
    Serial.print(getUnit(true));
    if (isAC()) Serial.print(F(" AC"));
    Serial.println(); 
}
