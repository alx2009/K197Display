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
    @details the lower 4 bits control the screen mode, the higher 4 bit controls
   other attributes:
    - bit 5 is used to distinguish between full screen and split screen
    - bit 6 activates the menu
    Note that not all combinations may be implemented, but in this way we can
   keep track of the mode when switching between full screen and split screen
*/
/**************************************************************************/
enum K197screenMode {
  K197sc_normal = 0x01,            ///< equivalent to original K197
  K197sc_minmax = 0x02,            ///< add statistics but less annunciators
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
public:
  unsigned long looptimerMax =
      0UL; ///< used to keep track of the time spent in loop

private:
  bool show_volt = false; ///< Show voltages if true (not currently used)
  bool show_temp = false; ///< Show temperature if true  (not currently used)
  K197screenMode screen_mode =
      (K197screenMode)(K197sc_normal |
                       K197sc_FullScreenBitMask); ///< Keep track of how to
                                                  ///< display stuff...

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
  K197screenMode getScreenMode() {
    return (K197screenMode)(screen_mode & K197sc_ScreenModeMask);
  };

public:
  UImanager(){}; ///< default constructor for the class
  void setup();

  /*!
     @brief  check if display is in full screen mode
     @return true if in full screen mode
  */
  bool isFullScreen() {
    return k197dev.isCal() ||
           ((screen_mode & K197sc_FullScreenBitMask) != 0x00);
  }
  /*!
     @brief  check if display is in split screen mode
     @return true if in split screen mode
  */
  bool isSplitScreen() { return !isFullScreen(); }
  /*!
     @brief  chek if the options menu is visible
     @return true if the menu is visible
  */
  bool isMenuVisible() {
    return k197dev.isNotCal() && ((screen_mode & K197sc_MenuBitMask) != 0x00);
  };
  /*!
     @brief  set full screen mode
     @details also clears the other attributes
  */
  void showFullScreen() {
    screen_mode = (K197screenMode)(screen_mode & K197sc_ScreenModeMask);
    screen_mode = (K197screenMode)(screen_mode | K197sc_FullScreenBitMask);
    clearScreen();
  };
  /*!
     @brief  show the option menu
     @details also clears the other attributes
  */
  void showOptionsMenu() {
    if (k197dev.isCal())
      return;
    screen_mode = (K197screenMode)(screen_mode & K197sc_ScreenModeMask);
    screen_mode = (K197screenMode)(screen_mode | K197sc_MenuBitMask);
    clearScreen();
  };
  /*!
     @brief  show the option menu
     @details also clears the other attributes
  */
  void showDebugLog() {
    if (k197dev.isCal())
      return;
    // The debug log is shown in split mode if the menu is not active
    screen_mode = (K197screenMode)(screen_mode & K197sc_ScreenModeMask);
    clearScreen();
  };

  void updateDisplay();
  void updateBtStatus();

  void setContrast(uint8_t value);

  bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);

  void setLogging(bool yesno);
  bool isLogging();

  void logData();

  static const char *formatNumber(char buf[K197_RAW_MSG_SIZE + 1], float f);
};

/**************************************************************************/
/*!
    @brief  structure used to store and retrieve the configuration to and from
   the EEPROM
*/
/**************************************************************************/
struct permadata {
private:
  static const unsigned long magicNumberExpected =
      0x1a2b3c4dul; ///< This is the magic number telling us if the EEPROM
                    ///< contains data
  static const unsigned long revisionExpected =
      0x01ul; ///< the revision of this structure. Increment whenever the
              ///< structure is modified

  // structure identity
  unsigned long magicNumber =
      magicNumberExpected; ///< this is the magic number stored/retrieved from
                           ///< the EEPROM
  unsigned long revision =
      revisionExpected; ///< this is the revision stored/retrieved from the
                        ///< EEPROM

  struct bool_options_struct {
    /*!
       @brief  A union is used to simplify initialization of the flags
       @return Not really a return type, this attribute will save some RAM
    */
    union {
      unsigned char value = 0x00; ///< allows access to all the flags in the
                                  ///< union as one unsigned char
      struct {
        bool additionalModes : 1; ///< store menu option value
        bool reassignStoRcl : 1;  ///< store menu option value
        bool logEnable : 1;       ///< store menu option value
        bool logSplitUnit : 1;    ///< store menu option value
        bool logTimestamp : 1;    ///< store menu option value
        bool logTamb : 1;         ///< store menu option value
        bool logStat : 1;         ///< store menu option value
      };
    } __attribute__((packed)); ///<
  }; ///< Structure designed to pack a number of flags into one byte

  struct byte_options_struct {
    byte contrastCtrl;   ///< store menu option value
    byte logSkip;        ///< store menu option value
    byte logStatSamples; ///< store menu option value
  }; ///< Structure designed to collect all byte optipons together

  bool_options_struct bool_options; ///< store all bool options
  byte_options_struct byte_options; ///< store all byte options

  void copyFromUI();
  void copyToUI();

public:
  static void store_to_EEPROM();
  static void retrieve_from_EEPROM();
};
extern UImanager uiman;

#endif // UIMANAGER_H__
