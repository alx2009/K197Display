/**************************************************************************/
/*!
  @file     K197device.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the k197device class

  This class is responsible to store and decode the raw information received
  through the base class SPIdevice, and make it available to the rest of the
  sketch

*/
/**************************************************************************/
#ifndef K197_DEVICE_H
#define K197_DEVICE_H
#include "SPIdevice.h"
#include <Arduino.h>
#include <ctype.h> // isDigit()

// define bitmaps for the various annunciators

// annunciators0
#define K197_AUTO_bm 0x01  ///< bitmap for the AUTO annunciator
#define K197_REL_bm 0x02   ///< bitmap for the REL annunciator
#define K197_STO_bm 0x04   ///< bitmap for the STO annunciator
#define K197_dB_bm 0x08    ///< bitmap for the dB annunciator
#define K197_AC_bm 0x10    ///< bitmap for the AC annunciator
#define K197_RCL_bm 0x20   ///< bitmap for the RCL annunciator
#define K197_BAT_bm 0x40   ///< bitmap for the MAT annunciator
#define K197_MINUS_bm 0x80 ///< bitmap for the "-" annunciator

// annunciators7
#define K197_mV_bm 0x01    ///< bitmap for the "m" annunciator used for mV
#define K197_M_bm 0x02     ///< bitmap for the "M" annunciator
#define K197_micro_bm 0x04 ///< bitmap for the "µ" annunciator
#define K197_V_bm 0x08     ///< bitmap for the "V" annunciator
#define K197_k_bm 0x10     ///< bitmap for the "k" annunciator
#define K197_mA_bm 0x20    ///< bitmap for the "m" annunciator used for mA

// annunciators8
#define K197_Cal_bm 0x01   ///< bitmap for the CAL annunciator
#define K197_Omega_bm 0x02 ///< bitmap for the "Ω" annunciator
#define K197_A_bm 0x04     ///< bitmap for the "A" annunciator
#define K197_RMT_bm 0x20   ///< bitmap for the RMT annunciator

#define K197_RAW_MSG_SIZE 8 ///< raw message size = 6 digits + [sign] + null

/**************************************************************************/
/*!
    @brief  Simple class to handle a cluster of buttons.

    This class is responsible to store and decode the raw information received
    through the base class SPIdevice, and make it available to the rest of the
    sketch

    Usage:

    Call setup() once before doing anything else
    then hasNewData() inherited from the base class should be called frequently
   (tipically in the main Arduino loop). As soon as hasNewData() returns true
   then getNewReading() must be called to decode and store the new batch of data
   from the K197/197A. After that the new information is available via the other
   member functions
*/
/**************************************************************************/
class K197device : public SPIdevice {
private:
  struct devflags_struct {
    /*!
       @brief  A union is used to simplify initialization of the flags
       @return Not really a return type, this attribute will save some RAM
    */
    union {
      unsigned char value = 0x00; ///< allows acccess to all the flags in the
                                  ///< union as one unsigned char
      struct {
        bool tkMode : 1;         ///< show T instead of V (K type thermocouple)
        bool msg_is_num : 1;     ///< true if message is numeric
        bool msg_is_ovrange : 1; ///< true if overange detected
        bool hold : 1;           ///< true if the display is holding the value
      };
    } __attribute__((packed)); ///<
  }; ///< Structure designed to pack a number of flags into one byte
  devflags_struct flags; ///< holds a number of key flags

  char raw_msg[K197_RAW_MSG_SIZE]; ///< stores decoded sign + 6 char, no DP (0
                                   ///< term. char array)
  byte raw_dp = 0x00; ///< Stores the Decimal Point (bit 0 = not used, bit 1...7
                      ///< = DP bit for digit/char 0-6 of raw_msg)

  float msg_value = 0.0; ///< numeric value of message

  byte annunciators0 = 0x00; ///< Stores MINUS BAT RCL AC dB STO REL AUTO
  byte annunciators7 = 0x00; ///< Stores mA, k, V, u (micro), M, m (mV)
  byte annunciators8 = 0x00; ///< Stores RMT, A, Omega (Ohm), C

  /*!
      @brief  check if b has the decimal point set
      @param b byte with a 7 segment + Decimal Point (DP) code as received from
     the K197/197A
      @return true if DP was set, false otherwise
  */
  inline static bool hasDecimalPoint(byte b) { return (b & 0b00000100) != 0; };
  /*!
      @brief  check if a char is a digit or a space
      @param c the char to check
      @return true if the char represents a digit or a space
  */
  inline static bool isDigitOrSpace(char c) {
    if (isDigit(c))
      return true;
    if (c == CH_SPACE)
      return true;
    return false;
  };

  float tcold = 0.0; ///< temperature used for cold junction compensation

  void setOverrange();
  void tkConvertV2C();

public:
  /*!
      @brief  constructor for the class. Do not forget that setup() must be
     called before using the other member functions.
  */
  K197device() { raw_msg[0] = 0; };
  bool getNewReading();
  byte getNewReading(byte *data);

  /*!
      @brief  get display hold mode
      @return true if display hold mode is active, false otherwise
  */
  bool getDisplayHold() { return flags.hold; };

  /*!
      @brief  set display hold mode
      @param newValue true to enter display hold mode, false to exit
  */
  void setDisplayHold(bool newValue) { flags.hold = newValue; };

  /*!
      @brief  Return the raw message

      @return last raw message received from the K197/197A
  */
  const char *getRawMessage() { return raw_msg; };

  /*!
      @brief  check if there is a decimal point on the nth character in raw
     message

      This is useful because in a 7 segment display the digit position is fixed
     (obviously). If we printed the numerical on the display, the digits would
     move as the range change, since the decimal point is considered a character
     on its own by the display driver library. And this is annoying, especially
     with auto range.

      The display code can use getRawMessage() to display the digits/chars
     without the decimal point and then use isDecPointOn() to add a decimal
     point at the right place in between

      @param  char_n the char to check (allowed range: 0 to 7, however 0 always
     return false)
      @return true if the last raw_message received from the K197/197A had a
     decimal point at the nth character
  */
  bool isDecPointOn(byte char_n) { return bitRead(raw_dp, char_n); };

  const __FlashStringHelper *
  getUnit(bool include_dB = false); // Note: includes UTF-8 characters

  /*!
      @brief  check if overange is detected
      @return true if overange is detected
  */

  bool isOvrange() { return flags.msg_is_ovrange; };

  /*!
      @brief  check if overange is detected
      @return true if overange is NOT detected
  */
  bool notOvrange() { return !flags.msg_is_ovrange; };

  /*!
      @brief  check if message is a number
      @return true if message is a number (false means something else, like
     "Err", "0L", etc.)
  */
  bool isNumeric() { return flags.msg_is_num; };

  /*!
      @brief  returns the measurement value if available
      @return returns the measurement value if available (isNumeric() returns
     true) or 0.0 otherwise
  */
  float getValue() { return msg_value; };

  void debugPrint();

  /*!
      @brief  structure used to store previus vales, average, max, min, etc.
   */
  struct {
  public:
    byte nsamples = 3;     ///< Number of samples to use for rolling average
    bool tkMode = false;   ///< caches tkMode from previous measurement
    float msg_value = 0.0; ///< caches msg_value from previous measurement
    byte annunciators0 =
        0x00; ///< caches annunciators0 from previous measurement
    byte annunciators7 =
        0x00; ///< caches annunciators7 from previous measurement
    byte annunciators8 =
        0x00; ///< caches annunciators8 from previous measurement

    float average = 0.0; ///< keep track of the average
    float min = 0.0;     ///< keep track of the minimum
    float max = 0.0;     ///< keep track of the maximum
  } cache;

  void updateCache();
  void resetStatistics();

  /*!
      @brief  set the number of samples for rolling average calculation

      @details Note that because this is a rolling average, the effect of a
     change is not immediate, it requires on the order of nsamples before it
     takes effect

      @param nsamples number of samples
  */
  void setNsamples(byte nsamples) { cache.nsamples = nsamples; };
  /*!
      @brief get the number of samples for rolling average calculation
      @return number of samples
  */
  float getNsamples() { return cache.nsamples; };

  /*!
      @brief  returns the average value (see also setNsamples())
      @return average value
  */
  float getAverage() { return cache.average; };

  /*!
      @brief  returns the minimum value
      @return minimum value
  */
  float getMin() { return cache.min; };

  /*!
      @brief  returns the maximum value
      @return maximum value
  */
  float getMax() { return cache.max; };

public:
  // annunciators0
  /*!
      @brief  test if AUTO is on
      @return returns true if on, false otherwise
  */
  inline bool isAuto() { return (annunciators0 & K197_AUTO_bm) != 0; };

  /*!
      @brief  test if REL is on
      @return returns true if on, false otherwise
  */
  inline bool isREL() { return (annunciators0 & K197_REL_bm) != 0; };
  /*!
      @brief  test if STO is on
      @return returns true if on, false otherwise
  */
  inline bool isSTO() { return (annunciators0 & K197_STO_bm) != 0; };
  /*!
      @brief  test if dB is on
      @return returns true if on, false otherwise
  */
  inline bool isdB() { return (annunciators0 & K197_dB_bm) != 0; };
  /*!
      @brief  test if AC is on
      @return returns true if on, false otherwise
  */
  inline bool isAC() { return (annunciators0 & K197_AC_bm) != 0; };
  /*!
      @brief  test if AC is on
      @return returns true if on, false otherwise
  */
  inline bool isDC() { return (annunciators0 & K197_AC_bm) == 0; };
  /*!
      @brief  test if RCL is on
      @return returns true if on, false otherwise
  */
  inline bool isRCL() { return (annunciators0 & K197_RCL_bm) != 0; };
  /*!
      @brief  test if BAT is on
      @return returns true if on, false otherwise
  */
  inline bool isBAT() { return (annunciators0 & K197_BAT_bm) != 0; };
  /*!
      @brief  test if "-" is on (negative number displayed)
      @return returns true if on, false otherwise
  */
  inline bool isMINUS() { return (annunciators0 & K197_MINUS_bm) != 0; };

  // annunciators7
  /*!
      @brief  test if "m" used for V is on
      @return returns true if on, false otherwise
  */
  inline bool ismV() { return (annunciators7 & K197_mV_bm) != 0; };
  /*!
      @brief  test if "M" is on
      @return returns true if on, false otherwise
  */
  inline bool isM() { return (annunciators7 & K197_M_bm) != 0; };
  /*!
      @brief  test if "µ" is on
      @return returns true if on, false otherwise
  */
  inline bool ismicro() { return (annunciators7 & K197_micro_bm) != 0; };
  /*!
      @brief  test if "V" is on
      @return returns true if on, false otherwise
  */
  inline bool isV() { return (annunciators7 & K197_V_bm) != 0; };
  /*!
      @brief  test if "k" is on
      @return returns true if on, false otherwise
  */
  inline bool isk() { return (annunciators7 & K197_k_bm) != 0; };
  /*!
      @brief  test if "m" used for A is on
      @return returns true if on, false otherwise
  */
  inline bool ismA() { return (annunciators7 & K197_mA_bm) != 0; };

  // annunciators8
  /*!
      @brief  test if CAL is on
      @return returns true if on, false otherwise
  */
  inline bool isCal() {
    if ((annunciators8 & K197_Cal_bm) != 0)
      return true;
    return false;
  }
  /*!
      @brief  test if CAL is off
      @return returns true if off, false otherwise
  */
  inline bool isNotCal() { return !isCal(); };
  /*!
      @brief  test if "Ω" is on
      @return returns true if on, false otherwise
  */
  inline bool isOmega() { return (annunciators8 & K197_Omega_bm) != 0; };
  /*!
      @brief  test if "A" is on
      @return returns true if on, false otherwise
  */
  inline bool isA() { return (annunciators8 & K197_A_bm) != 0; };
  /*!
      @brief  test if RMT is on
      @return returns true if on, false otherwise
  */
  inline bool isRMT() { return (annunciators8 & K197_RMT_bm) != 0; };

  // Extra modes/annnunciators not available on original K197

  /*!
      @brief  set Thermocuple mode
      @details when set, if the k197 is in DC mode and mV range, the mV reading
      is converted to a temperature value assuming a k thermocouple is connected
      @param mode true if enabled, false if disabled
  */
  void setTKMode(bool mode) { flags.tkMode = mode; }

  /*!
      @brief  get Thermocuple mode (see also setTKMode())
      @return returns true when K Thermocouple mode is enabled
  */
  bool getTKMode() { return flags.tkMode; }

  /*!
      @brief  check if the Thermocuple mode is active now
      @return returns true when K Thermocouple mode is enabled and active,
  */
  bool isTKModeActive() { return isV() && ismV() && flags.tkMode && isDC(); }

  /*!
      @brief  returns the temperature used for cold junction compensation
      @return temperature in celsius
  */
  float getTColdJunction() {
    return abs(tcold) < 999.99 ? tcold : 999.99; // Keep it in the display range
                                                 // just to be on the safe side
  }
};

extern K197device k197dev; ///< predefined device to use with this class

#endif // K197_DEVICE_H
