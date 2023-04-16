/**************************************************************************/
/*!
  @file     debugUtil.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the debugUtil class/functions

  This class encapsulate a number of utility functions to support debugging

*/
/**************************************************************************/
#ifndef DEBUGUTIL_H__
#define DEBUGUTIL_H__
#include <Arduino.h>

/**************************************************************************/
/*!
    @brief  Debug utility class

    This class encapsulate a number of utility functions useful for debugging
    The function implements the Print interface like Serial, etc.
    The output will go to Serial and/or the Oled (the latter via u8g2log defined
   in UIManager.h), depending on the properties set with begin and/or
   useSerial() and/or useOled()

    Normally this class is used via the predefined object DebugOut (similarly to
   how Serial is used)

    PREREQUISITES: Serial.begin() and u8g2log.begin() must be called before
   enabling the Serial and the Oled output respectively The latter is called
   when an object of class UImanager is initialized via UImanager::begin()

*/
/**************************************************************************/

//#define PROFILE_TIMER 1 ///< when defined, add the possibility to profile
// sections of code

#define RUNTIME_ASSERTS 1 ///< when defined, add additional runtime checks

/**************************************************************************/
/*!
   @brief class implementing an object used as debug output

   @details This class implements the usual Print interface and can send the
   output to two parallel streams:
   - Serial
   - a u8g2log object of type U8G2LOG, it can be used to show the log in the
   oled screen via the u8g2 library

    PREREQUISITES: Serial.begin() and u8g2log.begin() must be called before
   enabling the Serial and the Oled output respectively.
*/
/**************************************************************************/
class debugUtil : public Print {
  bool use_serial = false; ///< enable Serial output if true
  bool use_oled = false;   ///< enable Oled output if true

#ifdef PROFILE_TIMER
  unsigned long proftimer[PROFILE_TIMER]; ///< store the profile timers(s)

public:
  static const byte PROFILE_MATH = 0;    ///< profiler time slot for math stuff
  static const byte PROFILE_LOOP = 1;    ///< profiler time slot for loop()
  static const byte PROFILE_DEVICE = 2;  ///< profiler time slot for K197dev
  static const byte PROFILE_DISPLAY = 3; ///< profiler time slot for uiman
#endif                                   // PROFILE_TIMER

public:
  /*!
     @brief  default constructor for the class.

     Note that normally there is no need to declare an object explciitly, just
     use the predefined DebugOut
  */
  debugUtil(){};
  /*!
     @brief  initialize the object.

     It should be called before using the object, albeit in the curent
     implementation it is safe to use it even before, since by default debug
     output is disabled.

     PREREQUISITES: Serial.begin and UImanager.begin must be called if they are
     enabled

     @param serial enable Serial output if true, disable if false
     @param oled enable oled output (via u8g2log) if true, disable if false
  */
  void begin(bool serial = false, bool oled = false) {
    use_serial = serial;
    use_oled = oled;
  };
  /*!
    @brief  query if Serial output is enabled
    @return true if Serial output is enabled, false otherwise
 */
  bool useSerial() { return use_serial; };
  /*!
     @brief  enabled/disable Serial output.

     PREREQUISITES: Serial.begin() must be called if Serial output is enabled

     @param serial enable Serial output if true, disable if false
  */
  void useSerial(bool serial) { use_serial = serial; };
  /*!
    @brief  query if Oled output is enabled
    @return true if Oled output is enabled, false otherwise
 */
  bool useOled() { return use_oled; };
  /*!
     @brief  enabled/disable Oled output.

     PREREQUISITES: u8g2log.begin() must be called if Oled output is enabled.
     Note that this is automatically done when any object of class UImanager is
     initialized with UImanager::begin()

     @param oled enable oled output if true, disable if false
  */
  void useOled(bool oled) { use_oled = oled; };

  virtual size_t write(uint8_t);
  virtual size_t write(const char *str);
  virtual size_t write(const uint8_t *buffer, size_t size);
  virtual int availableForWrite();
  virtual void flush();

#ifdef PROFILE_TIMER
  /*!
    @brief  start a profile timer
    @details a profile timer is used to measure the execution time of a given
    section of code bracketed between profileStart() and profileEnd();
    @param slot the slot number of this particular timer
  */
  void profileStart(unsigned int slot) {
    if (slot < PROFILE_TIMER)
      proftimer[slot] = micros();
  }
  /*!
    @brief  Stop a profile timer
    @param slot the slot number of this particular timer
  */
  void profileStop(unsigned int slot) {
    if (slot < PROFILE_TIMER)
      proftimer[slot] = micros() - proftimer[slot];
  }
  /*!
    @brief  print a profile timer
    @param slot the slot number of this particular timer
  */
  void profilePrint(unsigned int slot) {
    if (slot < PROFILE_TIMER)
      print(proftimer[slot]);
  }
  /*!
    @brief  print a profile timer
    @param slot the slot number of this particular timer
    @param slotName the name of the slot
  */
  void profilePrintln(unsigned int slot, const __FlashStringHelper *slotName) {
    if (slot < PROFILE_TIMER) {
      print(slotName);
      print('=');
      print(proftimer[slot]);
      println(F("us"));
    }
  }
#endif // PROFILE_TIMER
};

extern debugUtil
    DebugOut; ///< this is the predefined object that is used with print(),
              ///< etc. (similar to how Serial is used for debug output)

#ifdef PROFILE_TIMER
#define PROFILE_start(...)                                                     \
  DebugOut.profileStart(__VA_ARGS__) ///< macro to start profiling
#define PROFILE_stop(...)                                                      \
  DebugOut.profileStop(__VA_ARGS__) ///< macro to stop profiling
#define PROFILE_print(...)                                                     \
  DebugOut.profilePrint(__VA_ARGS__) ///< macro to print the counter
#define PROFILE_println(...)                                                   \
  DebugOut.profilePrintln(__VA_ARGS__) ///< macro to print the counter
#else
#define PROFILE_start(...)   ///< Does nothing when not profiling
#define PROFILE_stop(...)    ///< Does nothing when not profiling
#define PROFILE_print(...)   ///< Does nothing when not profiling
#define PROFILE_println(...) ///< Does nothing when not profiling
#endif                       // PROFILE_TIMER

#ifdef RUNTIME_ASSERTS
#define RT_ASSERT(condition, message_string) if (!(condition)) {DebugOut.println(F(message_string));}
#define RT_ASSERT_EXT(condition, action) if (!(condition)) {action;}
#define RT_ASSERT_EXT2(condition, action1, action2) if (!(condition)) {action1; action2;}
#else
#define RT_ASSERT(...) ///< Does nothing with real time assets disabled
#define RT_ASSERT_EXT(...) ///< Does nothing with real time assets disabled
#define RT_ASSERT_EXT2(...) ///< Does nothing with real time assets disabled
#endif //RUNTIME_ASSERTS 

#endif // DEBUGUTIL_H__
