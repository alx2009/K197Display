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
#define K197_AUTO_bm 0x01
#define K197_REL_bm 0x02
#define K197_STO_bm 0x04
#define K197_dB_bm 0x08
#define K197_AC_bm 0x10
#define K197_RCL_bm 0x20
#define K197_BAT_bm 0x40
#define K197_MINUS_bm 0x80

// annunciators7
#define K197_mV_bm 0x01
#define K197_M_bm 0x02
#define K197_micro_bm 0x04
#define K197_V_bm 0x08
#define K197_k_bm 0x10
#define K197_mA_bm 0x20

// annunciators8
#define K197_Cal_bm 0x01
#define K197_Omega_bm 0x02
#define K197_A_bm 0x04
#define K197_RMT_bm 0x20

#define K197_MSG_SIZE 9     // 6 digits + [sign] + [dot] + null
#define K197_RAW_MSG_SIZE 8 // 6 digits + [sign] + null

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
  char raw_msg[K197_RAW_MSG_SIZE]; ///< stores decoded sign + 6 char, no DP (0
                                   ///< term. char array)
  byte raw_dp = 0x00; ///< Stores the Decimal Point (bit 0 = not used, bit 1...7
                      ///< = DP bit for digit/char 0-6 of raw_msg)
  char message[K197_MSG_SIZE]; ///< stores decoded sign + 6 digits/chars + DP at
                               ///< the right position

  bool msg_is_num = false;     ///< true if message is numeric
  bool msg_is_ovrange = false; ///< true if overange detected
  float msg_value;             ///< numeric value of message

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
    if (c == ' ')
      return true;
    return false;
  };

public:
  /*!
      @brief  constructor for the class. Do not forget that setup() must be
     called before using the other member functions.
  */
  K197device() {
    message[0] = 0;
    raw_msg[0] = 0;
  };
  bool getNewReading();
  byte getNewReading(byte *data);

  /*!
      @brief  Return the decoded message
      @return last message received from the K197/197A, including sign and
     decimal point.
  */
  const char *getMessage() { return message; };
  /*!
      @brief  Return the raw message (same as message except there is no decimal
     point)

      This is useful for example to print out the measurement value to Serial.
     Not recommended for the actual display (see getRawMessage() and
     isDecPointOn() instead

      @return last raw message received from the K197/197A
  */
  const char *getRawMessage() { return raw_msg; };

  /*!
      @brief  check if there is a decimal point on the nth character in raw
     message

      This is useful because in a 7 segment display the digit position is fixed
     (obviously). If we used getMessage for the display, the digits would move
     as the range changes, since the decimal point is considered a character on
     its own by the display driver library. And this is annoying, especially
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

  const char *getUnit(); // Note: includes UTF-8 characters

  /*!
      @brief  check if overange is detected
      @return true if overange is detected
  */

  bool isOvrange() { return msg_is_ovrange; };

  /*!
      @brief  check if overange is detected
      @return true if overange is NOT detected
  */
  bool notOvrange() { return !msg_is_ovrange; };

  /*!
      @brief  check if message is a number
      @return true if message is a number (false means something else, like
     "Err", "0L", etc.)
  */
  bool isNumeric() { return msg_is_num; };

  /*!
      @brief  returns the measurement value if available
      @return returns the measurement value if available (isNumeric() returns
     true) or 0.0 otherwise
  */
  float getValue() { return msg_value; };

  void debugPrint();

public:
  // annunciators0
  inline bool isAuto() { return (annunciators0 & K197_AUTO_bm) != 0; };

  inline bool isREL() { return (annunciators0 & K197_REL_bm) != 0; };
  inline bool isSTO() { return (annunciators0 & K197_STO_bm) != 0; };
  inline bool isdB() { return (annunciators0 & K197_dB_bm) != 0; };
  inline bool isAC() { return (annunciators0 & K197_AC_bm) != 0; };
  inline bool isRCL() { return (annunciators0 & K197_RCL_bm) != 0; };
  inline bool isBAT() { return (annunciators0 & K197_BAT_bm) != 0; };
  inline bool isMINUS() { return (annunciators0 & K197_MINUS_bm) != 0; };

  // annunciators7
  inline bool ismV() { return (annunciators7 & K197_mV_bm) != 0; };
  inline bool isM() { return (annunciators7 & K197_M_bm) != 0; };
  inline bool ismicro() { return (annunciators7 & K197_micro_bm) != 0; };
  inline bool isV() { return (annunciators7 & K197_V_bm) != 0; };
  inline bool isk() { return (annunciators7 & K197_k_bm) != 0; };
  inline bool ismA() { return (annunciators7 & K197_mA_bm) != 0; };

  // annunciators8
  inline bool isCal() { return (annunciators8 & K197_Cal_bm) != 0; };
  inline bool isOmega() { return (annunciators8 & K197_Omega_bm) != 0; };
  inline bool isA() { return (annunciators8 & K197_A_bm) != 0; };
  inline bool isRMT() { return (annunciators8 & K197_RMT_bm) != 0; };
};

#endif // K197_DEVICE_H
