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

#include "UIevents.h"

extern U8G2LOG u8g2log; ///< This is used to display the debug log on the OLED

/**************************************************************************/
/*!
    @brief  Simple enum to identify the screen mode being displayed
*/
/**************************************************************************/
enum K197screenMode {
   K197sc_normal   = 0x01,  ///< equivalent to original K197 
   K197sc_mainMenu = 0x02,  ///< display main menu
   K197sc_debug    = 0x03,  ///< display log window
   K197sc_logMenu  = 0x04,  ///< display main menu
};

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
private:
  K197device *k197;
  bool show_volt = false; ///< Show voltages if true (not currently used)
  bool show_temp = false; ///< Show temperature if true  (not currently used)
  K197screenMode screen_mode = K197sc_normal; // Keep track of how to display stuff...

  void updateDisplayNormal();
  void updateDisplaySplit();

  void setupMenus();

  bool msg_log = false;      ///< if true logs data to Serial
  byte logskip=0;

  bool handleUIEventMainMenu(K197UIeventsource eventSource, K197UIeventType eventType);
  bool handleUIEventLogMenu(K197UIeventsource eventSource, K197UIeventType eventType);

public:
  UImanager(K197device *k197);
  void setup();
  void setScreenMode(K197screenMode mode);

  /*!
    @brief  get the screen mode
   @return screen mode, it must be one of the displayXXX constants defined in
   class UImanager
*/
  K197screenMode getScreenMode() { return screen_mode; };

  void updateDisplay();
  void updateBtStatus(bool present, bool connected);

  void setContrast(uint8_t value);

  bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);

  //TODO: document and implement via settings menu
  bool isExtraModeEnabled();
  bool isBtDatalogEnabled();

    /*!
      @brief  set data logging to Serial
      @param yesno true to enabl, false to disable
  */
  void setLogging(bool yesno) {
        if (!yesno) logskip=0;
        msg_log = yesno;
  }

  /*!
      @brief  query data logging to Serial
      @return returns trueif logging is active
  */
  bool isLogging() {
        return msg_log;
  }

  void logData();
};

#endif // UIMANAGER_H__
