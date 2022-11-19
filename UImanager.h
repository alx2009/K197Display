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
    @details the lower 4 bits control the screen mode, the higher 4 bit controls other attributes:
    - bit 5 is used to distinguish between full screen and split screen 
    - bit 6 activates the menu
    Note that not all combinations may be implemented, but in this way we can keep track of the mode when switching between full screen and split screen
*/
/**************************************************************************/
enum K197screenMode {
  K197sc_normal = 0x01,   ///< equivalent to original K197
  K197sc_minmax = 0x02,   ///< add statistics but less annunciators 
  K197sc_FullScreenBitMask = 0x10, ///< full screen when set
  K197sc_MenuBitMask = 0x20,       ///< show menu when set
  K197sc_ScreenModeMask = 0x0f,    ///< Mask for mode bits
  K197sc_AttributesBitMask = 0xf0  ///< Mask for attribute bits
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
  K197screenMode screen_mode = (K197screenMode) (K197sc_normal
      | K197sc_FullScreenBitMask); ///< Keep track of how to display stuff...

  void updateNormalScreen();
  void updateMinMaxScreen();
  void updateSplitScreen();

  void setupMenus();

  byte logskip_counter = 0; ///< counter used when data logging, counts how
                            ///< many measurements are skipped
  void clearScreen();
  void setScreenMode(K197screenMode mode);
  /*!
     @brief  get the screen mode
     @return screen mode, it must be one of the displayXXX constants defined in
     class UImanager
  */
  K197screenMode getScreenMode() { return (K197screenMode) (screen_mode & K197sc_ScreenModeMask); };


public:
  UImanager(){}; ///< default constructor for the class
  void setup();

  /*!
     @brief  check if display is in full screen mode
     @return true if in full screen mode
  */
 bool isFullScreen() {return k197dev.isCal() || ( (screen_mode & K197sc_FullScreenBitMask) != 0x00);}
  /*!
     @brief  check if display is in split screen mode
     @return true if in split screen mode
  */
  bool isSplitScreen() {return !isFullScreen();}
  /*!
     @brief  chek if the options menu is visible
     @return true if the menu is visible
  */
  bool isMenuVisible() {return k197dev.isNotCal() && ( (screen_mode & K197sc_MenuBitMask) != 0x00);};
  /*!
     @brief  set full screen mode
     @details also clears the other attributes as we do not keep track of them
     @return true if the menu is visible
  */
  void showFullScreen() { 
    screen_mode = (K197screenMode) (screen_mode & K197sc_ScreenModeMask); 
    screen_mode = (K197screenMode) (screen_mode | K197sc_FullScreenBitMask); 
    clearScreen();
  };
  /*!
     @brief  show the option menu
     @details also clears the other attributes as we do not keep track of them
     @return true if the menu is visible
  */
  void showOptionsMenu() { 
    if (k197dev.isCal()) return;
    screen_mode = (K197screenMode) (screen_mode & K197sc_ScreenModeMask);
    screen_mode = (K197screenMode) (screen_mode | K197sc_MenuBitMask);
    clearScreen();
  };
  /*!
     @brief  show the option menu
     @details also clears the other attributes as we do not keep track of them
     @return true if the menu is visible
  */
  void showDebugLog() {
    if (k197dev.isCal()) return;
    // The debug log is shown in split mode if the menu is not active
    screen_mode = (K197screenMode) (screen_mode & K197sc_ScreenModeMask);
    clearScreen();
  }; 

  void updateDisplay();
  void updateBtStatus();

  void setContrast(uint8_t value);

  bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);

  void setLogging(bool yesno);
  bool isLogging();

  void logData();

  static const char *formatNumber(char buf[K197_MSG_SIZE], float f);

};

extern UImanager uiman;

#endif // UIMANAGER_H__
