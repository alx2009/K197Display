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
  char
      raw_msg[K197_RAW_MSG_SIZE]; // 0 term. string, stores sign + 6 char, no DP
  byte raw_dp = 0x00; // Stores the Decimal Point (bit 0 = not used, bit 1...7
                      // char 0-6 of raw_msg)
  char message[K197_MSG_SIZE]; // 0 term. string, sign + 6 char + DP at the
                               // right position

  bool msg_is_num = false;     // true if message is numeric
  bool msg_is_ovrange = false; // true if overange detected
  float msg_value;             // numeric value of message

  byte annunciators0 = 0x00; // Stores MINUS BAT RCL AC dB STO REL AUTO
  byte annunciators7 = 0x00; // Stores mA, k, V, u (micro), M, m (mV)
  byte annunciators8 = 0x00; // Stores RMT, A, Omega (Ohm), C

  inline static bool hasDecimalPoint(byte b) { return (b & 0b00000100) != 0; };
  inline static bool isDigitOrSpace(char c) {
    if (isDigit(c))
      return true;
    if (c == ' ')
      return true;
    return false;
  };

public:
  K197device() {
    message[0] = 0;
    raw_msg[0] = 0;
  };
  void getNewReading();
  byte getNewReading(byte *data);

  const char *getMessage() { return message; };
  const char *getRawMessage() { return raw_msg; };
  bool isDecPointOn(byte char_n) { return bitRead(raw_dp, char_n); };

  const char *getUnit(); // Note: includes UTF-8 characters
  bool isOvrange() { return msg_is_ovrange; };
  bool notOvrange() { return !msg_is_ovrange; };
  bool isNumeric() { return msg_is_num; };
  float getValue() { return msg_value; };
  void debugPrint();

public:
  // annunciators0
  inline bool isAuto() { return (annunciators0 & K197_AUTO_bm) != 0; }; // TODO

  inline bool isREL() { return (annunciators0 & K197_REL_bm) != 0; }; // TODO
  inline bool isSTO() { return (annunciators0 & K197_STO_bm) != 0; }; // TODO
  inline bool isdB() { return (annunciators0 & K197_dB_bm) != 0; };   // TODO
  inline bool isAC() { return (annunciators0 & K197_AC_bm) != 0; };
  inline bool isRCL() { return (annunciators0 & K197_RCL_bm) != 0; }; // TODO
  inline bool isBAT() { return (annunciators0 & K197_BAT_bm) != 0; }; // TODO
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
  inline bool isOmega() {
    return (annunciators8 & K197_Omega_bm) != 0;
  }; // TODO
  inline bool isA() { return (annunciators8 & K197_A_bm) != 0; };
  inline bool isRMT() { return (annunciators8 & K197_RMT_bm) != 0; }; // TODO
};

#endif // K197_DEVICE_H
