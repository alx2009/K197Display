/**************************************************************************/
/*!
  @file     UImanager.h

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
  K197sc_normal = 0x11,          ///< equivalent to original K197
  K197sc_minmax = 0x12,          ///< equivalent to original K197
  K197sc_mainMenu = 0x03,        ///< display main menu
  K197sc_debug = 0x04,           ///< display log window
  K197sc_FullScreenMask = 0x10   ///< full/split screen detection
};

/**************************************************************************/
/*!
    @brief  the class responsible for managing the display

    This class is responsible to handle the user interface displayed to the user
   on the OLED

    A pointer to a K197device object must be provided at instantiation. This is
   the object that will provide the information to display.

*/
/**************************************************************************/
class UImanager {
private:
  bool show_volt = false; ///< Show voltages if true (not currently used)
  bool show_temp = false; ///< Show temperature if true  (not currently used)
  K197screenMode screen_mode =
      K197sc_normal; ///< Keep track of how to display stuff...

  void updateNormalScreen();
  void updateMinMaxScreen();
  void updateSplitScreen();

  void setupMenus();

  byte logskip_counter = 0; ///< counter used whenj data logging, counts how
                            ///< many measurements are skipped

public:
  UImanager(){}; ///< default constructor for the class
  void setup();
  void setScreenMode(K197screenMode mode);

  /*!
     @brief  get the screen mode
     @return screen mode, it must be one of the displayXXX constants defined in
     class UImanager
  */
  K197screenMode getScreenMode() { return screen_mode; };

  /*!
     @brief  check if display is in full screen mode
     @return true if in full screen mode
  */
 bool isFullScreen() {return screen_mode & K197sc_FullScreenMask;}
  /*!
     @brief  check if display is in split screen mode
     @return true if in split screen mode
  */
  bool isSplitScreen() {return !isFullScreen();}

  void updateDisplay();
  void updateBtStatus();

  void setContrast(uint8_t value);

  bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);

  bool isExtraModeEnabled();
  bool reassignStoRcl();

  void setLogging(bool yesno);
  bool isLogging();

  void logData();

  static const char *formatNumber(char buf[K197_MSG_SIZE], float f);

};

extern UImanager uiman;

#endif // UIMANAGER_H__
