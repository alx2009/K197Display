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

void K197device::getNewReading() {
  byte spiData[PACKET];
  getNewReading(spiData);
}

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

const char *K197device::getUnit() { // Note: includes UTF-8 characters
  if (isV()) {
    if (ismV())
      return "mV";
    else
      return " V";
  } else if (isOmega()) {
    if (isM())
      return "MΩ";
    else if (isk())
      return "kΩ";
    else
      return " Ω";
  } else if (isA()) {
    if (ismicro())
      return "µA";
    else if (ismA())
      return "mA";
    else
      return " A";
  } else {
    return "  ";
  }
}

byte K197device::getNewReading(byte *data) {
  byte n = getNewData(data);
  if (n != 9) {
    Serial.print(F("Warning, K197 n="));
    Serial.println(n);
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
        Serial.println(F("Warning, K197 dupl. DP"));
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
  return n;
}

void K197device::debugPrint() {
  Serial.print(message);
  if (msg_is_num) {
    Serial.print(F(", ("));
    Serial.print(msg_value, 6);
    Serial.print(')');
  }
  if (msg_is_ovrange)
    Serial.print(F(" + Ov.Range"));
  Serial.println();
}
