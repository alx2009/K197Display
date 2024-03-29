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
  K197sc_normal = 0x01,                ///< equivalent to original K197
  K197sc_minmax = 0x02,                ///< add statistics but less annunciators
  K197sc_graph = 0x03,                 ///< show a graph
  K197sc_FullScreenBitMask = 0x10,     ///< full screen when set
  K197sc_MenuBitMask = 0x20,           ///< show menu when set
  K197sc_CursorsVisibleBitMask = 0x40, ///< show cursors when set + graph mode
  K197sc_activeCursorBitMask = 0x80,   ///< select active cursor
  K197sc_ScreenModeMask = 0x0f,        ///< Mask for mode bits
  K197sc_AttributesBitMask = 0xf0      ///< Mask for attribute bits
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
  static const u8g2_uint_t display_size_x =
      256; ///< constant, "doodle" x coordinate
  static const u8g2_uint_t display_size_y =
      64; ///< constant, "doodle" y coordinate
  static const u8g2_uint_t doodle_x_coord =
      display_size_x - 8; ///< constant, "doodle" x coordinate
  static const u8g2_uint_t doodle_y_coord =
      display_size_y - 12; ///< constant, "doodle" y coordinate

  static const char CURSOR_A = 'A'; ///< constant, identifies cursor A
  static const char CURSOR_B = 'B'; ///< constant, identifies cursor B
  static const char MARKER =
      '+'; ///< constant, identifies the latest sample in the graph

private:
  byte cursor_a = 60;  ///< Stores cursor A position
  byte cursor_b = 120; ///< Stores cursor B position

  bool hold_flag = false; ///< prevents changing hold mode at long press

  K197screenMode screen_mode =
      (K197screenMode)(K197sc_normal |
                       K197sc_FullScreenBitMask); ///< Keep track of how to
                                                  ///< display stuff...

  void displayDoodle(u8g2_uint_t x, u8g2_uint_t y, bool stepDoodle = true);
  void updateNormalScreen();
  void updateMinMaxScreen();
  void updateSplitScreen();
  void updateGraphScreen();
  void drawGraphScreenNormalPanel(u8g2_uint_t topln_x);
  void drawGraphScreenCursorPanel(u8g2_uint_t topln_x, u8g2_uint_t ax,
                                  u8g2_uint_t bx);
  void drawMarker(u8g2_uint_t x, u8g2_uint_t y,
                  char marker_type = UImanager::MARKER);

  void setupMenus();

  byte logskip_counter = 0; ///< counter used when data logging, counts how
                            ///< many measurements are skipped
  void clearScreen();

public:
  UImanager(){}; ///< default constructor for the class
  void setup();

  void setScreenMode(K197screenMode mode);
  /*!
     @brief  get the screen mode
     @return screen mode, (note that only the modes < 0x0f can be returned, the
     others are for internal use in class UImanager)
  */
  K197screenMode getScreenMode() {
    return (K197screenMode)(screen_mode & K197sc_ScreenModeMask);
  };

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
     @brief  chek if the screen is in graph mode
     @return true if the screen is in graph mode
  */
  bool isGraphMode() {
    return !isMenuVisible() &&
           ((screen_mode & K197sc_ScreenModeMask) == K197sc_graph);
  };
  /*!
     @brief  chek if the cursors are visible
     @return true if the cursors are visible
  */
  bool areCursorsVisible() {
    return (screen_mode & K197sc_CursorsVisibleBitMask) != 0x00;
  };
  /*!
     @brief  return the active cursor
     @return returns the active cursor (CURSOR_A or CURSOR_B)
  */
  char getActiveCursor() {
    if ((screen_mode & K197sc_activeCursorBitMask) == 0x00)
      return CURSOR_A;
    else
      return CURSOR_B;
  };

  /*!
     @brief  return the cursor position
     @param  which_cursor the requested cursor (CURSOR_A or CURSOR_B)
     @return returns the requested cursor position
  */
  byte getCursorPosition(char which_cursor) {
    return which_cursor == CURSOR_A ? cursor_a : cursor_b;
  };
  /*!
     @brief  set the cursor position
     @details: enforce the range 0 - k197_display_graph_type::x_size
     @param  which_cursor the cursor whose position shall be set (CURSOR_A or
     CURSOR_B)
     @param  new_position position shall be set
  */
  void setCursorPosition(char which_cursor, byte new_position) {
    if (new_position >= k197_display_graph_type::x_size)
      new_position = k197_display_graph_type::x_size - 1;
    if (which_cursor == CURSOR_A) {
      cursor_a = new_position;
    } else {
      cursor_b = new_position;
    }
  };

  /*!
     @brief  increment the active cursor position
     @details the cursor position will not be incremented past the maximum value
     (k197_display_graph_type::x_size-1) or less than zero
     @param increment the number to be added to the current position (use a
     negative increment to decrement)
  */
  void incrementCursor(int increment = 1) {
    bool isA = getActiveCursor() == CURSOR_A;
    byte oldvalue = isA ? cursor_a : cursor_b;
    byte newvalue = oldvalue + increment;
    if (increment > 0) { // make sure we increment within boundary
      if ((newvalue < oldvalue) ||
          (newvalue >= k197_display_graph_type::x_size))
        newvalue = k197_display_graph_type::x_size - 1;
    } else { // make sure we decrement
      if (newvalue > oldvalue)
        newvalue = 0;
    }
    if (isA)
      cursor_a = newvalue;
    else
      cursor_b = newvalue;
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
     @brief  toggle the cursor visibility
  */
  void toggleCursorsVisibility() {
    if (areCursorsVisible())
      screen_mode =
          (K197screenMode)(screen_mode & (~K197sc_CursorsVisibleBitMask));
    else
      screen_mode =
          (K197screenMode)(screen_mode | K197sc_CursorsVisibleBitMask);
  };

  /*!
     @brief  toggle the active cursor
  */
  void toggleActiveCursor() {
    if (getActiveCursor() == CURSOR_B)
      screen_mode =
          (K197screenMode)(screen_mode & (~K197sc_activeCursorBitMask));
    else
      screen_mode = (K197screenMode)(screen_mode | K197sc_activeCursorBitMask);
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

  void updateDisplay(bool stepDoodle = true);
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
      0x02ul; ///< the revision of this structure. Increment whenever the
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
      uint16_t value = 0x00; ///< allows access to all the flags in the
                             ///< union as one 16 bit unsigned integer
      struct {
        bool additionalModes : 1;      ///< store menu option value
        bool reassignStoRcl : 1;       ///< store menu option value
        bool showDoodle : 1;           ///< store menu option value
        bool logEnable : 1;            ///< store menu option value
        bool logSplitUnit : 1;         ///< store menu option value
        bool logTimestamp : 1;         ///< store menu option value
        bool logTamb : 1;              ///< store menu option value
        bool logStat : 1;              ///< store menu option value
        bool gr_yscale_show0 : 1;      ///< store menu option value
        bool unused_no_1 : 1;          ///< backward compatibility
        bool gr_xscale_autosample : 1; ///< store menu option value
        bool logError : 1;             ///< store menu option value
        bool unused_no_2 : 1;          ///< backward compatibility
        bool gr_yscale_full_range : 1; ///< store menu option value
      };
    } __attribute__((packed)); ///<
  }; ///< Structure designed to pack a number of flags into two bytes

  struct byte_options_struct {
    byte contrastCtrl;   ///< store menu option value
    byte logSkip;        ///< store menu option value
    byte logStatSamples; ///< store menu option value
    byte opt_gr_type;    ///< store menu option value
    byte opt_gr_yscale;  ///< store menu option value
    byte gr_sample_time; ///< store menu option value
    byte cursor_a;       ///< store cursor A position
    byte cursor_b;       ///< store cursor B position
  }; ///< Structure designed to collect all byte optipons together

  bool_options_struct bool_options; ///< store all bool options
  byte_options_struct byte_options; ///< store all byte options
  K197screenMode screenMode;        ///< store screen mode

  void copyFromUI();
  void copyToUI(bool restore_screen_mode = false);

public:
  static bool store_to_EEPROM();
  static bool retrieve_from_EEPROM(bool restore_screen_mode = false);
};
extern UImanager uiman;

#endif // UIMANAGER_H__
