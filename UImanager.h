/**************************************************************************/
/*!
  @file     UImanager.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the UImanager class

  This class is responsible for displaying the information on the SSD1322
  display module

*/
/**************************************************************************/
#ifndef UIMANAGER_H__
#define UIMANAGER_H__
#include "K197device.h"
#include <Arduino.h>
#include <U8g2lib.h>

extern U8G2LOG u8g2log; ///< This is used to display the debug log on the OLED

/**************************************************************************/
/*!
    @brief  Simple class to handle the display

    This class is responsible to handle the user interface displayed to the user
   on the OLED

    A pointer to a K197device object must be provided at instantiation. This is
   the object that will provide the information to display.

*/
/**************************************************************************/
class UImanager {
public:
  static const byte displayNormal =
      0; ///< constant used to indicate the normal display layout
  static const byte displayDebug =
      1; ///< constamnt used to indicate a display layout including showing the
         ///< debug output

private:
  K197device *k197;
  bool show_volt = false; ///< Show voltages if true (not currently used)
  bool show_temp = false; ///< Show temperature if true  (not currently used)
  byte display_mode = displayNormal; // Keep track of how to display stuff...

  void updateDisplayNormal();
  void updateDisplaySplit();

public:
  UImanager(K197device *k197);
  void setup();
  void setScreenMode(byte mode);

  /*!
    @brief  get the screen mode
   @return screen mode, it must be one of the displayXXX constants defined in
   class UImanager
*/
  byte getScreenMode() { return display_mode; };

  void updateDisplay();
  void updateBtStatus(bool present);

  void setContrast(uint8_t value);
};

#endif // UIMANAGER_H__
