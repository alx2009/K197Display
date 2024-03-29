/**************************************************************************/
/*!
  @file     UImanager.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the UImanager class, see UImanager.h for the class
  definition uses u8g2 library to control the SSD1322 display module via HW SPI

  Because of the size of the display, the 16 bit mode of u8g2 MUST be used
  This is not the default for the AVR architecture (despite the new chips having
  more than enough RAM for the job), so one header file in the u8g2 must be
  edited to uncomment the "#define U8G2_16BIT" statement.  See also
  https://github.com/olikraus/u8g2/blob/master/doc/faq.txt and search for
  U8G2_16BIT

  If 16 bit mode is not active we will trigger a compilation error as otherwise
  it would be rather difficult to figure out why it does not diplay correctly...

*/
/**************************************************************************/

/*=================Font Usage=======================
  Fonts used in the application:
    u8g2_font_5x7_mr  ==> terminal window, local T, Bluetooth status
    u8g2_font_6x12_mr ==> BAT, all menus
    u8g2_font_8x13_mr ==> AUTO, REL, dB, STO, RCL, Cal, RMT, most annunciators
  in split screen
    u8g2_font_9x15_m_symbols ==> meas. unit, AC, Split screen:
    AC u8g2_font_inr30_mr ==> main message u8g2_font_inr16_mr ==> main
  message in minmax mode

    mr ==> monospace restricted
    m ==> monospace
 */

#include <Arduino.h>
#include <U8g2lib.h>

#define DEFAULT_CONTRAST                                                       \
  0x00 ///< defines the default contrast. Can be changed via serial port

#ifndef U8G2_16BIT // Trigger an error if u8g2 is not in 16 bit mode, as
                   // explained above
#error you MUST define U8G2_16BIT in u8g2.h ! See comment at the top of UImanager.cpp
#endif // U8G2_16BIT

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include "UImenu.h"
UImenu UImainMenu(130, true); ///< the main menu for this application
UImenu UIlogMenu(130);        ///< the submenu to set logging options
UImenu UIgraphMenu(130);      ///< the submenu to set graph options

#include "BTmanager.h"
#include "K197PushButtons.h"
#include "UImanager.h"
#include "debugUtil.h"
#include "dxUtil.h"
#include "pinout.h"

UImanager uiman; ///< defines the UImanager instance to use in the application

/*!
    @brief  Constructor, see
   https://github.com/olikraus/u8g2/wiki/setup_tutorial

   @return no return value
*/
// At the moment we do not use the RESET pin. It does seem to work correctly
// without.
#ifdef OLED_DC // 4Wire SPI
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI
u8g2(U8G2_R0, OLED_SS,
     OLED_DC /*,reset_pin*/); ///< u8g2 object. See pinout.h for pin definition.
#else                         // 3Wire SPI
U8G2_SSD1322_NHD_256X64_F_3W_HW_SPI
u8g2(U8G2_R0, OLED_SS
     /*,reset_pin*/); ///< u8g2 object. See pinout.h for pin definition.
#endif

#define U8LOG_WIDTH 25                            ///< Size of the log window
#define U8LOG_HEIGHT 5                            ///< Height of the log window
uint8_t u8log_buffer[U8LOG_WIDTH * U8LOG_HEIGHT]; ///< buffer for the log window
U8G2LOG u8g2log;                                  ///< the log window

// ***************************************************************************************
// Graphics utility functions
// ***************************************************************************************
/*!
    @brief utility function: draw an horizontal dotted line
    @param x0 Coordinate x of the starting point
    @param y0 Coordinate y (only one y coord needed, line is horizontal)
    @param x1 Coordinate x of the ending point
    @param dotsize the size of the "dot" (default 10)
    @param dotDistance the distance between "dots" (default 20)
*/
void drawDottedHLine(uint16_t x0, uint16_t y0, uint16_t x1,
                     uint16_t dotsize = 10, uint16_t dotDistance = 20) {
  if (dotsize == 0)
    dotsize = 1;
  if (dotDistance == 0)
    dotDistance = dotsize * 5;
  uint16_t xdot;
  do {
    xdot = x0 + dotsize;
    if (xdot > x1)
      xdot = x1;
    u8g2.drawLine(x0, y0, xdot, y0);
    x0 += dotDistance;
  } while (x0 <= x1);
}

// ***************************************************************************************
// UI Setup
// ***************************************************************************************

/*!
    @brief  this function is intended to include any drawing command that may be
   needed the very first time the display is used

    Called from setup, it should not be called from elsewhere
*/
void setup_draw(void) {
  u8g2.setFont(
      u8g2_font_inr30_mr); // width =25  points (7 characters=175 points)
  u8g2.setFontMode(0);
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontRefHeightExtendedText();
  u8g2.setFontDirection(0);

  CHECK_FREE_STACK();
}

/*!
    @brief  setup the display and clears the screen. Must be called before any
   other member function
*/
void UImanager::setup() {
  pinMode(OLED_MOSI, OUTPUT); // Needed to work around a bug in the micro or
                              // dxCore with certain swap options
  bool pmap = SPI.swap(OLED_SPI_SWAP_OPTION); // See pinout.h for pin definition
  if (!pmap) {
    DebugOut.println(F("SPI map!"));
    DebugOut.flush();
    while (true)
      ;
  }
  u8g2.setBusClock(12000000);
  u8g2.begin();
  setContrast(DEFAULT_CONTRAST);
  u8g2.enableUTF8Print();
  u8g2log.begin(U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);

  u8g2.clearBuffer();
  setup_draw();
  u8g2.sendBuffer();

  setupMenus();
}

// ***************************************************************************************
// Display update
// ***************************************************************************************

/*!
    @brief  update the display.
    @details This function will not cause the K197device object to read new
   data, it will use whatever data is already stored in the object.

    Therefore, it should not be called before the first data has been received
   from the K197/197A

    If you want to add an initial scren/text, the best way would be to add this
   to setup();
   @param stepDoodle if true and the doodle animation is enabled, the animation
   is updated
*/
void UImanager::updateDisplay(bool stepDoodle) {
  u8g2.clearBuffer(); // Clear display area

  if (k197dev.isNotCal() && isSplitScreen())
    updateSplitScreen();
  else if (k197dev.isCal() || (getScreenMode() == K197sc_normal))
    updateNormalScreen();
  else if (getScreenMode() == K197sc_minmax)
    updateMinMaxScreen();
  else
    updateGraphScreen();

  displayDoodle(doodle_x_coord, doodle_y_coord, stepDoodle);
  u8g2.sendBuffer();
  CHECK_FREE_STACK();
}

/*!
    @brief  update the display, used when in debug and other modes with split
   screen.
   @details this screen always show the current measured value, even if we are
   in hold mode This is by design so that it is possible to see the current
   value without exiting the hold mode
*/
void UImanager::updateSplitScreen() {
  // temporary buffer used for number formatting
  char buf[K197_RAW_MSG_SIZE + 1]; // +1 needed to account for '.'

  u8g2_uint_t x = 140;
  u8g2_uint_t y = 5;
  u8g2.setFont(u8g2_font_8x13_mr);
  u8g2.setCursor(x, y);
  if (k197dev.isAuto())
    u8g2.print(F("AUTO "));
  if (k197dev.isBAT())
    u8g2.print(F("BAT "));
  if (k197dev.isREL())
    u8g2.print(F("REL "));
  if (k197dev.isCal())
    u8g2.print(F("Cal   "));

  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  if (k197dev.isNumeric()) {
    u8g2.print(formatNumber(buf, k197dev.getValue()));
  } else
    u8g2.print(k197dev.getRawMessage());
  u8g2.print(CH_SPACE);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197dev.getUnit(true));
  y += u8g2.getMaxCharHeight();
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  if (k197dev.isAC())
    u8g2.print(F(" AC   "));

  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_8x13_mr);
  if (k197dev.isSTO())
    u8g2.print(F("STO "));
  if (k197dev.isRCL())
    u8g2.print(F("RCL "));
  if (k197dev.isRMT())
    u8g2.print(F("RMT   "));

  if (isSplitScreen() && !isMenuVisible()) { // Show the debug log
    u8g2.setFont(u8g2_font_5x7_mr); // set the font for the terminal window
    u8g2.drawLog(0, 0, u8g2log);    // draw the terminal window on the display
  } else { // For all other modes we show the settings menu when in split mode
    UIwindow::getcurrentWindow()->draw(&u8g2, 0, 10);
  }
  CHECK_FREE_STACK();
}

/*!
    @brief  update the display, used when in normal screen mode. See also
   updateDisplay().
*/
void UImanager::updateNormalScreen() {
  u8g2.setFont(
      u8g2_font_inr30_mr); // width =25  points (7 characters=175 points)
  const unsigned int xraw = 49;
  const unsigned int yraw = 15;
  const unsigned int dpsz_x = 3;  // decimal point size in x direction
  const unsigned int dpsz_y = 3;  // decimal point size in y direction
  const unsigned int dphsz_x = 2; // decimal point "half size" in x direction
  const unsigned int dphsz_y = 2; // decimal point "half size" in y direction

  bool hold = k197dev.getDisplayHold();
  u8g2.drawStr(xraw, yraw, k197dev.getRawMessage(hold));
  for (byte i = 1; i <= 7; i++) {
    if (k197dev.isDecPointOn(i, hold)) {
      u8g2.drawBox(xraw + i * u8g2.getMaxCharWidth() - dphsz_x,
                   yraw + u8g2.getAscent() - dphsz_y, dpsz_x, dpsz_y);
    }
  }
  // set the unit
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  const unsigned int xunit = 229;
  const unsigned int yunit = 20;
  u8g2.setCursor(xunit, yunit);
  u8g2.print(k197dev.getUnit(false, hold));

  // set the AC/DC indicator
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  const unsigned int xac = xraw + 3;
  const unsigned int yac = 40;
  u8g2.setCursor(xac, yac);
  if (k197dev.isAC(hold))
    u8g2.print(F("AC"));

  // set the other announciators
  u8g2.setFont(u8g2_font_8x13_mr);
  unsigned int x = 0;
  unsigned int y = 5;
  u8g2.setCursor(x, y);
  if (k197dev.isAuto())
    u8g2.print(F("AUTO"));
  x = u8g2.tx;
  x += u8g2.getMaxCharWidth() * 2;
  u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setCursor(x, y);
  if (k197dev.isBAT())
    u8g2.print(F("BAT"));

  u8g2.setFont(u8g2_font_8x13_mr);
  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);

  if (k197dev.isREL(hold))
    u8g2.print(F("REL"));
  x += u8g2.getMaxCharWidth() * 3;
  x += (u8g2.getMaxCharWidth() / 2);
  u8g2.setCursor(x, y);
  if (k197dev.isdB(hold))
    u8g2.print(F("dB"));

  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);
  if (hold) {
    u8g2.print(F("HOLD"));
    // not enough space, go to the next line
    x = 0;
    y = u8g2.tx + u8g2.getMaxCharHeight();
    u8g2.setCursor(x, y);
  }
  if (k197dev.isSTO()) {
    u8g2.print(F("STO"));
    x = u8g2.tx + (u8g2.getMaxCharWidth() / 2);
  } else {
    x = u8g2.tx + (u8g2.getMaxCharWidth() * 7 / 2);
  }
  if (!hold) { // then move to Next line now
    x = 0;
    y += u8g2.getMaxCharHeight();
  }
  u8g2.setCursor(x, y);
  if (k197dev.isRCL())
    u8g2.print(F("RCL"));

  x = 229;
  y = 0;
  u8g2.setCursor(x, y);
  if (k197dev.isCal())
    u8g2.print(F("Cal"));

  y += u8g2.getMaxCharHeight() * 3;
  u8g2.setCursor(x, y);
  if (k197dev.isRMT())
    u8g2.print(F("RMT"));

  x = 140;
  y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (k197dev.isTKModeActive(hold)) { // Display local temperature
    char buf[K197_RAW_MSG_SIZE + 1];
    dtostrf(k197dev.getTColdJunction(hold), K197_RAW_MSG_SIZE, 2, buf);
    u8g2.print(buf);
    u8g2.print(k197dev.getUnit(false, hold));
  }

  updateBtStatus();
  CHECK_FREE_STACK();
}

/*!
    @brief  update the display, used when in minmax screen mode. See also
   updateDisplay().
*/
void UImanager::updateMinMaxScreen() {
  // temporary buffer used for number formatting
  char buf[K197_RAW_MSG_SIZE + 1]; // +1 needed to account for '.'

  u8g2.setFont(
      u8g2_font_inr16_mr);        // width =25  points (7 characters=175 points)
  const unsigned int xraw = 130;  // x coordinate for raw_msg
  const unsigned int yraw = 15;   // y coordinate for raw_msg
  const unsigned int dpsz_x = 3;  // decimal point size in x direction
  const unsigned int dpsz_y = 3;  // decimal point size in y direction
  const unsigned int dphsz_x = 2; // decimal point "half size" in x direction
  const unsigned int dphsz_y = 2; // decimal point "half size" in y direction
  const unsigned int xstat = 28;  // x coordinate for the statistics
  const unsigned int ystat = 5;   // y coordinate for the statistics (1st line)
  const unsigned int xunit = 229; // x coordinate for the unit
  const unsigned int yunit = 20;  // y coordinate for the unit

  bool hold = k197dev.getDisplayHold();

  u8g2.drawStr(xraw, yraw, k197dev.getRawMessage(hold));
  for (byte i = 1; i <= 7; i++) {
    if (k197dev.isDecPointOn(i, hold)) {
      u8g2.drawBox(xraw + i * u8g2.getMaxCharWidth() - dphsz_x,
                   yraw + u8g2.getAscent() - dphsz_y, dpsz_x, dpsz_y);
    }
  }

  // set the unit
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.setCursor(xunit, yunit);
  u8g2.print(k197dev.getUnit(true, hold));

  // set the AC/DC indicator
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  int char_height_9x15 = u8g2.getMaxCharHeight(); // needed later on
  const unsigned int xac = 229;
  const unsigned int yac = 35;
  u8g2.setCursor(xac, yac);
  if (k197dev.isAC(hold))
    u8g2.print(F("AC"));

  // set the REL announciator
  u8g2.setFont(u8g2_font_6x12_mr);
  unsigned int x = 0;
  unsigned int y = 5;
  u8g2.setCursor(x, y);
  if (k197dev.isREL(hold))
    u8g2.print(F("REL"));

  // Write Min/average/Max labels
  u8g2.setFont(u8g2_font_5x7_mr);
  x = xstat;
  y = ystat;
  u8g2.setCursor(x, y);
  u8g2.print(F("Max "));
  y += char_height_9x15;
  u8g2.setCursor(x, y);
  u8g2.print(F("Avg "));
  y += char_height_9x15;
  u8g2.setCursor(x, y);
  u8g2.print(F("Min "));

  u8g2.setFont(u8g2_font_9x15_m_symbols);
  x = u8g2.tx;
  y = 3;
  u8g2.setCursor(x, y);
  u8g2.print(formatNumber(buf, k197dev.getMax(hold)));
  y += char_height_9x15;
  u8g2.setCursor(x, y);
  u8g2.print(formatNumber(buf, k197dev.getAverage(hold)));
  y += char_height_9x15;
  u8g2.setCursor(x, y);
  u8g2.print(formatNumber(buf, k197dev.getMin(hold)));

  x = 170;
  y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (k197dev.isTKModeActive(hold)) { // Display local temperature
    char buf[K197_RAW_MSG_SIZE + 1];
    dtostrf(k197dev.getTColdJunction(hold), K197_RAW_MSG_SIZE, 2, buf);
    u8g2.print(buf);
    u8g2.print(k197dev.getUnit(false, hold));
  }
  u8g2.setFont(u8g2_font_8x13_mr);
  x = 0;
  y = 5 + u8g2.getMaxCharHeight() * 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (hold)
    u8g2.print(F("HOLD"));

  // set the other announciators
  x = 0;
  y = 63 - u8g2.getMaxCharHeight() - 3;
  u8g2.setCursor(x, y);
  if (k197dev.isSTO())
    u8g2.print(F("STO "));
  if (k197dev.isRCL())
    u8g2.print(F("RCL "));
  if (k197dev.isBAT())
    u8g2.print(F("BAT "));
  if (k197dev.isRMT())
    u8g2.print(F("RMT "));
  if (k197dev.isCal())
    u8g2.print(F("Cal "));
  if (k197dev.isOvrange())
    u8g2.print(F("ovRange "));
  if (k197dev.isAuto())
    u8g2.print(F("AUTO"));

  CHECK_FREE_STACK();
}

/*!
      @brief display the BT module status (detected or not detected)

      @param present true if a BT module is detected, false otherwise
      @param connected true if a BT connection is detected, false otherwise
*/
void UImanager::updateBtStatus() {
  unsigned int x = 95;
  unsigned int y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (BTman.isPresent()) {
    u8g2.print(F("bt "));
  }
  x += u8g2.getStrWidth("   ");
  u8g2.setCursor(x, y);
  bool connected = BTman.isConnected();
  if (connected && isLogging()) {
    u8g2.print(F("<=>"));
  } else if (connected) {
    u8g2.print(F("<->"));
  }
  CHECK_FREE_STACK();
}

// ***************************************************************************************
// Screen mode & clear screen
// ***************************************************************************************

/*!
      @brief set the screen mode

   @details  Three modes are defined: normal mode, menu mode and debug mode.
   In debug and menu mode the measurements are displays on the right of the
   screen (split screen), while the left part is reserved for debug messages and
   menu items respectively. Normal mode is a full screen mode equivalent to the
   original K197 display. As the name suggest debug mode is intended for
   debugging the code that interacts with the serial port/bluetooth module
   itself, so that Serial cannot be used.

      @param mode the new display mode. Only the modes defined in K197screenMode
   < 0x0f are valid. Using an invalid mode will have no effect
*/
void UImanager::setScreenMode(K197screenMode mode) {
  if ((mode <= 0) || (mode > K197sc_graph))
    return;
  screen_mode =
      (K197screenMode)(screen_mode &
                       K197sc_AttributesBitMask); // clear current screen mode
  screen_mode = (K197screenMode)(screen_mode | mode); // set the mode bits to
                                                      // enter the new mode
  clearScreen();
}

/*!
  @brief clear the display
  @details This is done automatically when the screen mode changes, there should
  be no need to call this function elsewhere
*/
void UImanager::clearScreen() {
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  CHECK_FREE_STACK();
}

// ***************************************************************************************
//  Message box definitions
// ***************************************************************************************

DEF_MESSAGE_BOX(EEPROM_save_msg_box, 100,
                "config saved"); ///< Config saved message box
DEF_MESSAGE_BOX(EEPROM_reload_msg_box, 100,
                "config reloaded"); ///< Config reloaded msg. box
DEF_MESSAGE_BOX(ERROR_msg_box, 100, "Error (see log)"); ///< Error message box

// ***************************************************************************************
//  Menu definition/handling
// ***************************************************************************************

DEF_MENU_CLOSE(closeMenu, 15,
               "< Back"); ///< Menu close action (used in multiple menus)
DEF_MENU_ACTION(
    exitMenu, 15, "Exit",
    uiman.showFullScreen();); ///< Menu close action (used in multiple menus)

// Main menu
DEF_MENU_SEPARATOR(mainSeparator0, 15, "< Options >");       ///< Menu separator
DEF_MENU_BOOL(additionalModes, 15, "Extra Modes");           ///< Menu input
DEF_MENU_BOOL(reassignStoRcl, 15, "Reassign STO/RCL");       ///< Menu input
DEF_MENU_OPEN(btDatalog, 15, "Data logger >>>", &UIlogMenu); ///< Open submenu
DEF_MENU_OPEN(btGraphOpt, 15, "Graph opt. >>>",
              &UIgraphMenu); ///< Open submenu
DEF_MENU_BOOL_ACT(showDoodle, 15, "Doodle",
                  if (!getValue()) u8g2.drawGlyph(UImanager::doodle_x_coord,
                                                  UImanager::doodle_y_coord,
                                                  CH_SPACE);); ///< Menu input
DEF_MENU_BYTE_ACT(contrastCtrl, 15, "Contrast",
                  u8g2.setContrast(getValue());); ///< set contrast
DEF_MENU_ACTION(
    saveSettings, 15, "Save settings",
    if (permadata::store_to_EEPROM()) { EEPROM_save_msg_box.show(); } else {
      ERROR_msg_box.show();
    }); ///< save config to EEPROM and show result
DEF_MENU_ACTION(
    reloadSettings, 15, "Reload settings",
    if (permadata::retrieve_from_EEPROM()) {
      EEPROM_reload_msg_box.show();
    } else {
      ERROR_msg_box.show();
    }); ///< load config from EEPROM and show result
DEF_MENU_ACTION(openLog, 15, "Show log",
                REPORT_FREE_STACK();
                DebugOut.println(); uiman.showDebugLog();); ///< show debug log
DEF_MENU_ACTION(resetAVR, 15, "RESET",
                _PROTECTED_WRITE(RSTCTRL.SWRR, 1);); ///< Menu input

UImenuItem *mainMenuItems[] = {
    &mainSeparator0, &additionalModes, &reassignStoRcl,
    &btDatalog,      &btGraphOpt,      &showDoodle,
    &contrastCtrl,   &exitMenu,        &saveSettings,
    &reloadSettings, &openLog,         &resetAVR}; ///< Root menu items

// Logging/statistics menu
DEF_MENU_SEPARATOR(logSeparator0, 15, "< BT Datalogging >"); ///< Menu separator
DEF_MENU_BOOL(logEnable, 15, "Enabled");                     ///< Menu input
DEF_MENU_BYTE(logSkip, 15, "Samples to skip");               ///< Menu input
DEF_MENU_BOOL(logSplitUnit, 15, "Split unit");               ///< Menu input
DEF_MENU_BOOL(logTimestamp, 15, "Log tstamp");               ///< Menu input
DEF_MENU_BOOL(logTamb, 15, "Incl. Tamb");                    ///< Menu input
DEF_MENU_BOOL(logStat, 15, "Incl. Statistics");              ///< Menu input
DEF_MENU_BOOL(logError, 15, "Log errors");                   ///< Menu input
DEF_MENU_SEPARATOR(logSeparator1, 15, "< Statistics >");     ///< Menu separator
DEF_MENU_BYTE_ACT(logStatSamples, 15, "Num. Samples",
                  k197dev.setNsamples(getValue());); ///< Menu input

UImenuItem *logMenuItems[] = {
    &logSeparator0, &logEnable, &logSkip,  &logSplitUnit,  &logTimestamp,
    &logTamb,       &logStat,   &logError, &logSeparator1, &logStatSamples,
    &closeMenu,     &exitMenu}; ///< Datalog menu items

// Graph menu
DEF_MENU_SEPARATOR(graphSeparator0, 15,
                   "< Graph options >"); ///< Menu separator
DEF_MENU_OPTION(opt_gr_type_lines, OPT_GRAPH_TYPE_LINES, 0,
                "Lines"); ///< Menu input
DEF_MENU_OPTION(opt_gr_type_dots, OPT_GRAPH_TYPE_DOTS, 1,
                "Dots"); ///< Menu input
DEF_MENU_OPTION_INPUT(opt_gr_type, 15, "Graph type", OPT(opt_gr_type_lines),
                      OPT(opt_gr_type_dots)); ///< Menu input

DEF_MENU_SEPARATOR(graphSeparator1, 15, "< Y axis >"); ///< Menu separator
DEF_MENU_BOOL_ACT(gr_yscale_full_range, 15, "Full range",
                  k197dev.setGraphFullRange(getValue());); ///< Menu input
BIND_MENU_OPTION(opt_gr_yscale_max, k197graph_yscale_zoom,
                 "zoom"); ///< Menu input
BIND_MENU_OPTION(opt_gr_yscale_zero, k197graph_yscale_zero,
                 "Incl. 0"); ///< Menu input
BIND_MENU_OPTION(opt_gr_yscale_prefsym, k197graph_yscale_prefsym,
                 "Symmetric"); ///< Menu input
BIND_MENU_OPTION(opt_gr_yscale_0sym, k197graph_yscale_0sym,
                 "0+symm"); ///< Menu input
BIND_MENU_OPTION(opt_gr_yscale_forcesym, k197graph_yscale_forcesym,
                 "Force symm."); ///< Menu input
BIND_MENU_OPTION(opt_gr_yscale_0forcesym, k197graph_yscale_0forcesym,
                 "0+force symm."); ///< Menu input
DEF_MENU_ENUM_INPUT(k197graph_yscale_opt, opt_gr_yscale, 15, "Y axis",
                    OPT(opt_gr_yscale_max), OPT(opt_gr_yscale_zero),
                    OPT(opt_gr_yscale_prefsym), OPT(opt_gr_yscale_0sym),
                    OPT(opt_gr_yscale_forcesym),
                    OPT(opt_gr_yscale_0forcesym)); ///< Menu input

DEF_MENU_BOOL(gr_yscale_show0, 15, "Show y=0"); ///< Menu input

DEF_MENU_SEPARATOR(graphSeparator2, 15, "< X axis >"); ///< Menu separator
DEF_MENU_BOOL_ACT(gr_xscale_autosample, 15, "Auto sample",
                  k197dev.setAutosample(getValue());); ///< Menu input
DEF_MENU_BYTE_SETGET(gr_sample_time, 15, "Sample time (s)",
                     k197dev.setGraphPeriod(newValue);
                     ,
                     return k197dev.getGraphPeriod();); ///< Menu input

UImenuItem *graphMenuItems[] =
    {&graphSeparator0, &opt_gr_type,
     &graphSeparator1, &gr_yscale_full_range,
     &opt_gr_yscale,   &gr_yscale_show0,
     &graphSeparator2, &gr_xscale_autosample,
     &gr_sample_time,  &closeMenu,
     &exitMenu}; ///< Collects all items in the graph menu

/*!
      @brief set the display contrast

      @details in addition to setting the contrast value, this method takes care
   of keeping the contrast menu item in synch

      @param value contrast value (0 to 255)
*/
void UImanager::setContrast(uint8_t value) {
  u8g2.setContrast(value);
  contrastCtrl.setValue(value);
  CHECK_FREE_STACK();
}

/*!
      @brief  setup the menu
      @details this method setup all the menus. It must be called before the
   menu can be displayed.
*/
void UImanager::setupMenus() {
  additionalModes.setValue(true);
  reassignStoRcl.setValue(true);
  showDoodle.setValue(true);
  UImainMenu.items = mainMenuItems;
  UImainMenu.num_items = sizeof(mainMenuItems) / sizeof(UImenuItem *);
  UImainMenu.selectFirstItem();

  logSkip.setValue(0);
  logSplitUnit.setValue(false);
  logTimestamp.setValue(true);
  logTamb.setValue(true);
  logStatSamples.setValue(k197dev.getNsamples());
  UIlogMenu.items = logMenuItems;
  UIlogMenu.num_items = sizeof(logMenuItems) / sizeof(UImenuItem *);
  UIlogMenu.selectFirstItem();
  UIgraphMenu.items = graphMenuItems;
  UIgraphMenu.num_items = sizeof(graphMenuItems) / sizeof(UImenuItem *);
  UIgraphMenu.selectFirstItem();

  gr_yscale_full_range.setValue(true);
  gr_yscale_full_range.change();
  gr_xscale_autosample.setValue(k197dev.getAutosample());

  permadata::retrieve_from_EEPROM(true);
}

// ***************************************************************************************
//  Logging
// ***************************************************************************************

/*!
      @brief  set data logging to Serial
      @param yesno true to enabl, false to disable
*/
void UImanager::setLogging(bool yesno) {
  if (!yesno)
    logskip_counter = 0;
  logEnable.setValue(yesno);
  CHECK_FREE_STACK();
}

/*!
      @brief  query data logging to Serial
      @return returns true if logging is active
*/
bool UImanager::isLogging() { return logEnable.getValue(); }

/*!
      @brief  Utility function, print a ";" if the option logSplit is active,
   otherwise a space
      @details the options controls how the unit is handled when data is
   imported into a spreadsheet
*/
inline void logU2U() {
  if (logSplitUnit.getValue())
    Serial.print(F(" ;"));
  else
    Serial.print(CH_SPACE);
}

/*!
      @brief  format a number
      @details format a float so that it has the right lenght and the maximum
   number of decimal digitas allowed in the available display space
      @param buf a buffer where the formatted string should be written (size>=9)
      @param f the number to be formatted
      @return a nul terminated char array with the formatted number (same as
   buf)
*/
const char *UImanager::formatNumber(char *buf, float f) {
  if (f > 999999.0)
    f = 999999.0;
  else if (f < -999999.0)
    f = -999999.0;
  float f_abs = abs(f);
  int ndec = 0;
  if (f_abs <= 9.99999)
    ndec = 5;
  else if (f_abs <= 99.9999)
    ndec = 4;
  else if (f_abs <= 999.999)
    ndec = 3;
  else if (f_abs <= 9999.99)
    ndec = 2;
  else if (f_abs <= 99999.9)
    ndec = 1;
  // else we leave ndec=0
  return dtostrf(f, K197_RAW_MSG_SIZE, ndec, buf);
}

/*!
    @brief  data logging to Serial
    @details does the actual data logging when called. It has no effect if
   datalogging is disabled or in no connection has been detected
*/
void UImanager::logData() {
  // temporary buffer used for number formatting
  char buf[K197_RAW_MSG_SIZE + 1]; // +1 needed to account for '.'

  if (k197dev.isCal()) // No logging while in Cal mode
    return;
  if ((!logEnable.getValue()) || (!BTman.validconnection()))
    return;
  if (!k197dev.isNumeric()) {
    if (!logError.getValue())
      return;
  }

  if (logskip_counter < logSkip.getValue()) {
    logskip_counter++;
    return;
  }
  logskip_counter = 0;
  if (logTimestamp.getValue()) {
    Serial.print(millis());
    logU2U();
    Serial.print(F(" ms; "));
  }
  if (k197dev.isNumeric()) {
    Serial.print(formatNumber(buf, k197dev.getValue()));
  } else
    Serial.print(k197dev.getRawMessage());
  logU2U();
  const __FlashStringHelper *unit = k197dev.getUnit(true);
  Serial.print(unit);
  if (k197dev.isAC())
    Serial.print(F(" AC"));
  if (k197dev.isTKModeActive() && logTamb.getValue()) {
    Serial.print(F("; "));
    Serial.print(k197dev.getTColdJunction());
    logU2U();
    Serial.print(unit);
  }
  if (logStat.getValue()) {
    Serial.print(F("; "));
    Serial.print(formatNumber(buf, k197dev.getMin()));
    logU2U();
    Serial.print(unit);
    Serial.print(F("; "));
    Serial.print(formatNumber(buf, k197dev.getAverage()));
    logU2U();
    Serial.print(unit);
    Serial.print(F("; "));
    Serial.print(formatNumber(buf, k197dev.getMax()));
    logU2U();
    Serial.print(unit);
  }
  Serial.println();
  CHECK_FREE_STACK();
}

// ***************************************************************************************
// Display functions that have dependencies on menu options
// ***************************************************************************************

/*!
    @brief  display & update a small animation
    @details This function is used to display a small animation at the selected
   coordinate It should be called any time the display is updated due to a new
   messdurement being taken. In this way the user see that the voltmeter SW is
   running, even if the UI is not updated (e.g. in hold mode). Any stuttering
   would instead indicate that the display card is skipping measurements (e.g.
   when it's busy updating a slow bluetooth connection) Note that this function
   changes the active font, however it does not change the cursor coordinates
    @param x the x coordinate where to display the animation (upper-left corner)
    @param y the y coordinate where to display the animation (upper-left corner)
    @param stepDoodle if false the doodle animation is not updated
*/
void UImanager::displayDoodle(u8g2_uint_t x, u8g2_uint_t y, bool stepDoodle) {
  static byte phase = 0;
  if (!showDoodle.getValue())
    return;
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.drawGlyph(x, y, ((u8g2_uint_t)0x25f4) + phase);
  if (!stepDoodle)
    return;
  if (--phase > 3)
    phase = 3;
}

// ***************************************************************************************
//  Graph screen handling
// ***************************************************************************************

static k197_display_graph_type k197graph;
//                           0   1   2   3   4   5   6
static const char prefix[] = {
    'n', 'u', 'm', ' ', 'k', 'M', 'G'}; ///< Lookup table for unit prefixes

/*!
    @brief  get the unit prefix corresponding to a power of 10
    @details for example, pow in the range 6-8 (1 000 000-100 000 000) returns
   'M'
    @param pow10 the power of 10
    @return the unit prefix for that range of powers
*/
static inline char getPrefix(int8_t pow10) {
  int8_t index = pow10 >= 0 ? pow10 / 3 + 3 : (pow10 + 1) / 3 + 2;
  return prefix[index];
}

/*!
    @brief  get the zeroes that must be added to the prefix (see also getPrefix)
    @details for example, pow = 7 (10 000 000) returns 10.
    When the prefix returned by getPrefix(7) is added, we have 10M
    @param pow10 the power of 10
    @return the unit prefix for that range of powers
*/
static inline int8_t getZeroes(int8_t pow10) {
  return pow10 >= 0 ? pow10 % 3 : 2 + ((pow10 + 1) % 3);
}

/*!
    @brief  Utility function, print the label for the Y axis
    @param l the label to print
*/
static void printYLabel(k197graph_label_type l, bool hold) {
  int8_t pow10_effective = l.pow10 + k197dev.getUnitPow10(hold);
  u8g2.print(l.mult);

  int8_t nzeroes = getZeroes(pow10_effective);
  for (uint8_t i = 0; i < nzeroes; i++) {
    u8g2.print('0');
  }
  u8g2.print(getPrefix(pow10_effective));
}

/*!
    @brief  Utility function, print the label for the X and Y axis
    @param l the label to print for the Y axis
    @param nseconds the value in seconds to print for the X axis
*/
static void printXYLabel(k197graph_label_type l, uint16_t nseconds, bool hold) {
  bool hasHours = false;
  bool hasMinutes = false;
  if (nseconds >= 3600) {
    u8g2.print(nseconds / 3600);
    u8g2.print('h');
    nseconds = nseconds % 3600;
    hasHours = true;
  }
  if (nseconds >= (hasHours ? 60 : 901)) {
    u8g2.print(nseconds / 60);
    u8g2.print('\'');
    nseconds = nseconds % 60;
    hasMinutes = true;
  }
  if (nseconds > 0) {
    u8g2.print(nseconds);
    u8g2.print((hasHours || hasMinutes) ? '\"' : 's');
  }

  u8g2.print('/'); // separator between x and y labels

  int8_t pow10_effective = l.pow10 + k197dev.getUnitPow10(hold);
  u8g2.print(l.mult);

  int8_t nzeroes = getZeroes(pow10_effective);
  for (uint8_t i = 0; i < nzeroes; i++) {
    u8g2.print('0');
  }

  u8g2.print(getPrefix(pow10_effective));
}

/*!
    @brief draw a marker at a specific point in the graph
    @details: can print one of the following marker types:
    - CURSOR_A
    - CURSOR_B
    Note that the font used to print the identity of the cursor marker (A or B)
   must be set before calling this function
    @param x the x coordinate of the point where to place the mark
    @param y the y coordinate of the point where to place the mark
    @param marker_type market type
*/
void UImanager::drawMarker(u8g2_uint_t x, u8g2_uint_t y, char marker_type) {
  static const u8g2_uint_t marker_size = 7;
  // k197_display_graph_type::x_size
  u8g2_uint_t x0 = x < marker_size ? 0 : x - marker_size;
  u8g2_uint_t x1 = k197_display_graph_type::x_size < (x + marker_size)
                       ? k197_display_graph_type::x_size
                       : x + marker_size;
  u8g2_uint_t y0 = y < marker_size ? 0 : y - marker_size;
  u8g2_uint_t y1 = k197_display_graph_type::y_size < (y + marker_size)
                       ? k197_display_graph_type::y_size
                       : y + marker_size;
  switch (marker_type) {
  case UImanager::CURSOR_A:
    u8g2.drawLine(x0, y0, x, y);
    u8g2.drawLine(x, y, x1, y1);
    u8g2.drawLine(x0, y1, x, y);
    u8g2.drawLine(x, y, x1, y0);
    if (int(y1) > (k197_display_graph_type::y_size - u8g2.getMaxCharHeight())) {
      u8g2.setCursor(x0,
                     y0 - u8g2.getMaxCharHeight()); // Position above the marker
    } else {
      u8g2.setCursor(x0, y1); // Position below the marker
    }
    u8g2.print(marker_type);
    if (getActiveCursor() == marker_type)
      u8g2.print('<');
    break;
  case UImanager::CURSOR_B:
    u8g2.drawLine(x0, y, x1, y);
    u8g2.drawLine(x, y0, x, y1);
    u8g2.drawFrame(x0, y0, x1 - x0, y1 - y0);
    bool actv = getActiveCursor() == marker_type;
    if (int(y1) > (k197_display_graph_type::y_size - u8g2.getMaxCharHeight())) {
      u8g2.setCursor(x1 - u8g2.getMaxCharWidth() * (actv ? 2 : 1),
                     y0 - u8g2.getMaxCharHeight()); // Position above the marker
    } else {
      u8g2.setCursor(x1 - u8g2.getMaxCharWidth(),
                     y1); // Position below the marker
    }
    if (actv)
      u8g2.print('>');
    u8g2.print(marker_type);
    break;
  }
}

/*!
    @brief  update the display, used when in graph mode
   screen.
*/
void UImanager::updateGraphScreen() {
  bool hold = k197dev.getDisplayHold();

  // Get graph data
  k197dev.fillGraphDisplayData(&k197graph, opt_gr_yscale.getValue(), hold);
  RT_ASSERT(k197graph.gr_size <= k197graph.x_size, "!updGrDsp1");

  // autoscale x axis
  uint16_t i1 = 16;
  while (i1 < k197graph.gr_size)
    i1 *= 2;
  if (i1 > k197graph.x_size)
    i1 = k197graph.x_size;

  byte xscale = k197graph.x_size / i1;
  RT_ASSERT(k197graph.gr_size <= k197graph.x_size, "!updGrDsp1");

  // Y axis
  u8g2.drawLine(k197graph.x_size, k197graph.y_size, k197graph.x_size, 0);

  // X axis
  if (gr_yscale_show0.getValue() && k197graph.y0.isNegative() &&
      k197graph.y1.isPositive()) {
    drawDottedHLine(0, k197graph.y_size - k197graph.y_zero,
                    k197graph.x_size); // zero axis
  }
  u8g2.setFont(u8g2_font_6x12_mr);

  // Draw axis labels
  u8g2.setDrawColor(1);
  u8g2.setCursor(k197graph.x_size + 2,
                 k197graph.y_size - u8g2.getMaxCharHeight());
  printXYLabel(k197graph.y0,
               k197graph.nsamples_graph == 0
                   ? 60 / xscale
                   : (60 / xscale) * k197graph.nsamples_graph,
               hold);
  u8g2.setCursor(k197graph.x_size + 2, 0);
  printYLabel(k197graph.y1, hold);
  u8g2_uint_t topln_x = u8g2.tx;

  // Draw the graph
  if ((opt_gr_type.getValue() == OPT_GRAPH_TYPE_DOTS) ||
      (k197graph.gr_size < 2)) {
    for (int i = 0; i < k197graph.gr_size; i++) {
      RT_ASSERT(k197graph.point[i] <= k197graph.y_size, "!updGrDsp2a");
      u8g2.drawPixel(i * xscale, k197graph.y_size - k197graph.point[i]);
    }
  } else { // OPT_GRAPH_TYPE_LINES && k197graph.gr_size>=2
    for (int i = 0; i < (k197graph.gr_size - 1); i++) {
      RT_ASSERT(k197graph.point[i] <= k197graph.y_size, "!updGrDsp2b");
      RT_ASSERT(k197graph.point[i + 1] <= k197graph.y_size, "!updGrDsp2c");
      u8g2.drawLine(i * xscale, k197graph.y_size - k197graph.point[i],
                    (i + 1) * xscale,
                    k197graph.y_size - k197graph.point[i + 1]);
    }
  }

  // Information panel
  if (areCursorsVisible() && k197graph.gr_size > 0) {
    u8g2_uint_t ax =
        cursor_a >= k197graph.gr_size ? k197graph.gr_size - 1 : cursor_a;
    u8g2_uint_t bx =
        cursor_b >= k197graph.gr_size ? k197graph.gr_size - 1 : cursor_b;
    drawMarker(xscale * ax, k197graph.y_size - k197graph.point[ax], CURSOR_A);
    drawMarker(xscale * bx, k197graph.y_size - k197graph.point[bx], CURSOR_B);

    RT_ASSERT_ACT(ax < k197dev.getGraphSize(hold), DebugOut.print(F("!AX "));
                  DebugOut.print(ax); DebugOut.print(F(", A: "));
                  DebugOut.print(cursor_a); DebugOut.print(F(", size="));
                  DebugOut.println(k197dev.getGraphSize(hold));)
    RT_ASSERT_ACT(bx < k197dev.getGraphSize(hold), DebugOut.print(F("!BX "));
                  DebugOut.print(bx); DebugOut.print(F(", B: "));
                  DebugOut.print(cursor_b); DebugOut.print(F(", size="));
                  DebugOut.println(k197dev.getGraphSize(hold));)
    drawGraphScreenCursorPanel(topln_x, ax, bx);
  } else {
    drawGraphScreenNormalPanel(topln_x);
  }
  CHECK_FREE_STACK();
}

/*!
    @brief  draw a small panel showing the measurement value
    @details this method is called from updateGraphScreen().
    Used when in graph mode screen when the cursors are hidden
    The top line include the axis labels, which has variable lenght
    and is printed by updateGraphScreen(). The start cooordinate for this
   line is passed in the argument topln_x
    @param topln_x the first usable x coordinates of the top line of the panel
   panel
*/
void UImanager::drawGraphScreenNormalPanel(u8g2_uint_t topln_x) {
  // temporary buffer used for number formatting
  char buf[K197_RAW_MSG_SIZE + 1]; // +1 needed to account for '.'

  bool hold = k197dev.getDisplayHold();

  u8g2_uint_t x = 185 + 10;
  u8g2_uint_t y = 3;
  u8g2.setFont(u8g2_font_5x7_mr);
  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);

  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197dev.getUnit(true, hold));
  y += u8g2.getMaxCharHeight();

  u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setCursor(u8g2.tx, u8g2.ty + 1);
  if (k197dev.isAC(hold))
    u8g2.print(F(" AC"));
  if (k197dev.isREL(hold))
    u8g2.print(F(" REL"));

  x = 185 + 5;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_8x13_mr);
  if (k197dev.isNumeric(hold)) {
    u8g2.print(formatNumber(buf, k197dev.getValue(hold)));
  } else {
    u8g2.print(k197dev.getRawMessage(hold));
  }

  // Draw AUTO & HOLD at about the same height as the Y label
  u8g2.setFont(u8g2_font_5x7_mr);
  x = topln_x + 5;
  y = 1;
  u8g2.setCursor(x, y);
  if (k197dev.isAuto())
    u8g2.print(F("AUTO"));
  else
    u8g2.print(F("    "));
  if (k197dev.getDisplayHold())
    u8g2.print(F(" HOLD"));
}

/*!
    @brief  draw a small panel showing the cursor values
    @details this method is called from updateGraphScreen().
    Used when in graph mode screen when the cursors are visible
    The top line includes the axis label, which has variable lenght
    and is printed by updateGraphScreen(). The start cooordinate for this
   line is passed in the argument topln_x
    @param topln_x the first usable x coordinates of the top line of the panel
   panel
    @param ax the graph index corresponding to cursor A
    @param bx the graph index corresponding to cursor B
*/
void UImanager::drawGraphScreenCursorPanel(u8g2_uint_t topln_x, u8g2_uint_t ax,
                                           u8g2_uint_t bx) {
  // temporary buffer used for number formatting
  char buf[K197_RAW_MSG_SIZE + 1]; // +1 needed to account for '.'

  bool hold = k197dev.getDisplayHold();

  u8g2.setCursor(topln_x + 1, 0);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197dev.getUnit(true, hold));

  u8g2.setFont(u8g2_font_5x7_mr);
  u8g2.setCursor(u8g2.tx + 2, 3);
  if (k197dev.isAC(hold))
    u8g2.print(F("AC"));
  else
    u8g2.print(CH_SPACE);
  u8g2.setCursor(u8g2.tx + 2, u8g2.ty);
  if (k197dev.isREL(hold))
    u8g2.print(F("REL"));

  u8g2.setCursor(183, u8g2.ty + u8g2.getMaxCharHeight() + 4);
  u8g2.print(F("<A>"));
  u8g2.print(CH_SPACE);
  // u8g2.print(k197dev.getGraphValue(ax, hold), 6);
  u8g2.print(formatNumber(buf, k197dev.getGraphValue(ax, hold)));

  u8g2.setCursor(183, u8g2.ty + u8g2.getMaxCharHeight() + 2);
  u8g2.print(F("<B>"));
  u8g2.print(CH_SPACE);
  // u8g2.print(k197dev.getGraphValue(bx, hold), 6);
  u8g2.print(formatNumber(buf, k197dev.getGraphValue(bx, hold)));

  uint16_t deltax = ax > bx ? ax - bx : bx - ax;

  u8g2.setCursor(183, u8g2.ty + u8g2.getMaxCharHeight() + 2);
  u8g2.print(F("Avg"));
  u8g2.print(CH_SPACE);
  // u8g2.print(k197dev.getGraphAverage(ax < bx ? ax : bx, deltax+1),
  // 6);
  u8g2.print(formatNumber(
      buf, k197dev.getGraphAverage(ax < bx ? ax : bx, deltax + 1, hold)));

  u8g2.setCursor(183, u8g2.ty + u8g2.getMaxCharHeight() + 2);
  u8g2.print(F("Dt"));
  u8g2.print(CH_SPACE);
  if (k197graph.nsamples_graph == 0) {
    u8g2.print(float(deltax) / 3.0, 2);
  } else {
    u8g2.print(deltax * k197graph.nsamples_graph / 3.0);
  }
  u8g2.print(CH_SPACE);
  u8g2.print('s');

  if (hold) {
    u8g2_uint_t x = display_size_x - u8g2.getMaxCharWidth();
    u8g2.setCursor(x, 7 + u8g2.getMaxCharHeight());
    u8g2.print('H');
    u8g2.setCursor(x, u8g2.ty + u8g2.getMaxCharHeight());
    u8g2.print('o');
    u8g2.setCursor(x, u8g2.ty + u8g2.getMaxCharHeight());
    u8g2.print('l');
    u8g2.setCursor(x, u8g2.ty + u8g2.getMaxCharHeight());
    u8g2.print('d');
  }
}

// ***************************************************************************************
//  UI event management
// ***************************************************************************************

/*!
    @brief  handle UI event

    @details handle UI events (pushbutton presses) that should be handled
   locally, according to display mode and K197 current status In menu mode most
   events are passed to UImenu

    @param eventSource identifies the source of the event (REL, DB, etc.)
    @param eventType identifies the source of the event (REL, DB, etc.)

    @return true if the event has been completely handled, false otherwise
*/
bool UImanager::handleUIEvent(K197UIeventsource eventSource,
                              K197UIeventType eventType) {
  if (k197dev.isCal())
    return false;
  if (eventSource == K197key_REL && eventType == UIeventLongPress &&
      !(isGraphMode() &&
        areCursorsVisible())) { // This event is handled the same in all
                                // other screen modes
    if (isFullScreen())
      showOptionsMenu();
    else
      showFullScreen();
    return true; // Skip normal handling in the main sketch
  }
  if (isMenuVisible()) {
    if (UIwindow::getcurrentWindow()->handleUIEvent(eventSource, eventType))
      return true;              // Skip normal handling in the main sketch
  } else if (isSplitScreen()) { // Split screen with no menu visible
    if (eventType == UIeventClick || eventType == UIeventLongPress) {
      showFullScreen();
      return true; // Skip normal handling in the main sketch
    }
  } else
    switch (eventSource) { // Full scren mode
    case K197key_STO:
      if (reassignStoRcl.getValue()) {
        if (eventType == UIeventPress) {
          if (k197dev.getDisplayHold()) { // we are currently on hold
            // At this point we don't know if it will be a short or long
            // press/click since removing hold is not time critical, we wait
            // until the click event We will fix things in the following events
            hold_flag = true;
          } else { // we are currently not on hold
            // At this point we don't know if it will be a short or long press
            // since hold can be time critical, we set hold mode immediately
            // We will fix things in the following events
            hold_flag = false;
            k197dev.setDisplayHold(true);
          }
          // GPIOR3 |= 0x10; // set debug flag
        } else if (eventType == UIeventLongPress) {
          // if hold was not active before, we need to undo the activation
          if (hold_flag == false) { // hold was not active before
            // and then we cancel the hold
            k197dev.setDisplayHold(false);
          }
          K197screenMode screen_mode = uiman.getScreenMode();
          if (screen_mode != K197sc_minmax)
            uiman.setScreenMode(K197sc_minmax);
          else
            uiman.setScreenMode(K197sc_normal);
        } else if (eventType == UIeventClick) {
          // Now we know this was a short click
          if (hold_flag) { // hold was active when button was pressed
            k197dev.setDisplayHold(false); // so now we need to cancel the hold
          }
        } else if (eventType == UIeventDoubleClick) {
          // NOP
        }
        return true; // Skip normal handling in the main sketch
      }
      break;
    case K197key_RCL:
      if (reassignStoRcl.getValue()) {
        if (eventType == UIeventClick) {
          if (isGraphMode() && areCursorsVisible())
            toggleActiveCursor();
        } else if (eventType == UIeventLongPress) {
          K197screenMode screen_mode = uiman.getScreenMode();
          if (screen_mode != K197sc_graph)
            uiman.setScreenMode(K197sc_graph);
          else
            uiman.setScreenMode(K197sc_normal);
        } else if (eventType == UIeventDoubleClick) {
          if (isGraphMode())
            toggleCursorsVisibility();
        }
        return true; // Skip normal handling in the main sketch
      }
      break;
    case K197key_REL:
      if (isGraphMode() && areCursorsVisible()) {
        if (eventType == UIeventPress) {
          incrementCursor(-1);
        } else if (eventType == UIeventLongPress) {
          incrementCursor(-10);
        } else if (eventType == UIeventHold) {
          incrementCursor(-5);
        }
        return true; // Skip normal handling in the main sketch
      } else if (eventType == UIeventDoubleClick) {
        pushbuttons.cancelClickREL();
        // DebugOut.println(F("REL dblclk"));
        k197dev.resetStatistics();
        return true; // Skip normal handling in the main sketch
      }
      break;
    case K197key_DB:
      if (isGraphMode() && areCursorsVisible()) {
        if (eventType == UIeventPress) {
          incrementCursor(1);
        } else if (eventType == UIeventLongPress) {
          incrementCursor(10);
        } else if (eventType == UIeventHold) {
          incrementCursor(5);
        }
        return true; // Skip normal handling in the main sketch
      } else if (additionalModes.getValue()) {
        if (eventType == UIeventPress) {
          if (k197dev.isV() && k197dev.ismV() && k197dev.isDC()) {
            if (!k197dev.getTKMode()) { // TK mode is not yet enabled
              k197dev.setTKMode(true);  // Activate TK mode
              return true; // Skip normal handling in the main sketch
            }
          } else {
            k197dev.setTKMode(false);
            // Do NOT skip normal handling in the main sketch
          }
        }
      }
      break;
    }
  return false;
  CHECK_FREE_STACK();
}

// ***************************************************************************************
//  EEPROM Save/restore
// ***************************************************************************************
#include <EEPROM.h>
#define EEPROM_BASE_ADDRESS 0x00 ///< Base address on the EEPROM

/*!
    @brief copy data from the GUI to the structure
    @details This function is used inside store_to_EEPROM
*/
void permadata::copyFromUI() {
  CHECK_FREE_STACK();
  bool_options.additionalModes = additionalModes.getValue();
  bool_options.reassignStoRcl = reassignStoRcl.getValue();
  bool_options.showDoodle = showDoodle.getValue();
  bool_options.logEnable = logEnable.getValue();
  bool_options.logSplitUnit = logSplitUnit.getValue();
  bool_options.logTimestamp = logTimestamp.getValue();
  bool_options.logTamb = logTamb.getValue();
  bool_options.logStat = logStat.getValue();
  bool_options.gr_yscale_show0 = gr_yscale_show0.getValue();
  bool_options.unused_no_1 = 1; // Backward compatibility for removed option
  bool_options.gr_xscale_autosample = gr_xscale_autosample.getValue();
  bool_options.logError = logError.getValue();
  bool_options.unused_no_2 = false;
  bool_options.gr_yscale_full_range = gr_yscale_full_range.getValue();
  byte_options.contrastCtrl = contrastCtrl.getValue();
  byte_options.logSkip = logSkip.getValue();
  byte_options.logStatSamples = logStatSamples.getValue();
  byte_options.opt_gr_type = opt_gr_type.getValue();
  byte_options.opt_gr_yscale = (byte)opt_gr_yscale.getValue();
  byte_options.gr_sample_time = gr_sample_time.getValue();
  screenMode = uiman.getScreenMode();
  byte_options.cursor_a = uiman.getCursorPosition(UImanager::CURSOR_A);
  byte_options.cursor_b = uiman.getCursorPosition(UImanager::CURSOR_B);
}

/*!
    @brief copy data from the structure to the GUI
    @details the new options take affect immediately.
    This function is used inside retrieve_from_EEPROM
    @param restore_screen_mode if true restores the screen mode
*/
void permadata::copyToUI(bool restore_screen_mode) {
  CHECK_FREE_STACK();
  additionalModes.setValue(bool_options.additionalModes);
  reassignStoRcl.setValue(bool_options.reassignStoRcl);
  showDoodle.setValue(bool_options.showDoodle);
  showDoodle.change();
  logEnable.setValue(bool_options.logEnable);
  logSkip.setValue(byte_options.logSkip);
  logSplitUnit.setValue(bool_options.logSplitUnit);
  logTimestamp.setValue(bool_options.logTimestamp);
  logTamb.setValue(bool_options.logTamb);
  logStat.setValue(bool_options.logStat);
  logError.setValue(bool_options.logError);
  // option unused_no_2 is of course not used anymore
  logStatSamples.setValue(byte_options.logStatSamples);
  k197dev.setNsamples(byte_options.logStatSamples);

  gr_yscale_show0.setValue(bool_options.gr_yscale_show0);
  opt_gr_yscale.setValue((k197graph_yscale_opt)byte_options.opt_gr_yscale);
  // option unused_no_1 is of course not used anymore
  gr_xscale_autosample.setValue(bool_options.gr_xscale_autosample);
  gr_xscale_autosample.change();
  gr_yscale_full_range.setValue(bool_options.gr_yscale_full_range);
  gr_yscale_full_range.change();

  uiman.setContrast(byte_options.contrastCtrl);
  opt_gr_type.setValue(byte_options.opt_gr_type);
  if (!bool_options.gr_xscale_autosample) { // Do not mess sample rate if
                                            // autosamplig mode
    gr_sample_time.setValue(byte_options.gr_sample_time);
  }
  if (restore_screen_mode)
    uiman.setScreenMode(screenMode);
  uiman.setCursorPosition(UImanager::CURSOR_A, byte_options.cursor_a);
  uiman.setCursorPosition(UImanager::CURSOR_B, byte_options.cursor_b);
}

/*!
    @brief  store all configuration options from the EEPROM
    @details a confirmation or error message is sent to DebugOut
    @return true if the operation is succesful
*/
bool permadata::store_to_EEPROM() {
  if ((EEPROM_BASE_ADDRESS + sizeof(permadata)) > EEPROM.length()) {
    DebugOut.print(F("Data size"));
    return false;
  }
  permadata pdata;
  pdata.copyFromUI();
  EEPROM.put(EEPROM_BASE_ADDRESS, pdata);
  return true;
}

/*!
    @brief  retrieve all configuration options to the EEPROM
    @details if successful, the new options take affect immediately.
    A confirmation or error message is sent to DebugOut
    @param restore_screen_mode if true restores the screen mode
    @return true if the operation is succesful
*/
bool permadata::retrieve_from_EEPROM(bool restore_screen_mode) {
  if ((EEPROM_BASE_ADDRESS + sizeof(permadata)) > EEPROM.length()) {
    DebugOut.print(F("Data size"));
    // DebugOut.print(CH_SPACE); DebugOut.print(sizeof(permadata));
    // DebugOut.print(F(" exceed "));
    // DebugOut.println(EEPROM.length());
    return false;
  }
  permadata pdata;
  EEPROM.get(EEPROM_BASE_ADDRESS, pdata);
  if (pdata.magicNumber != magicNumberExpected) {
    DebugOut.println(F("No data"));
    return false;
  }
  if (pdata.revision != revisionExpected) {
    DebugOut.print(F("EEPROM: rev. "));
    DebugOut.print(pdata.revision, HEX);
    DebugOut.print(F(", expected "));
    DebugOut.println(revisionExpected, HEX);
    return false;
  }
  pdata.copyToUI(restore_screen_mode);
  return true;
}
