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

  Reference for math functions: https://www.nongnu.org/avr-libc/user-manual/group__avr__math.html

*/
/**************************************************************************/
#include "K197device.h"
#include <Arduino.h>
#include <stdlib.h> // atof()
#include <string.h> // strstr()

#include "dxUtil.h"

#include "debugUtil.h"
#include "pinout.h"

K197device k197dev;

/*!
    @brief Lookup table to convert from segments to char
    @details before using the table, shift the 5 most significant bit one
   position to the right (removes DP bit)
*/
const char seg2char[128] PROGMEM = {
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
      @brief utility function, return the value of a string
      @details return the value of a string skipping initial spaces

      this function can only be used within K197device.cpp

      @param s a null terminated char array
      @param len the number of characters in s to consider

      @return the value using atof(), or 0 if s includes only space characters

*/
float getMsgValue(char *s, int len) {
  for (int i = 0; i < len; i++) {
    if (*s != CH_SPACE) {
      return atof(s);
    }
    s++;
  }
  return 0.0; // we consider a string of spaces as equivalent to 0.0
}

/*!
      @brief process a new reading

      @details process a new reading that has just been received via SPI.
      A new reading is not processed automatically to make sure no value can be
      overriden while in use (especially when interrupts are used). Instead,
      SPIdevice::hasNewData() should be called periodically, when it returns
   true getNewReading() can be called to process the new information. The
   information will be available until the next cycle.

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

#define K197_MSG_SIZE (K197_RAW_MSG_SIZE + 1) ///< add '.'

/*!
      @brief process a new reading

   @details process a new reading that has just been received via SPI and return
   a copy of the SPI data buffer

      Similar to getNewReading() except it also returns a copy of the SPI data
   bufer. The only reason to use this function is to help in troubleshooting,
      e.g. to print the SPI data buffer to Serial. For normal purposes it is
   enough to call getNewReading() and access the decoded information via the
   other member functions

   In addition to decoding the data, this function implements additional modes
   that have been enabled (for example conversion from mV to C for K type
   thermocouple temperature) and calculates statistics

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

  char message[K197_MSG_SIZE];
  for (int i = 0; i < K197_MSG_SIZE; i++)
    message[i] = 0;
  for (int i = 0; i < (K197_RAW_MSG_SIZE - 1); i++)
    raw_msg[0] = CH_SPACE;
  raw_msg[K197_RAW_MSG_SIZE - 1] = 0;
  int nchar = 0;
  if (n > 0 && isMINUS()) {
    raw_msg[0] = '-';
    message[nchar] = '-';
    nchar++;
  }
  int msg_n = n >= 7 ? 7 : n;
  byte num_dp = 0;
  flags.msg_is_num = true; // assumed true until proven otherwise
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
    char c =
        pgm_read_byte(&seg2char[seg128]); // lookup the character corresponding
                                          // to the segment combination
    raw_msg[i] = c;
    message[nchar] = c;
    if (!isDigitOrSpace(raw_msg[i]))
      flags.msg_is_num = false;
    nchar++;
  }
  if (flags.msg_is_num) {
    msg_value = getMsgValue(message, K197_MSG_SIZE);
    flags.msg_is_ovrange = false;
  } else {
    // if (strstr(message, "0L") != NULL) {/
    if (strcasecmp_P(message, PSTR("0L")) == 0) {
      flags.msg_is_ovrange = true;
    } else {
      flags.msg_is_ovrange = false;
    }
    msg_value = 0.0;
    if (strncmp_P(message, PSTR(" CAL"), 4) == 0) {
      annunciators8 |= K197_Cal_bm;
      DebugOut.println(F(" CAL found!"));
    }
  }
  if (isTKModeActive() && flags.msg_is_num) {
    tkConvertV2C();
  }
  updateCache();
  return n;
}

/*!
    @brief  convert a Voltage reading to a Temperature reading in Celsius
    @details Assumes the current value is V or mV coming from a K type
   thermocouple. Cold junction compensation uses the AVR internal temperature
   sensor and no linear compensation. The result of the conversion replaces the
   current measurement result
*/
void K197device::tkConvertV2C() {
  if (isOvrange() || (!isTKModeActive()))
    return;
  tcold = dxUtil.getTCelsius();
  float t = msg_value * 24.2271538 + tcold; // msg_value = mV
  if (t > 2200.0) {                         // Way more than needed...
    setOverrange();
    raw_dp = 0x00;
    return;
  }
  msg_value = t;
  char message[K197_MSG_SIZE];
  dtostrf(t, K197_MSG_SIZE - 1, 2, message);
  int j = 0;
  for (int i = 0; i < K197_MSG_SIZE; i++) {
    if (message[i] != '.') {
      raw_msg[j] = message[i];
      j++;
    }
    if (j >= K197_RAW_MSG_SIZE)
      break;
  }
  raw_msg[K197_RAW_MSG_SIZE - 1] = 0;
  raw_dp = 0x20;
}

/*!
    @brief  set the displayed message to Overrange
*/
void K197device::setOverrange() {
  flags.msg_is_ovrange = true;
  raw_msg[0] = CH_SPACE;
  raw_msg[1] = CH_SPACE;
  raw_msg[2] = CH_SPACE;
  raw_msg[3] = '0';
  raw_msg[4] = 'L';
  raw_msg[5] = CH_SPACE;
  raw_msg[6] = CH_SPACE;
  raw_msg[7] = 0;
  dxUtil.checkFreeStack();
}

/*!
    @brief  return the unit text (V, mV, etc.)
    @param include_dB if true, returns "dB" as a unit when in dB mode
    @return the unit (2 characters + terminating NUL). This is a UTF-8 string
   because it may include Ω or µ
*/
const __FlashStringHelper *
K197device::getUnit(bool include_dB) { // Note: includes UTF-8 characters
  if (isV()) {                         // Voltage units
    if (flags.tkMode && ismV() && isDC())
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
  } else if (isA()) { // Current units
    if (ismicro())
      return F("µA");
    else if (ismA())
      return F("mA");
    else
      return F(" A");
  } else { // No unit found
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
  DebugOut.print(raw_msg);
  if (flags.msg_is_num) {
    DebugOut.print(F(", ("));
    DebugOut.print(msg_value, 6);
    DebugOut.print(')');
  } else {
    for (int i = 0; i < K197_RAW_MSG_SIZE; i++) {
      DebugOut.print(F(" 0x"));
      if (raw_msg[i] < 0x10)
        DebugOut.print('0');
      DebugOut.print(raw_msg[i], HEX);
    }
    DebugOut.println();
  }
  if (flags.msg_is_ovrange)
    DebugOut.print(F(" + Ov.Range"));
  DebugOut.println();
}

/*!
    @brief  utility function, used to compare two set of annunciator0, ignoring
   the minus sign in the comparison
    @param b1 first annunciator0 set
    @param b2 second annunciator0 set
    @details average, max and min are calculated here
 */
static inline bool change0(byte b1, byte b2) {
  return (b1 & (~K197_MINUS_bm)) != (b2 & (~K197_MINUS_bm));
}

/*!
    @brief  update the cache
    @details average, max and min are calculated here
 */
void K197device::updateCache() {
  if (!isNumeric()) return; // No point updating statistics now
  if (cache.tkMode != flags.tkMode ||
      change0(cache.annunciators0, annunciators0) ||
      cache.annunciators7 != annunciators7 ||
      cache.annunciators8 != annunciators8) { // Something changed, reset stats
    resetStatistics();
  } else {
    cache.average += (msg_value - cache.average) * 
                     cache.avg_factor; // This not perfect but good enough
                                 // in most practical cases.
    if (msg_value < cache.min)
      cache.min = msg_value;
    if (msg_value > cache.max)
      cache.max = msg_value;
  }
  cache.msg_value = msg_value;
  cache.tkMode = flags.tkMode;
  cache.annunciators0 = annunciators0;
  cache.annunciators7 = annunciators7;
  cache.annunciators8 = annunciators8;
  cache.add2graph(msg_value);
  dxUtil.checkFreeStack();
}

static float getScaleMultiplierUp(float x, float pow10) { // Look for the maximum
  float norm=x * powf(10.0, -pow10); // normalized: -1<norm<1
  if ( norm > 0) {
     if (norm < 0.2) return 0.2;
     else if (norm < 0.5) return 0.5;
     return 1.0;
  } else {
     if (norm < -0.5) return -0.5;
     else if (norm < -0.2) return -0.2;
     return -0.1;
  }
}

static float getScaleMultiplierDown(float x, float pow10) { // Look for the minimum
  float norm=x * powf(10.0, -pow10); // normalized: 1>=abs(norm)>=1
  if ( norm > 0) {
     if (norm > 0.5) return 0.5;
     else if (norm > 0.2) return 0.2;
     return 0.1;
  } else {
     if (norm > -0.2) return -0.2;
     else if (norm > -0.5) return -0.5;
     return -1.0;
  }
}

#define SCALE_VALUE_MIN 0.0000001
#define SCALE_LOG_MIN -7
static float getPow10(float x) {
    x=fabsf(x);
    if (x<SCALE_VALUE_MIN) return SCALE_LOG_MIN;
    return ceilf(log10f(x));
}

void K197device::troubleshootAutoscale(float testmin, float testmax) {
  // Autoscale -  First we find the power of 10
  float max_pow10 = getPow10(testmin);  // First we bracket max ...
  float max_mult = getScaleMultiplierUp(testmax, max_pow10);
  // Then we fine tune the multiplier (1.0x, 0.5x or 0.2x)
  float ymax = max_mult*powf(10.0, max_pow10);
  // Print result
  DebugOut.print(F("testmax="));DebugOut.println(testmax);
  DebugOut.print(F("MAX 10^")); DebugOut.print(max_pow10); DebugOut.print(F("*")); DebugOut.print(max_mult); 
  DebugOut.print(F("=")); DebugOut.println(ymax); 

  // Autoscale -  First we find the power of 10
  float min_pow10 = getPow10(testmax); // and min with pow. of 10
  float min_mult = getScaleMultiplierDown(testmin, min_pow10);
  // Then we fine tune the multiplier (1.0x, 0.5x or 0.2x)
  float ymin = min_mult*powf(10.0, min_pow10);
  // Print result
  DebugOut.print(F("testmin="));DebugOut.println(testmin);
  DebugOut.print(F("MIN 10^")); DebugOut.print(min_pow10); DebugOut.print(F("*")); DebugOut.print(min_mult); 
  DebugOut.print(F("=")); DebugOut.println(ymin); 
}

void K197device::fillGraphDisplayData(k197graph_type *graphdata) {
  // Autoscale -  First we find the power of 10
  float max_pow10 = getPow10(cache.max);  
  float min_pow10 = getPow10(cache.min);
  // Then we fine tune the multiplier (1.0x, 0.5x, 0.2x or 0.1x, sign can be + or -)
  float max_mult = getScaleMultiplierUp(cache.max, max_pow10);   
  float min_mult = getScaleMultiplierDown(cache.min, min_pow10); 

  float ymax = max_mult*powf(10.0, max_pow10);
  float ymin = min_mult*powf(10.0, min_pow10);
  
  if (ymax == ymin) { //Safeguard from pathological cases...
     if (ymax>0) {
        ymax *=10.0;
        ymin *=0.1;
     } else if (ymax<0) {
        ymax *=0.1;
        ymin *=10.0;   
     } else { // ymax == ymin == 0.0
        ymax *= SCALE_VALUE_MIN;
        ymin *=-SCALE_VALUE_MIN;
     }
  }

  //DebugOut.print(F("cache.max="));DebugOut.println(cache.max);
  //DebugOut.print(F("MAX 10^")); DebugOut.print(max_pow10); DebugOut.print(F("*")); DebugOut.print(max_mult); 
  //DebugOut.print(F("=")); DebugOut.println(ymax); 
  //DebugOut.print(F("cache.in="));DebugOut.println(cache.min);
  //DebugOut.print(F("MIN 10^")); DebugOut.print(min_pow10); DebugOut.print(F("*")); DebugOut.print(min_mult); 
  //DebugOut.print(F("=")); DebugOut.println(ymin); 
  
  float scale_factor = float(graphdata->y_size)/round(ymax-ymin);
  for (int i=0; i<cache.gr_size; i++) {
     if (i>=graphdata->x_size) { // should be impossible but just to be safe
         DebugOut.print(F("i=")); DebugOut.print(i);
         DebugOut.print(F(", c=")); DebugOut.print(cache.gr_size);
         DebugOut.print(F(", g=")); DebugOut.print(graphdata->x_size);
         DebugOut.println(F("graph size!"));
         break; 
     }
     graphdata->point[i] = (cache.graph[i]-ymin)*scale_factor+0.5;
  }
  graphdata->current_idx = cache.gr_index==0 ? cache.gr_size: cache.gr_index; 
  if (graphdata->current_idx >0) graphdata->current_idx--;
  graphdata->npoints = cache.gr_size;
}

/*!
    @brief  reset all statistics (min, average, max)
    @details average, max and min are calculated here
 */
void K197device::resetStatistics() {
  cache.average = msg_value;
  cache.min = msg_value;
  cache.max = msg_value;
  cache.resetGraph();
}
