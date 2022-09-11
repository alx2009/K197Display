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
class debugUtil : public Print {
  bool use_serial = false; ///< enable Serial output if true
  bool use_oled = false;   ///< enable Oled output if true
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
};

extern debugUtil
    DebugOut; ///< this is the predefined oubject that is used with pribnt(),
              ///< etc. (similar to how Serial is used for debug output)

#endif // DEBUGUTIL_H__
