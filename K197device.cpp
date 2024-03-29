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

  Reference for math functions:
  https://www.nongnu.org/avr-libc/user-manual/group__avr__math.html

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

// ***************************************************************************************
//  Segment to Character conversion and handling of display message
// ***************************************************************************************

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
    '*',  'y',  'd', '&', '3', '9', 'a', '8' //  row 7 (0x70-0x7f)
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

// ***************************************************************************************
//  Read data from the voltmeter
// ***************************************************************************************

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
  byte spiData[PACKET_DATA];
  byte n = getNewReading(spiData);
  return n == 9 ? true : false;
}

#define K197_MSG_SIZE (K197_RAW_MSG_SIZE + 1) ///< add 1 because of '.'

/*!
   @brief utility function, check if a message indicates overrange
   @details an overrange message is a six character message "  0L  ",
    optionally pre-pended by a "-" character
   @param message points to the message to check
   @return true if the meesage indicates overrange
*/
bool checkMessage4overrange(const char *message) {
  if (message[0] == '-')
    message++;
  if (strcasecmp_P(message, PSTR("  0L  ")) == 0)
    return true;
  return false;
}

/*!
      @brief process a new reading

   @details process a new reading that has just been received via SPI and return
   a copy of the SPI data buffer

      Similar to getNewReading() except it also returns a copy of the SPI data
   buffer. The only reason to use this function is to help in troubleshooting,
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
    DebugOut.print(F("!K197 n="));
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
        DebugOut.println(F("!K197 DP"));
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
    // DebugOut.print(F("message=<")); DebugOut.print(message);
    // DebugOut.println(F(">"));
    flags.msg_is_ovrange = checkMessage4overrange(message);
    msg_value = 0.0;
    if (strncmp_P(message, PSTR(" CAL"), 4) == 0) {
      annunciators8 |= K197_Cal_bm;
      // DebugOut.println(F(" CAL found!"));
    }
  }
  if (isTKModeActive() && flags.msg_is_num) {
    tkConvertV2C();
  }
  if (n == 9)
    updateCache(); // Avoid updating the cache if data was not read correctly
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
  CHECK_FREE_STACK();
}

// ***************************************************************************************
//  Handling of mesurement units (V, A, etc.)
// ***************************************************************************************

/*!
    @brief  return the unit, including SI prefix (V, mV, etc.)
    @param include_dB if true, returns "dB" as a unit when in dB mode
    @param hold if true returns the value at the time hold mode was last entered
    @return the unit (2 characters + terminating NUL). This is a UTF-8 string
   because it may include Ω or µ
*/
const __FlashStringHelper *
K197device::getUnit(bool include_dB,
                    bool hold) { // Note: includes UTF-8 characters
  if (hold) {
    return include_dB ? cache.hold.unit_with_db : cache.hold.unit;
  }
  if (isV()) { // Voltage units
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
    @brief returns the main unit
    @details this is not intended for external presentation, only for internal
   housekeeping
    @ the supported units are: 'C' (°C), 'V' (Volt), 'O' (Ohm), 'A' (Ampere),
   'B' (dB) and ' ' (unrecognized unit)
    @returns a char corresponding to the main unit
*/
char K197device::getMainUnit() {
  if (isV()) { // Voltage or Temperature
    if (flags.tkMode && ismV() && isDC())
      return 'C';
    else
      return 'V';
  } else if (isOmega()) { // Resistence
    return 'O';
  } else if (isA()) { // Current
    return 'A';
  } else if (isdB()) { // dB
    return 'B';
  } else { // No unit found
    return ' ';
  }
}

/*!
    @brief returns the exponent corresponding to the SI multiplier
    @details 10 elevated to the exponent returned gives the power of 10
   corresponding to the prefix set in the annouciators For example, 1KΩ means
   10^3Ω ==> exponent is 3. In 1µA means 10^-6A exponent is -6. with no prefix 0
   is returned (10^0=1).
    @param hold if true returns the value at the time hold mode was last entered
    @returns the exponent corresponding to the SI multiplier
*/
int8_t K197device::getUnitPow10(bool hold) {
  if (hold) {
    return cache.hold.pow10;
  }
  if (isV()) { // Voltage units
    if (flags.tkMode && ismV() && isDC())
      return 0;
    else if (ismV())
      return -3;
    else
      return 0;
  } else if (isOmega()) { // Resistence units
    if (isM())
      return 6;
    else if (isk())
      return 3;
    else
      return 0;
  } else if (isA()) { // Current units
    if (ismicro())
      return -6;
    else if (ismA())
      return -3;
    else
      return 0;
  } else { // dB or no unit found
    return 0;
  }
}

// ***************************************************************************************
//  Debug & troubleshooting
// ***************************************************************************************

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
    DebugOut.print(F(" + OvR"));
  DebugOut.println();
}

// ***************************************************************************************
//  Hold mode handling
// ***************************************************************************************

/*!
    @brief  set display hold mode
    @param newValue true to enter display hold mode, false to exit
*/
void K197device::setDisplayHold(bool newValue) {
  if (newValue == flags.hold)
    return;
  if (newValue) { // activate hold mode
    // copy current data to cache.hold
    memcpy(cache.hold.raw_msg, raw_msg, K197_RAW_MSG_SIZE * sizeof(char));
    cache.hold.raw_dp = raw_dp;
    cache.hold.annunciators0 = annunciators0;
    cache.hold.msg_value = msg_value;
    cache.hold.tcold = tcold;
    cache.hold.average = cache.average;
    cache.hold.min = cache.min;
    cache.hold.max = cache.max;
    cache.hold.unit = getUnit();
    cache.hold.munit = cache.munit;
    cache.hold.unit_with_db = getUnit(true);
    cache.hold.pow10 = cache.pow10;
    cache.hold.isTKModeActive = isTKModeActive();
    cache.hold.isNumeric = flags.msg_is_num;

    // copy current graph data to cache.hold
    cache.hold.graph.copy(&(cache.graph));
    cache.hold.nsamples_graph = cache.nsamples_graph;
    digitalWriteFast(LED_BUILTIN, HIGH);
  } else {
    digitalWriteFast(LED_BUILTIN, LOW);
  }
  flags.hold = newValue;
};

// ***************************************************************************************
//  Cache handling
// ***************************************************************************************

/*!
    @brief  utility function, used to compare two set of annunciator0, ignoring
   the minus sign in the comparison
    @param b1 first annunciator0 set
    @param b2 second annunciator0 set
    @details average, max and min are calculated here
 */
static inline bool change0(byte b1, byte b2) {
  return (b1 & (~(K197_MINUS_bm | K197_BAT_bm | K197_AUTO_bm))) !=
         (b2 & (~(K197_MINUS_bm | K197_BAT_bm | K197_AUTO_bm)));
}

/*!
    @brief  utility function, get the conversion factor between measurement unit
   prefixes
    @details the input should be limited to what is visualized by the K197: -6,
   -3, 0, 3, 6 furthermore, -6 < (pow10_old-pow10_new) < +6 (nothing else is
   possible with the K197)
    @param pow10_old old power of 10 (what would have been returned by
   getUnitPow10())
    @param pow10_new new power of 10 (what is returned by getUnitPow10())
    @return the factor that when multiplied for a value expressed in old pow10
   returns the same value expressed with new pow10
 */
float getPrefixConversionFactor(int8_t pow10_old, int8_t pow10_new) {
  float fconv;
  int8_t pow10 = pow10_old - pow10_new;
  switch (pow10) {
  case -6:
    fconv = 0.000001;
    break;
  case -3:
    fconv = 0.001;
    break;
  case 3:
    fconv = 1000.0;
    break;
  case 6:
    fconv = 1000000.0;
    break;
  default:
    fconv = 1.0;
    break;
  }
  return fconv;
}

/*!
    @brief  check if cached data is invalid
    @details check if cached data is invalid because the
    unit has changed (e.g. from V to A) or one of the key announciators
    have changed (e.g. REL). It is intyended to be called from inside
   updateCache.
    @param munit the current main unit as returned by getMainUnit()
    @param pow10 the current power of 10 as returned by getUnitPow10()
    @return true if cached data is invalid
 */
bool K197device::isCacheInvalid(char munit, int8_t pow10) {
  if (cache.tkMode != flags.tkMode) {
    DebugOut.print(F("tkMode "));
    return true;
  }
  if (change0(cache.annunciators0, annunciators0)) {
    return true;
  }
  if (cache.munit != munit) {
    return true;
  }
  if (!flags.graph_full_range && (cache.pow10 != pow10)) {
    return true;
  }
  return false;
}

/*!
    @brief  update the cache
    @details if the cache is invalid isCacheInvalid() the cache and graph is
   reset (see resetStatistics()). Depending on settings, the cahced data can be
   rescaled to fit the new measurement unit prefix (see rescaleStatistics()).
   Then average, max and min are calculated and the new data is added to the
   graph (see add2graph())
 */
void K197device::updateCache() {
  if (!isNumeric())
    return; // No point updating statistics or graph now
  char munit = getMainUnit();
  int8_t pow10 = getUnitPow10();

  if (isCacheInvalid(munit,
                     pow10)) { // Something important changed, reset stats
    if (munit == CH_SPACE)
      return;                    // This is not a stable masurement, ignore
    if (cache.numInvalid == 0) { // This is the first invalid reading
      cache.numInvalid++;        // Take note we had an invalid result
      if (cache.nskip_graph !=
          0) { // We do our best to keep up with the sampling rate
        if (++cache.nskip_graph >=
            cache.nsamples_graph) // Step the counter anyway
          cache.nskip_graph = 0;
      }
      return; // Return without updating statistics/graph
    }
    resetStatistics();
  } else {
    cache.numInvalid = 0; // reset counter for next time
    if (cache.pow10 != pow10) {
      rescaleStatistics(getPrefixConversionFactor(cache.pow10, pow10));
    }
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
  cache.munit = munit;
  cache.pow10 = pow10;
  if (getAutosample() &&
      cache.nskip_graph == 0) { // Autosample is on and a sample is ready
    if (cache.graph.isFull()) { // And no room left for an extra sample
      uint16_t graphPeriod = getGraphPeriod();
      if (graphPeriod <
          max_graph_period) { //   And we have room to increase graphPeriod
        if (graphPeriod == 0)
          graphPeriod = 1;
        else
          graphPeriod *= 2;
        if (graphPeriod > max_graph_period)
          graphPeriod = max_graph_period;
        setGraphPeriod(graphPeriod); // Then we set the new period (this will
                                     // also trigger the re-sampling)
      }
    }
  }
  cache.add2graph(msg_value);
  CHECK_FREE_STACK();
}

/*!
    @brief  reset all statistics (min, average, max) & graph data
    @details average, max and min are reset, then resetGraph is invoked
 */
void K197device::resetStatistics() {
  cache.average = msg_value;
  cache.min = msg_value;
  cache.max = msg_value;
  cache.resetGraph();
}

/*!
    @brief  rescale all statistics (min, average, max) & graph data
    @details average, max and min are multiplied by fconv
    then graph.rescale(fconv) is invoked
    @param fconv the
 */
void K197device::rescaleStatistics(float fconv) {
  cache.average *= fconv;
  cache.min *= fconv;
  cache.max *= fconv;
  cache.graph.rescale(fconv);
}

// ***************************************************************************************
//  Graph & Autoscaling
// ***************************************************************************************

/*!
   @brief reset graph
*/
void K197device::k197_cache_struct::resetGraph() {
  graph.clear();
  nskip_graph = 0x00;
  if (autosample_graph) { // if autosample is set then...
    nsamples_graph = 0;   // set fastest sampling period
  }
}

const float scaleFactor[] PROGMEM = {
    1E-6, 1E-5, 1E-4, 1E-3, 1E-2, 0.1, 1,
    10,   1E2,  1E3,  1E4,  1E5,  1E6}; ///< helper array

#define sizeof_scaleFactor                                                     \
  int(sizeof(scaleFactor) /                                                    \
      sizeof(scaleFactor[0])) ///< number of elements in the scaleFactor array

#define SCALE_VALUE_MIN                                                        \
  1E-6 ///< minimum scale factor that we can calculate via scaleFactor array
#define SCALE_LOG_MIN                                                          \
  -6 ///< maximum scale factor that we can calculate via scaleFactor array

/*!
 @brief calculates the ceiling of the log (base 10) of the absolute value of a
 floating point number.
 @details: fast implementation using the scaleFactor array. In this application
 is used to calculate the scale of a graph.
 @param x the input value
*/
void k197graph_label_type::setLog10Ceiling(float x) {
  x = fabsf(x);
  if (x <= SCALE_VALUE_MIN) {
    pow10 = SCALE_LOG_MIN;
    return;
  }
  // return ceilf(log10f(x)); // next code is equivalent but faster
  for (int8_t i = sizeof_scaleFactor - 1; i > 0; i--) {
    if (x * pgm_read_float(&scaleFactor[i]) < 1.0) {
      pow10 = 6 - i;
      return;
    }
  }
  pow10 = 6;
}

/*!
 @brief calculates a power of 10.
 @details: fast implementation using the scaleFactor array.
 @param i the exponent of the power of 10 (10^i)
 @returns the value that would be returned by powf(10.0, i) within the range
 covered by the scaleFactor array
*/
float k197graph_label_type::getpow10(int i) {
  // return powf(10.0, i); // next code is equivalent but faster
  if (i < -6)
    i = -6;
  else if (i > 6)
    i = 6;
  return pgm_read_float(&scaleFactor[i + 6]);
}

/*!
 @brief set the multiplier and pow10 of the scale (label) corrresponding to a
 given value
 @details: A multiplier is 1, 2 or 5. The multiplier mult is chosen so that mult
 * pow10 is the minimum possible that is above (up) the value
 @param x the input value
*/
void k197graph_label_type::setScaleMultiplierUp(
    float x) {                       // Look for the maximum
  float norm = x * getpow10(-pow10); // normalized: -1<norm<1
  if (norm > 0) {
    if (norm < 0.2) {
      pow10--;
      mult = 2;
    } else if (norm < 0.5) {
      pow10--;
      mult = 5;
    } else {
      mult = 1;
    }
  } else {
    if (norm < -0.5) {
      pow10--;
      mult = -5;
    } else if (norm < -0.2) {
      pow10--;
      mult = -2;
    } else {
      pow10--;
      mult = -1;
    }
  }
}

/*!
 @brief set the multiplier and pow10 of the scale (label) corresponding to a
 given value
 @details: A multiplier is 1, 2 or 5. The multiplier mult is chosen so that mult
 * pow10 is the maximum possible that is below (down) the value
 @param x the input value
*/
void k197graph_label_type::setScaleMultiplierDown(
    float x) {                       // Look for the minimum
  float norm = x * getpow10(-pow10); // normalized: 1>=abs(norm)>=1
  if (norm > 0) {
    if (norm > 0.5) {
      pow10--;
      mult = 5;
    } else if (norm > 0.2) {
      pow10--;
      mult = 2;
    } else {
      pow10--;
      mult = 1;
    }
  } else {
    if (norm > -0.2) {
      pow10--;
      mult = -2;
    } else if (norm > -0.5) {
      pow10--;
      mult = -5;
    } else {
      mult = -1;
    }
  }
}

/*!
 @brief select the scale for the Y axis of a graph (autoscaling)
 @details The scale is set so that the graph can be displayed with the best
 possible resolution
 @param grmin the minimum y value in the graph
 @param grmax the maximum y value in the graph
 @param yopt the required options for the scale
*/
void k197_display_graph_type::setScale(
    float grmin, float grmax, k197graph_yscale_opt yopt,
    bool canBeNegative RT_ASSERT_ADD_PARAM(bool debug_flag)) {
  // Autoscale -  First we find the order of magnitude (power of 10)
  y0.setLog10Ceiling(grmin);
  y1.setLog10Ceiling(grmax);
  // Then we fine tune the multiplier (1x, 2x, 5x, sign can be + or -)
  y0.setScaleMultiplierDown(grmin);
  y1.setScaleMultiplierUp(grmax);

  if (y0 == y1) { // Safeguard from pathological cases...
    if (y1.isPositive()) {
      ++y1;
      --y0;
    } else if (y1.isNegative()) {
      --y1;
      ++y0;
    } else { // y1 == y0 == 0.0
      y1.setValue(1, SCALE_LOG_MIN);
      y0.setValue(-1, SCALE_LOG_MIN);
    }
  }
  RT_ASSERT_ADD_STATEMENTS(if (debug_flag) {
    DebugOut.print(F("y0: "));
    y0.debug_print();
    DebugOut.print(F(" abs="));
    y0.abs().debug_print();
    DebugOut.println();
    DebugOut.print(F("y1: "));
    y1.debug_print();
    DebugOut.print(F(" abs="));
    y1.abs().debug_print();
    DebugOut.println();
  })

  // apply y scale options
  if (yopt == k197graph_yscale_zero || yopt == k197graph_yscale_0sym ||
      yopt == k197graph_yscale_0forcesym) { // Make sure the value 0 is included
    RT_ASSERT_ADD_STATEMENTS(if (debug_flag) DebugOut.println(F("zero")));
    if (y0.isPositive())
      y0.reset();
    if (y1.isNegative())
      y1.reset();
  }
  if (canBeNegative &&
      (yopt == k197graph_yscale_prefsym ||
       yopt == k197graph_yscale_0sym)) { // Make symmetric if 0 is included
    RT_ASSERT_ADD_STATEMENTS(if (debug_flag) DebugOut.println(F("prefsym")));
    if (y0.isNegative() && y1.isPositive()) { // zero is included in the graph
      if (y0.abs() > y1)
        y1.setValue(-y0);
      else
        y0.setValue(-y1);
    }
  } else if (canBeNegative &&
             ((yopt == k197graph_yscale_forcesym) ||
              (yopt == k197graph_yscale_0forcesym))) { // Make symmetric even if
                                                       // 0 not included
    RT_ASSERT_ADD_STATEMENTS(if (debug_flag) DebugOut.println(F("forcesym")));
    if (y1.isPositive() && y0.isPositive()) { // all values are above 0
      y0.setValue(-y1);
    } else if (y1.isNegative() && y0.isNegative()) { // all values are below 0
      y1.setValue(-y0);
    } else { // y0 negative and y1 positive
      if (y0.abs() > y1)
        y1.setValue(-y0);
      else
        y0.setValue(-y1);
    }
  }
  RT_ASSERT_ADD_STATEMENTS(if (debug_flag) {
    DebugOut.print(F("y0: "));
    y0.debug_print();
    DebugOut.println();
    DebugOut.print(F("y1: "));
    y1.debug_print();
    DebugOut.println();
  })
}

/*!
 @brief fills a k197_display_graph_type data structure with the values currently
 stored in the cache
 @details The scale k197_display_graph_type data structure is used to display
 the graph on a oled The function concverts the high resolution floating point
 numbers from the voltmeter stored in the cache into integer values representing
 the coordinates of the pixels in the oled display, together with other
 information that is needed to display the graph (e.g. the axis labels)
 @param graphdata pointer to the data structure to fill
 @param yopt the required options for the scale
 @param hold if true returns the value at the time hold mode was last entered
*/
void K197device::fillGraphDisplayData(k197_display_graph_type *graphdata,
                                      k197graph_yscale_opt yopt, bool hold) {
  k197_stored_graph_type *graph = hold ? &cache.hold.graph : &cache.graph;
  byte gr_size = graph->getSize();

  // find max and min in the data set
  float grmin = graph->calcMin();
  float grmax = graph->calcMax();

  graphdata->setScale(grmin, grmax, yopt, k197dev.valueCanBeNegative(hold));
  float ymin = graphdata->y0.getValue();
  float ymax = graphdata->y1.getValue();
  RT_ASSERT_ADD_STATEMENTS(bool runAgain = false;)
  RT_ASSERT_ACT(ymin <= grmin, DebugOut.print(F(", ymin="));
                DebugOut.print(ymin, 6); DebugOut.print(F(" > grmin="));
                DebugOut.println(grmin, 6); runAgain = true;)
  RT_ASSERT_ACT(ymax >= grmax, DebugOut.print(F(", ymax="));
                DebugOut.print(ymax, 6); DebugOut.print(F(" < grmax="));
                DebugOut.println(grmax, 6); runAgain = true;)
  RT_ASSERT_ADD_STATEMENTS(
      if (runAgain) { graphdata->setScale(grmin, grmax, yopt, true); })
  float scale_factor = float(graphdata->y_size) / (ymax - ymin);

  for (int i = 0; i < gr_size; i++) {
    RT_ASSERT(i < graphdata->x_size, "!fg2a");
    if (i >= graphdata->x_size) {
      break; // protect from mem. corruption, only in case of a bug!
    }
    RT_ASSERT(i < graph->getSize(), "fg2b");
    RT_ASSERT(graphdata->point[i] <= graphdata->y_size, "fg2c");
    graphdata->point[i] = (graph->get(i) - ymin) * scale_factor + 0.5;
    if (graphdata->point[i] >
        graphdata->y_size) { // Can only be to bugs or rounding...
      // force within display area, otherwise u8g2 would slow down hence data
      // loss, etc. etc.
      graphdata->point[i] = graphdata->y_size;
    }
  }
  if (graphdata->y0.isNegative() &&
      graphdata->y1.isPositive()) { // 0 is included in the graph
    graphdata->y_zero = 0.5 - ymin * scale_factor;
  } else {
    graphdata->y_zero = 0;
  }
  graphdata->gr_size = gr_size;
  graphdata->nsamples_graph =
      hold ? cache.hold.nsamples_graph : cache.nsamples_graph;
}

/*!
 \def SWAP_BYTE(b0, b1)

 @brief utility macro used to swap the value of two byte variables
 @details After this macro is executed b0 wand b1 values will be swapped
 @param b0 the first byte to swap
 @param b1 the second byte to swap
*/
#define SWAP_BYTE(b0, b1)                                                      \
  {                                                                            \
    byte b_tmp = b0;                                                           \
    b0 = b1;                                                                   \
    b1 = b_tmp;                                                                \
  }

/*!
   @brief get average of the graph
   @details note: the starting point must be an array index
   @param first_point the array index to use as a starting point
   @param num_points the number of points to average [range: 0 - gr_size-1]
   @param hold if true returns the value at the time hold mode was last entered
   @return the requested average value (or 0.0 if num_pts==0 or gr_size==0)
*/
float K197device::getGraphAverage(byte first_point, byte num_points,
                                  bool hold) {
  return hold ? cache.hold.graph.calcAverage(first_point, num_points)
              : cache.graph.calcAverage(first_point, num_points);
}

/*!
   @brief resample the graph
   @details resample the stored data to match the new sample rate, then set
   nsamples_graph to nsamples_new
   @param nsamples_new new value of nsamples_graph
*/
void K197device::k197_cache_struct::resampleGraph(uint16_t nsamples_new) {
  byte gr_size = graph.getSize();
  if (gr_size == 0 || nsamples_new == nsamples_graph) {
    return;
  }
  uint16_t nsamples_old_positive = nsamples_graph == 0 ? 1 : nsamples_graph;
  uint16_t nsamples_new_positive = nsamples_new == 0 ? 1 : nsamples_new;

  unsigned long gr_size_new = long(gr_size - 1) * long(nsamples_old_positive) /
                                  long(nsamples_new_positive) +
                              1l + nskip_graph / nsamples_new_positive;
  if (gr_size_new > graph.max_graph_size)
    gr_size_new = graph.max_graph_size;
  RT_ASSERT(gr_size_new <= graph.max_graph_size, "rsmpl1a");
  RT_ASSERT(gr_size_new > 0, "rsmpl1b");
  float buffer[gr_size_new];

  if (nsamples_new >
      nsamples_graph) { // Decimation to match the new sample rate
    //  Copy the decimated data into the buffer, with correct ordering
    unsigned int new_idx = 0;
    for (int old_idx = 0; old_idx < gr_size; old_idx++) {
      if (old_idx * nsamples_old_positive >= new_idx * nsamples_new_positive) {
        RT_ASSERT_ACT(new_idx >= gr_size_new,
                      DebugOut.println("Error: new_idx 1");
                      break;)
        RT_ASSERT(new_idx < gr_size_new, "rsmpl2");
        RT_ASSERT(old_idx < graph.getSize(), "rsmpl3");
        buffer[new_idx] = graph.get(old_idx);
        new_idx++;
      }
    }
    RT_ASSERT(new_idx != gr_size_new, "Error: new_idx 2");
    // Adjust cache size. Note that nskip_graph does not need to change
    gr_size = new_idx;
  } else { // Add more data to match the new sample rate
    unsigned int old_idx = gr_size - 1;
    unsigned int new_idx = gr_size_new - 1;

    for (unsigned int n = 0; n < (nskip_graph / nsamples_new_positive); n++) {
      RT_ASSERT(new_idx < gr_size_new, "rsmpl4");
      RT_ASSERT(gr_size - 1 < graph.getSize(), "rsmpl5");
      buffer[new_idx] = graph.get(gr_size - 1);
      new_idx--;
    }
    // Adjust nskip_graph
    nskip_graph = nskip_graph % nsamples_new_positive;

    // Resample with shorter period. Old data may be lost here if there is no
    // room
    for (int i = new_idx; i >= 0; new_idx--, i--) {
      if (new_idx * nsamples_new_positive < old_idx * nsamples_old_positive) {
        if (old_idx > 0)
          old_idx--;
      }
      RT_ASSERT(new_idx < gr_size_new, "rsmpl6");
      RT_ASSERT(old_idx < graph.getSize(), "rsmpl7");
      buffer[new_idx] = graph.get(old_idx);
    }
    // Adjust cache size
    gr_size = gr_size_new;
  }
  CHECK_FREE_STACK(); // We may be using quite a bit of stack for the buffer

  graph.copy(buffer, gr_size); // Copy the buffer back to the cache
  nsamples_graph = nsamples_new;
}
