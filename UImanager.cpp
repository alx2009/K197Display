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

  If 16 mode is not active we will trigger a compilation error as otherwise it
  would be rather difficult to figure out why it does not diplay correctly...

*/
/**************************************************************************/

/*=================Font Usage=======================
  Fonts used in the application:
    u8g2_font_5x7_mr  ==> terminal window, local T, Bluetooth status
    u8g2_font_6x12_mr ==> BAT, all menus
    u8g2_font_8x13_mr ==> AUTO, REL, dB, STO, RCL, Cal, RMT, most annunciators
  in split screen u8g2_font_9x15_m_symbols ==> meas. unit, AC, Split screen:
  message, AC u8g2_font_inr30_mr ==> main message u8g2_font_inr16_mr ==> main
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

#define U8LOG_WIDTH 28                            ///< Size of the log window
#define U8LOG_HEIGHT 9                            ///< Height of the log window
uint8_t u8log_buffer[U8LOG_WIDTH * U8LOG_HEIGHT]; ///< buffer for the log window
U8G2LOG u8g2log;                                  ///< the log window

// ***************************************************************************************
// Graphics utility functions
// ***************************************************************************************
void drawDottedHLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t dotsize=10, uint16_t dotDistance=20) {
   if (dotsize==0) dotsize=1;
   if (dotDistance==0) dotDistance=dotsize*5;
   uint16_t xdot;
   do {
       xdot=x0+dotsize;
       if (xdot>x1)xdot=x1;
       u8g2.drawLine(x0, y0, xdot, y0);
       x0+=dotDistance;
   } while (x0<=x1);
}

/*
void drawHArrowHead(uint16_t x0, uint16_t y0, uint16_t arrowSize){
    u8g2.drawLine(x0, y0, x0+arrowSize, y0+arrowSize);
    u8g2.drawLine(x0, y0, x0+arrowSize, y0-arrowSize);
}*/

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

  /*
    u8g2.setFont(u8g2_font_inr16_mr);
    DebugOut.print(F("Disp. H="));
    DebugOut.print(u8g2.getDisplayHeight());
    DebugOut.print(F(", font h="));
    DebugOut.print(u8g2.getAscent());
    DebugOut.print(F("/"));
    DebugOut.print(u8g2.getDescent());
    DebugOut.print(F("/"));
    DebugOut.println(u8g2.getMaxCharHeight());
  */
  dxUtil.checkFreeStack();
}

/*!
    @brief  setup the display and clears the screen. Must be called before any
   other member function
*/
void UImanager::setup() {
  pinMode(OLED_MOSI, OUTPUT); // Needed to work around a bug in the micro or
                              // dxCore with certain swap options
  bool pmap = SPI.swap(OLED_SPI_SWAP_OPTION); // See pinout.h for pin definition
  if (pmap) {
    // DebugOut.println(F("SPI pin mapping OK!"));
    // DebugOut.flush();
  } else {
    DebugOut.println(F("SPI pin mapping failed!"));
    DebugOut.flush();
    while (true)
      ;
  }
  u8g2.setBusClock(12000000); // without this call the default clock is about
                              // 5 MHz, which is more than good enough.
  u8g2.begin();
  setContrast(DEFAULT_CONTRAST);
  u8g2.enableUTF8Print();
  u8g2log.begin(U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);

  u8g2.clearBuffer();
  setup_draw();
  u8g2.sendBuffer();
  DebugOut.print(F("sizeof(u8g2_uint_t)=")); DebugOut.println(sizeof(u8g2_uint_t));

  setupMenus();
}

// ***************************************************************************************
// Display update 
// ***************************************************************************************

/*!
    @brief  update the display. The information comes from the pointer to
   K197device passed when the object was constructed

    This function will not cause the K197device object to read new data, it will
   use whatever data is already stored in the object.

    Therefore, it should not be called before the first data has been received
   from the K197/197A

    If you want to add an initial scren/text, the best way would be to add this
   to setup();
*/
void UImanager::updateDisplay() {
  if (k197dev.isNotCal() && isSplitScreen())
    updateSplitScreen();
  else if (k197dev.isCal() || (getScreenMode() == K197sc_normal))
    updateNormalScreen();
  else if (getScreenMode() == K197sc_minmax)
    updateMinMaxScreen();
  else updateGraphScreen();
  dxUtil.checkFreeStack();
}

/*!
    @brief  update the display, used when in debug and other modes with split
   screen.
*/
void UImanager::updateSplitScreen() {
  u8g2_uint_t x = 140;
  u8g2_uint_t y = 5;
  u8g2.setFont(u8g2_font_8x13_mr);
  u8g2.setCursor(x, y);
  if (k197dev.isAuto())
    u8g2.print(F("AUTO "));
  else
    u8g2.print(F("     "));
  if (k197dev.isBAT())
    u8g2.print(F("BAT "));
  else
    u8g2.print(F("    "));
  if (k197dev.isREL())
    u8g2.print(F("REL "));
  else
    u8g2.print(F("    "));
  if (k197dev.isCal())
    u8g2.print(F("Cal   "));
  else
    u8g2.print(F("      "));

  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  if (k197dev.isNumeric()) {
    char buf[K197_RAW_MSG_SIZE + 1];
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
  else
    u8g2.print(F("      "));

  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_8x13_mr);
  if (k197dev.isSTO())
    u8g2.print(F("STO "));
  else
    u8g2.print(F("    "));
  if (k197dev.isRCL())
    u8g2.print(F("RCL "));
  else
    u8g2.print(F("    "));
  if (k197dev.isRMT())
    u8g2.print(F("RMT   "));
  else
    u8g2.print(F("      "));

  if (isSplitScreen() && !isMenuVisible()) { // Show the debug log
    u8g2.setFont(u8g2_font_5x7_mr); // set the font for the terminal window
    u8g2.drawLog(0, 0, u8g2log);    // draw the terminal window on the display
  } else { // For all other modes we show the settings menu when in split mode
    UIwindow::getcurrentWindow()->draw(&u8g2, 0, 10);
  }
  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
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

  if (!k197dev.getDisplayHold()) {
    u8g2.drawStr(xraw, yraw, k197dev.getRawMessage());
    for (byte i = 1; i <= 7; i++) {
      if (k197dev.isDecPointOn(i)) {
        u8g2.drawBox(xraw + i * u8g2.getMaxCharWidth() - dphsz_x,
                     yraw + u8g2.getAscent() - dphsz_y, dpsz_x, dpsz_y);
      }
    }
    // set the unit
    u8g2.setFont(u8g2_font_9x15_m_symbols);
    const unsigned int xunit = 229;
    const unsigned int yunit = 20;
    u8g2.setCursor(xunit, yunit);
    u8g2.print(k197dev.getUnit());

    // set the AC/DC indicator
    u8g2.setFont(u8g2_font_9x15_m_symbols);
    const unsigned int xac = xraw + 3;
    const unsigned int yac = 40;
    u8g2.setCursor(xac, yac);
    if (k197dev.isAC())
      u8g2.print(F("AC"));
    else
      u8g2.print(F("  "));
  }

  // set the other announciators
  u8g2.setFont(u8g2_font_8x13_mr);
  unsigned int x = 0;
  unsigned int y = 5;
  u8g2.setCursor(x, y);
  if (k197dev.isAuto())
    u8g2.print(F("AUTO"));
  else
    u8g2.print(F("    "));
  x = u8g2.tx;
  x += u8g2.getMaxCharWidth() * 2;
  u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setCursor(x, y);
  if (k197dev.isBAT())
    u8g2.print(F("BAT"));
  else
    u8g2.print(F("   "));

  u8g2.setFont(u8g2_font_8x13_mr);
  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);

  if (!k197dev.getDisplayHold()) {
    if (k197dev.isREL())
      u8g2.print(F("REL"));
    else
      u8g2.print(F("   "));
    x = u8g2.tx;
    x += (u8g2.getMaxCharWidth() / 2);
    u8g2.setCursor(x, y);
    if (k197dev.isdB())
      u8g2.print(F("dB"));
    else
      u8g2.print(F("  "));
  }
  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);
  if (k197dev.getDisplayHold()) {
    u8g2.print(F("HOLD"));
  } else {
    if (k197dev.isSTO())
      u8g2.print(F("STO "));
    else
      u8g2.print(F("    "));

    y += u8g2.getMaxCharHeight();
    u8g2.setCursor(x, y);
    if (k197dev.isRCL())
      u8g2.print(F("RCL "));
    else
      u8g2.print(F("    "));
  }
  x = 229;
  y = 0;
  u8g2.setCursor(x, y);
  if (k197dev.isCal())
    u8g2.print(F("Cal"));
  else
    u8g2.print(F("   "));

  y += u8g2.getMaxCharHeight() * 3;
  u8g2.setCursor(x, y);
  if (k197dev.isRMT())
    u8g2.print(F("RMT"));
  else
    u8g2.print(F("   "));

  x = 140;
  y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (k197dev.isTKModeActive() &&
      !k197dev.getDisplayHold()) { // Display local temperature
    char buf[K197_RAW_MSG_SIZE + 1];
    dtostrf(k197dev.getTColdJunction(), K197_RAW_MSG_SIZE, 2, buf);
    u8g2.print(buf);
    u8g2.print(k197dev.getUnit());
  } else if (!k197dev.getDisplayHold()) {
    u8g2.print(F("          "));
  }

  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
    @brief  update the display, used when in minmax screen mode. See also
   updateDisplay().
*/
void UImanager::updateMinMaxScreen() {
  u8g2.setFont(
      u8g2_font_inr16_mr); // width =25  points (7 characters=175 points)
  const unsigned int xraw = 130;
  const unsigned int yraw = 15;
  const unsigned int dpsz_x = 3;  // decimal point size in x direction
  const unsigned int dpsz_y = 3;  // decimal point size in y direction
  const unsigned int dphsz_x = 2; // decimal point "half size" in x direction
  const unsigned int dphsz_y = 2; // decimal point "half size" in y direction

  if (!k197dev.getDisplayHold()) {
    u8g2.drawStr(xraw, yraw, k197dev.getRawMessage());
    for (byte i = 1; i <= 7; i++) {
      if (k197dev.isDecPointOn(i)) {
        u8g2.drawBox(xraw + i * u8g2.getMaxCharWidth() - dphsz_x,
                     yraw + u8g2.getAscent() - dphsz_y, dpsz_x, dpsz_y);
      }
    }

    // set the unit
    u8g2.setFont(u8g2_font_9x15_m_symbols);
    const unsigned int xunit = 229;
    const unsigned int yunit = 20;
    u8g2.setCursor(xunit, yunit);
    u8g2.print(k197dev.getUnit(true));

    // set the AC/DC indicator
    u8g2.setFont(u8g2_font_9x15_m_symbols);
    int char_height_9x15 = u8g2.getMaxCharHeight(); // needed later on
    const unsigned int xac = 229;
    const unsigned int yac = 35;
    u8g2.setCursor(xac, yac);
    if (k197dev.isAC())
      u8g2.print(F("AC"));
    else
      u8g2.print(F("  "));

    // set the other announciators
    u8g2.setFont(u8g2_font_6x12_mr);
    unsigned int x = 0;
    unsigned int y = 5;
    u8g2.setCursor(x, y);
    if (k197dev.isREL())
      u8g2.print(F("REL"));
    else
      u8g2.print(F("   "));

    // Write Min/average/Max labels
    u8g2.setFont(u8g2_font_5x7_mr);
    x = u8g2.tx + 10;
    y = 5;
    u8g2.setCursor(x, y);
    u8g2.print(F("Max "));
    y += char_height_9x15;
    u8g2.setCursor(x, y);
    u8g2.print(F("Avg "));
    y += char_height_9x15;
    u8g2.setCursor(x, y);
    u8g2.print(F("Min "));

    u8g2.setFont(u8g2_font_9x15_m_symbols);
    char buf[K197_RAW_MSG_SIZE + 1];
    x = u8g2.tx;
    y = 3;
    u8g2.setCursor(x, y);
    u8g2.print(formatNumber(buf, k197dev.getMax()));
    y += char_height_9x15;
    u8g2.setCursor(x, y);
    u8g2.print(formatNumber(buf, k197dev.getAverage()));
    y += char_height_9x15;
    u8g2.setCursor(x, y);
    u8g2.print(formatNumber(buf, k197dev.getMin()));

    x = 170;
    y = 2;
    u8g2.setCursor(x, y);
    u8g2.setFont(u8g2_font_5x7_mr);
    if (k197dev.isTKModeActive()) { // Display local temperature
      char buf[K197_RAW_MSG_SIZE + 1];
      dtostrf(k197dev.getTColdJunction(), K197_RAW_MSG_SIZE, 2, buf);
      u8g2.print(buf);
      u8g2.print(k197dev.getUnit());
    } else {
      u8g2.print(F("          "));
    }
  }
  u8g2.setFont(u8g2_font_8x13_mr);
  unsigned int x = 0;
  unsigned int y = 5 + u8g2.getMaxCharHeight() * 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (k197dev.getDisplayHold())
    u8g2.print(F("HOLD"));
  else
    u8g2.print(F("    "));
  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
      @brief display the BT module status (detected or not detected)

      @param present true if a BT module is detected, false otherwise
      @param connected true if a BT connection is detected, false otherwise
*/
void UImanager::updateBtStatus() {
  if (isSplitScreen() ||
      (getScreenMode() != K197sc_normal)) {
    return;
  }
  unsigned int x = 95;
  unsigned int y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (BTman.isPresent()) {
    u8g2.print(F("bt "));
  } else {
    u8g2.print(F("   "));
  }
  x += u8g2.getStrWidth("   ");
  u8g2.setCursor(x, y);
  bool connected = BTman.isConnected();
  if (connected && isLogging()) {
    u8g2.print(F("<=>"));
  } else if (connected) {
    u8g2.print(F("<->"));
  } else {
    u8g2.print(F("   "));
  }
  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
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

      @param mode the new display mode. Only the modes defined in K197screenMode < 0x0f are valid. 
      Using an invalid mode will have no effect 
*/
void UImanager::setScreenMode(K197screenMode mode) {
  if ( (mode <=0) || (mode>K197sc_graph) ) return;
  screen_mode =
      (K197screenMode)(screen_mode &
                       K197sc_AttributesBitMask); // clear current screen mode
  screen_mode =
      (K197screenMode)(screen_mode | mode); // set the mode bits to
                                            // enter the new mode
  clearScreen();
}

/*!
  @brief clear the display
  @details This is done automatically when the screen mode changes, there should
  be no need to call this function elsewhere
*/
void UImanager::clearScreen() {
  // DebugOut.print(F("screen_mode=")); DebugOut.println(screen_mode, HEX);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  k197dev.setDisplayHold(false);
  dxUtil.checkFreeStack();
}

// ***************************************************************************************
//  Message box definitions
// ***************************************************************************************

DEF_MESSAGE_BOX(EEPROM_save_msg_box, 100, "config saved");
DEF_MESSAGE_BOX(EEPROM_reload_msg_box, 100, "config reloaded");
DEF_MESSAGE_BOX(ERROR_msg_box, 100, "Error (see log)");


// ***************************************************************************************
//  Menu definition/handling
// ***************************************************************************************

DEF_MENU_CLOSE(closeMenu, 15,
               "< Back"); ///< Menu close action (used in multiple menus)
DEF_MENU_ACTION(
    exitMenu, 15, "Exit",
    uiman.showFullScreen();); ///< Menu close action (used in multiple menus)

DEF_MENU_SEPARATOR(mainSeparator0, 15, "< Options >"); ///< Menu separator
DEF_MENU_BOOL(additionalModes, 15, "Extra Modes");     ///< Menu input
DEF_MENU_BOOL(reassignStoRcl, 15, "Reassign STO/RCL"); ///< Menu input
DEF_MENU_OPEN(btDatalog, 15, "Data logging >>>", &UIlogMenu); ///< Open submenu
DEF_MENU_OPEN(btGraphOpt, 15, "Graph options >>>", &UIgraphMenu); ///< Open submenu
DEF_MENU_BUTTON(bluetoothMenu, 15,
                "Bluetooth"); ///< TBD: submenu not yet implemented
DEF_MENU_BYTE_ACT(contrastCtrl, 15, "Contrast",
                  u8g2.setContrast(getValue());); ///< set contrast
DEF_MENU_ACTION(saveSettings, 15, "Save settings",
      if (permadata::store_to_EEPROM()) {
          EEPROM_save_msg_box.show();
      } else {
          ERROR_msg_box.show();
      } ); ///< save config to EEPROM and show result
DEF_MENU_ACTION(
      reloadSettings, 15, "Reload settings",
      if (permadata::retrieve_from_EEPROM()) {
          EEPROM_reload_msg_box.show();
      } else {
          ERROR_msg_box.show();
      } ); ///< load config from EEPROM and show result
DEF_MENU_ACTION(openLog, 15, "Show log",
                dxUtil.reportStack();
                DebugOut.println(); uiman.showDebugLog();); ///< show debug log

UImenuItem *mainMenuItems[] = {
    &mainSeparator0, &additionalModes, &reassignStoRcl, &btDatalog, &btGraphOpt,
    &bluetoothMenu,  &contrastCtrl,    &exitMenu,       &saveSettings,
    &reloadSettings, &openLog}; ///< Root menu items

DEF_MENU_SEPARATOR(logSeparator0, 15, "< BT Datalogging >"); ///< Menu separator
DEF_MENU_BOOL(logEnable, 15, "Enabled");                     ///< Menu input
DEF_MENU_BYTE(logSkip, 15, "Samples to skip");               ///< Menu input
DEF_MENU_BOOL(logSplitUnit, 15, "Split unit");               ///< Menu input
DEF_MENU_BOOL(logTimestamp, 15, "Log timestamp");            ///< Menu input
DEF_MENU_BOOL(logTamb, 15, "Include Tamb");                  ///< Menu input
DEF_MENU_BOOL(logStat, 15, "Include Statistics");            ///< Menu input
DEF_MENU_SEPARATOR(logSeparator1, 15, "< Statistics >");     ///< Menu separator
DEF_MENU_BYTE_ACT(logStatSamples, 15, "Num. Samples",
                  k197dev.setNsamples(getValue());); ///< Menu input

UImenuItem *logMenuItems[] = {
    &logSeparator0,  &logEnable, &logSkip, &logSplitUnit,
    &logTimestamp,   &logTamb,   &logStat, &logSeparator1,
    &logStatSamples, &closeMenu, &exitMenu}; ///< Datalog menu items

DEF_MENU_SEPARATOR(graphSeparator0, 15, "< Graph options >"); ///< Menu separator

DEF_MENU_OPTION(opt_gr_type_lines, OPT_GRAPH_TYPE_LINES, 0, "Lines");
DEF_MENU_OPTION(opt_gr_type_dots,  OPT_GRAPH_TYPE_DOTS,  1, "Dots");
DEF_MENU_OPTION_INPUT(opt_gr_type, 15, "Graph type", OPT(opt_gr_type_lines), OPT(opt_gr_type_dots));

DEF_MENU_SEPARATOR(graphSeparator1, 15, "< Y axis >"); ///< Menu separator
BIND_MENU_OPTION(opt_gr_yscale_max, k197graph_yscale_zoom, "zoom");
BIND_MENU_OPTION(opt_gr_yscale_zero, k197graph_yscale_zero, "Incl. 0");
BIND_MENU_OPTION(opt_gr_yscale_prefsym, k197graph_yscale_prefsym, "Symmetric");
BIND_MENU_OPTION(opt_gr_yscale_0sym, k197graph_yscale_0sym, "0+symm");
BIND_MENU_OPTION(opt_gr_yscale_forcesym, k197graph_yscale_forcesym, "Force symm.");
DEF_MENU_ENUM_INPUT(k197graph_yscale_opt, opt_gr_yscale, 15, "Y axis", OPT(opt_gr_yscale_max), OPT(opt_gr_yscale_zero), OPT(opt_gr_yscale_prefsym), OPT(opt_gr_yscale_0sym), OPT(opt_gr_yscale_forcesym));
  
DEF_MENU_BOOL(gr_yscale_show0, 15, "Show y=0");

DEF_MENU_SEPARATOR(graphSeparator2, 15, "< X axis >"); ///< Menu separator
DEF_MENU_BOOL(gr_xscale_roll_mode, 15, "Roll mode");       ///< Menu input
DEF_MENU_BOOL_ACT(gr_xscale_autosample, 15, "Auto sample",
                  k197dev.setAutosample(getValue()););       ///< Menu input
DEF_MENU_BYTE_SETGET(gr_sample_time, 15, "Sample time (s)",
                  k197dev.setGraphPeriod(newValue);,
                  return k197dev.getGraphPeriod(););         ///< Menu input

UImenuItem *graphMenuItems[] = {
    &graphSeparator0, &opt_gr_type, 
    &graphSeparator1, &opt_gr_yscale, &gr_yscale_show0, 
    &graphSeparator2, &gr_xscale_roll_mode, &gr_xscale_autosample, &gr_sample_time, &closeMenu, &exitMenu};

/*!
      @brief set the display contrast

      @details in addition to setting the contrast value, this method takes care
   of keeping the contrast menu item in synch

      @param value contrast value (0 to 255)
*/
void UImanager::setContrast(uint8_t value) {
  u8g2.setContrast(value);
  contrastCtrl.setValue(value);
  dxUtil.checkFreeStack();
}

/*!
      @brief  setup the menu
      @details this method setup all the menus. It must be called before the
   menu can be displayed.
*/
void UImanager::setupMenus() {
  additionalModes.setValue(true);
  reassignStoRcl.setValue(true);
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

  //gr_sample_time.setValue(k197dev.getGraphPeriod());
  gr_xscale_roll_mode.setValue(true);
  gr_xscale_autosample.setValue(k197dev.getAutosample());

  permadata::retrieve_from_EEPROM();
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
  dxUtil.checkFreeStack();
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
      @param buf a buffer where the formatted string should be written
      @param f the number to be formatted
      @return a nul terminated char array with the formatted number (same as
   buf)
*/
const char *UImanager::formatNumber(char buf[K197_RAW_MSG_SIZE + 1], float f) {
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
  return dtostrf(f, K197_RAW_MSG_SIZE, ndec, buf);
}

/*!
    @brief  data logging to Serial
    @details does the actual data logging when called. It has no effect if
   datalogging is disabled or in no connection has been detected
*/
void UImanager::logData() {
  if (k197dev.isCal()) // No logging while in Cal mode
    return;
  if ((!logEnable.getValue()) || (!BTman.validconnection()))
    return;
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
    char buf[K197_RAW_MSG_SIZE + 1];
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
    char buf[K197_RAW_MSG_SIZE + 1];
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
  dxUtil.checkFreeStack();
}

// ***************************************************************************************
//  Graph screen handling
// ***************************************************************************************

static k197graph_type k197graph;
//                            0   1   2   3   4   5   6
static const char prefix[]={'n','u','m',' ','k','M','G'};
static inline char getPrefix(int8_t pow10) {
   int8_t index = pow10 >=0 ? pow10/3 + 3 : (pow10+1)/3+2;
   return prefix[index];
}

static inline int8_t getZeroes(int8_t pow10) {
   //int8_t nz = pow10 >=0 ? pow10 % 3 : 2+((pow10+1)%3);
   //DebugOut.print(F("pow10="));DebugOut.println(pow10);
   //DebugOut.print(F("nz="));DebugOut.println(nz);   
   //return nz;
   return pow10 >=0 ? pow10 % 3 : 2+((pow10+1)%3);
} 

static void printYLabel(k197graph_label_type l) {
   int8_t pow10_effective = l.pow10+k197dev.getUnitPow10();
   u8g2.print(l.mult);

   int8_t nzeroes = getZeroes(pow10_effective);
   for (uint8_t i=0; i<nzeroes; i++) {
      u8g2.print('0');  
   }
   //DebugOut.print(F("pow10_effective="));DebugOut.println(pow10_effective);
   u8g2.print(getPrefix(pow10_effective));
}

static void printXYLabel(k197graph_label_type l, uint16_t nseconds) {
   //DebugOut.print(F("printXYLabel nseconds=")); DebugOut.println(nseconds);
   u8g2.print(nseconds); u8g2.print(F("s/"));
  
   int8_t pow10_effective = l.pow10+k197dev.getUnitPow10();
   u8g2.print(l.mult);

   int8_t nzeroes = getZeroes(pow10_effective);
   for (uint8_t i=0; i<nzeroes; i++) {
      u8g2.print('0');  
   }
   //DebugOut.print(F("pow10_effective="));DebugOut.println(pow10_effective);
   u8g2.print(getPrefix(pow10_effective));
}

/*!
    @brief draw a marker at a specific point in the graph
    @details: can print one of the following marker types:
    - MARKER
    - CURSOR_A
    - CURSOR_B
    Note that the font used to print the identity of the cursor marker (A or B) must be set before calling this function
    @param x the x coordinate of the point where to place the mark
    @param y the y coordinate of the point where to place the mark
    @param marker_type market type
*/
void UImanager::drawMarker(u8g2_uint_t x, u8g2_uint_t y, char marker_type) {
  static const u8g2_uint_t marker_size = 7;
  //k197graph_type::x_size
  u8g2_uint_t x0 = x < marker_size ? 0 : x -  marker_size;
  u8g2_uint_t x1 = k197graph_type::x_size<(x+marker_size) ? k197graph_type::x_size : x+marker_size;
  u8g2_uint_t y0 = y < marker_size ? 0 : y -  marker_size;
  u8g2_uint_t y1 = k197graph_type::y_size<(y+marker_size) ? k197graph_type::y_size : y+marker_size;
  switch(marker_type) {
     case UImanager::MARKER:
         u8g2.drawLine(x0, y, x1, y);
         u8g2.drawLine(x, y0, x, y1);
         break;
     case UImanager::CURSOR_A:
         u8g2.drawLine(x0, y0, x, y);
         u8g2.drawLine(x, y, x1, y1);
         u8g2.drawLine(x0, y1, x, y);
         u8g2.drawLine(x, y, x1, y0);
         if ( int(y1) > (k197graph_type::y_size-u8g2.getMaxCharHeight()) ) { 
             u8g2.setCursor(x0, y0-u8g2.getMaxCharHeight()); // Position above the marker
         } else {
             u8g2.setCursor(x0, y1); // Position below the marker          
         }
         u8g2.print(marker_type);
         if (getActiveCursor() == marker_type) u8g2.print('<');
         break;
     case UImanager::CURSOR_B:
         u8g2.drawLine(x0, y, x1, y);
         u8g2.drawLine(x, y0, x, y1);
         u8g2.drawFrame(x0, y0, x1-x0, y1-y0);
         bool actv = getActiveCursor() == marker_type;
         if ( int(y1) > (k197graph_type::y_size-u8g2.getMaxCharHeight()) ) { 
             u8g2.setCursor(x1-u8g2.getMaxCharWidth()*(actv ? 2 : 1), y0-u8g2.getMaxCharHeight()); // Position above the marker
         } else {
             u8g2.setCursor(x1-u8g2.getMaxCharWidth(), y1); // Position below the marker          
         }
         if (actv) u8g2.print('>');
         u8g2.print(marker_type);
         break;
  }
}

#define IS_DISPLAY_ROLL_MODE() (xscale==1) && (k197graph.npoints == k197graph_type::x_size) && gr_xscale_roll_mode.getValue()

/*!
    @brief  update the display, used when in graph mode
   screen.
*/
void UImanager::updateGraphScreen() {
  u8g2.clearBuffer(); // Clear display area
  
  // Get graph data
  k197dev.fillGraphDisplayData(&k197graph, opt_gr_yscale.getValue()); 

  // autoscale x axis
  uint16_t i1 = 16;
  while (i1<k197graph.npoints) i1*=2;    
  if (i1>k197graph.x_size) i1=k197graph.x_size;
  //DebugOut.print(F("i1 ")); DebugOut.print(i1); DebugOut.print(F(", npoints ")); DebugOut.println(k197graph.npoints);
  byte xscale = k197graph.x_size / i1;

  // Draw the axis
  //u8g2.drawLine(0, k197graph.y_size, k197graph.x_size, k197graph.y_size); // X axis
  u8g2.drawLine(k197graph.x_size, k197graph.y_size, k197graph.x_size, 0); // Y axis
  if (gr_yscale_show0.getValue() && k197graph.y0.isNegative() && k197graph.y1.isPositive())
      drawDottedHLine(0, k197graph.y_size-k197graph.y_zero, k197graph.x_size); // zero axis

  u8g2.setFont(u8g2_font_6x12_mr);
  //Clear the space normally occupied by the axis labels  
  u8g2.setDrawColor(0);
  u8g2.drawBox( k197graph.x_size+2, k197graph.y_size-u8g2.getMaxCharHeight(), 256,  k197graph.y_size);
  u8g2.drawBox( k197graph.x_size+2, 0, 256, u8g2.getMaxCharHeight());
  //Draw axis labels  
  u8g2.setDrawColor(1);
  u8g2.setCursor(k197graph.x_size+2, k197graph.y_size-u8g2.getMaxCharHeight());
  uint16_t nseconds = gr_sample_time.getValue();
  printXYLabel(k197graph.y0, nseconds == 0 ? i1/3 : i1 * nseconds);  
  u8g2_uint_t botln_x = u8g2.tx;
  u8g2.setCursor(k197graph.x_size+2, 0);
  printYLabel(k197graph.y1);
  u8g2_uint_t topln_x = u8g2.tx;
      
  if ( IS_DISPLAY_ROLL_MODE() ) {  // Draw the graph in roll mode
      if (opt_gr_type.getValue() == OPT_GRAPH_TYPE_DOTS || k197graph.npoints<2) {
          for (int i=0; i<k197graph.npoints; i++) {
              u8g2.drawPixel(i, k197graph.y_size-k197graph.point[k197graph.idx(i)]);
          }
      } else { // OPT_GRAPH_TYPE_LINES && k197graph.npoints>=2  
          for (int i=0; i<(k197graph.npoints-1); i++) {
              u8g2.drawLine(i, k197graph.y_size-k197graph.point[k197graph.idx(i)], i+1, k197graph.y_size-k197graph.point[k197graph.idx(i+1)]);
          }
      }    
      //drawMarker(k197graph.x_size, k197graph.y_size-k197graph.point[k197graph.current_idx]);
  } else { // Draw the graph in overwrite mode
      if (opt_gr_type.getValue() == OPT_GRAPH_TYPE_DOTS || k197graph.npoints<2) {
          for (int i=0; i<k197graph.npoints; i++) {
              u8g2.drawPixel(xscale*i, k197graph.y_size-k197graph.point[i]);
          }
      } else { // OPT_GRAPH_TYPE_LINES && k197graph.npoints>=2  
          for (int i=0; i<(k197graph.npoints-1); i++) {
              u8g2.drawLine(xscale*i, k197graph.y_size-k197graph.point[i], xscale*(i+1), k197graph.y_size-k197graph.point[i+1]);
          }
      }    
      drawMarker(xscale*k197graph.current_idx, k197graph.y_size-k197graph.point[k197graph.current_idx]);
  }

  u8g2_uint_t ax = cursor_a > k197graph.npoints ? k197graph.npoints-1 : cursor_a;
  u8g2_uint_t bx = cursor_b > k197graph.npoints ? k197graph.npoints-1 : cursor_b;

  if (areCursorsVisible() && k197graph.npoints>0) {
      if (IS_DISPLAY_ROLL_MODE()) { // Draw the cursors in roll mode
          drawMarker(xscale*ax, k197graph.y_size-k197graph.point[k197graph.idx(ax)],CURSOR_A);
          drawMarker(xscale*bx, k197graph.y_size-k197graph.point[k197graph.idx(bx)],CURSOR_B);                
      } else { // Draw the cursors in overwrite mode
          drawMarker(xscale*ax, k197graph.y_size-k197graph.point[ax],CURSOR_A);
          drawMarker(xscale*bx, k197graph.y_size-k197graph.point[bx],CURSOR_B);        
      }
  }
  if (areCursorsVisible() && k197graph.npoints>0) {
      if (IS_DISPLAY_ROLL_MODE()) { // Translate considering we are in roll mode
           ax=k197graph.idx(ax); 
           bx=k197graph.idx(bx); 
      }
      drawGraphScreenCursorPanel(topln_x, botln_x, ax, bx);
  } else {
      drawGraphScreenNormalPanel(topln_x, botln_x);
  }
  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
    @brief  draw a small panel showing the measurement value 
    @details this method is called from updateGraphScreen(). 
    Used when in graph mode screen when the cursors are hidden
    The top and bottom line include the axis labels, which have variable lenght
    and are printed by updateGraphScreen(). The start cooordinate for those lines
    is passed in the two arguments topln_x and botln_x
    @param topln_x the first usable x coordinates of the top line of the panel
    @param botln_x the first usable x coordinates of the bottom line of the panel
*/
void UImanager::drawGraphScreenNormalPanel(u8g2_uint_t topln_x, u8g2_uint_t botln_x) {
  u8g2_uint_t x = 185+10;
  u8g2_uint_t y = 3;
  u8g2.setFont(u8g2_font_5x7_mr);
  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);

  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197dev.getUnit(true));
  y += u8g2.getMaxCharHeight();

  u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setCursor(u8g2.tx, u8g2.ty + 1);
  if (k197dev.isAC())
    u8g2.print(F(" AC"));
  else
    u8g2.print(F("   "));
  if (k197dev.isREL())
    u8g2.print(F(" REL"));


  x = 185+5;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_8x13_mr);
  if (k197dev.isNumeric()) {
    char buf[K197_RAW_MSG_SIZE + 1];
    u8g2.print(formatNumber(buf, k197dev.getValue()));
  } else {
    u8g2.print(k197dev.getRawMessage());
  }

  // Draw AUTO & HOLD at about the same height as the Y label
  
  u8g2.setFont(u8g2_font_5x7_mr);
  x=topln_x + 5;
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
    The top and bottom line include the axis labels, which have variable lenght
    and are printed by updateGraphScreen(). The start cooordinate for those lines
    is passed in the two arguments topln_x and botln_x
    @param topln_x the first usable x coordinates of the top line of the panel
    @param botln_x the first usable x coordinates of the bottom line of the panel
    @param ax the graph index corresponding to cursor A 
    @param bx the graph index corresponding to cursor B
*/
void UImanager::drawGraphScreenCursorPanel(u8g2_uint_t topln_x, u8g2_uint_t botln_x, u8g2_uint_t ax, u8g2_uint_t bx) {
  u8g2.setCursor(topln_x+1, 0);

  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197dev.getUnit(true));
  
  u8g2.setFont(u8g2_font_5x7_mr);
  u8g2.setCursor(u8g2.tx+2, 3);
  if (k197dev.isAC())
    u8g2.print(F("AC"));
  else
    u8g2.print(CH_SPACE);
  u8g2.setCursor(u8g2.tx+2, u8g2.ty);
  if (k197dev.isREL())
    u8g2.print(F("REL"));

  u8g2.setCursor(183, u8g2.ty+u8g2.getMaxCharHeight()+3);
  u8g2.print(CURSOR_A); u8g2.print(CH_SPACE); 
  u8g2.print(k197dev.getGraphValue(ax), 6);   

  u8g2.setCursor(183, u8g2.ty+u8g2.getMaxCharHeight()+2);
  u8g2.print(CURSOR_B); u8g2.print(CH_SPACE); 
  u8g2.print(k197dev.getGraphValue(bx), 6);    

  /*if (GPIOR3 & 0x10) {   // debug flag is set
      DebugOut.print(F("Graph: N=")); DebugOut.print(k197graph.npoints); DebugOut.print(F(", i=")); DebugOut.print(k197graph.current_idx);
      DebugOut.print(F(", s=")); DebugOut.println(k197graph.nsamples_graph);
      DebugOut.print(F("A =")); DebugOut.print(cursor_a); DebugOut.print(F(", B =")); DebugOut.println(cursor_b);
      DebugOut.print(F("OLD ax=")); DebugOut.print(ax); DebugOut.print(F(", bx=")); DebugOut.println(bx);
  }*/
  
  // for the next calculations, we need the logical index, regardless how the graph is plot
  u8g2_uint_t logic_ax=k197graph.logic_index(ax);
  u8g2_uint_t logic_bx=k197graph.logic_index(bx); 
  uint16_t deltax = logic_ax > logic_bx ? logic_ax - logic_bx : logic_bx - logic_ax;

  u8g2.setCursor(183, u8g2.ty+u8g2.getMaxCharHeight()+2);
  u8g2.print(F("Avg")); u8g2.print(CH_SPACE); 
  u8g2.print(0.0, 6);    

  /*if (GPIOR3 & 0x10) {   // debug flag is set
      GPIOR3 &= (~0x10); // reset flag: we print only once
      DebugOut.print(F("NEW ax=")); DebugOut.print(ax); DebugOut.print(F(", bx=")); DebugOut.print(bx);
      DebugOut.print(F(", deltax=")); DebugOut.println(deltax);
  }*/
  
  u8g2.setCursor(183, u8g2.ty+u8g2.getMaxCharHeight()+2);
  u8g2.print(F("Dt")); u8g2.print(CH_SPACE); 
  if (k197graph.nsamples_graph==0) {
     u8g2.print(float(deltax)/3.0, 2);
  } else {
     u8g2.print(deltax*k197graph.nsamples_graph); 
  }  
  u8g2.print(CH_SPACE); u8g2.print('s');
  
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
  if (eventSource == K197key_REL &&
      eventType == UIeventLongPress &&
      ! ( isGraphMode() && areCursorsVisible() ) 
      ) { // This event is handled the same in all
                                       // other screen modes
    if (isFullScreen())
      showOptionsMenu();
    else
      showFullScreen();
    return true; // Skip normal handling in the main sketch
  }
  if (isMenuVisible()) {
    if (UIwindow::getcurrentWindow()->handleUIEvent(eventSource, eventType))
      return true; // Skip normal handling in the main sketch
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
          k197dev.setDisplayHold(!k197dev.getDisplayHold());
          //GPIOR3 |= 0x10; // set debug flag
        } else if (eventType == UIeventLongPress) {
          K197screenMode screen_mode = uiman.getScreenMode();
          if (screen_mode != K197sc_minmax)
            uiman.setScreenMode(K197sc_minmax);
          else
            uiman.setScreenMode(K197sc_normal);
        } else if (eventType == UIeventDoubleClick) {
          K197screenMode screen_mode = uiman.getScreenMode();
          if (screen_mode != K197sc_graph)
            uiman.setScreenMode(K197sc_graph);
          else
            uiman.setScreenMode(K197sc_normal);          
        }
        return true; // Skip normal handling in the main sketch
      }
      break;
    case K197key_RCL:
      if (reassignStoRcl.getValue()) {
          if (eventType == UIeventClick) {
             if (isGraphMode() && areCursorsVisible()) toggleActiveCursor();
          } else if (eventType == UIeventLongPress) {
             if (isGraphMode()) toggleCursorsVisibility();
          } else if (eventType == UIeventDoubleClick) {
             DebugOut.print(F("Max loop (us): "));
             DebugOut.println(looptimerMax);
             looptimerMax = 0UL;
          }
          return true; // Skip normal handling in the main sketch       
      }
      break;
    case K197key_REL:
      if ( isGraphMode() && areCursorsVisible() ) {
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
          k197dev.resetStatistics();
          // DebugOut.print('x');
          return true; // Skip normal handling in the main sketch
      }
      break;
    case K197key_DB:
      if ( isGraphMode() && areCursorsVisible() ) {
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
  dxUtil.checkFreeStack();
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
  dxUtil.checkFreeStack();
  bool_options.additionalModes = additionalModes.getValue();
  bool_options.reassignStoRcl = reassignStoRcl.getValue();
  bool_options.logEnable = logEnable.getValue();
  bool_options.logSplitUnit = logSplitUnit.getValue();
  bool_options.logTimestamp = logTimestamp.getValue();
  bool_options.logTamb = logTamb.getValue();
  bool_options.logStat = logStat.getValue();
  byte_options.contrastCtrl = contrastCtrl.getValue();
  byte_options.logSkip = logSkip.getValue();
  byte_options.logStatSamples = logStatSamples.getValue();
}

/*!
    @brief copy data from the structure to the GUI
    @details the new options take affect immediately.
    This function is used inside retrieve_from_EEPROM
*/
void permadata::copyToUI() {
  dxUtil.checkFreeStack();
  additionalModes.setValue(bool_options.additionalModes);
  reassignStoRcl.setValue(bool_options.reassignStoRcl);
  logEnable.setValue(bool_options.logEnable);
  logSplitUnit.setValue(bool_options.logSplitUnit);
  logTimestamp.setValue(bool_options.logTimestamp);
  logTamb.setValue(bool_options.logTamb);
  logStat.setValue(bool_options.logStat);
  uiman.setContrast(byte_options.contrastCtrl);
  logSkip.setValue(byte_options.logSkip);
  logStatSamples.setValue(byte_options.logStatSamples);
  k197dev.setNsamples(byte_options.logStatSamples);
}

/*!
    @brief  store all configuration options from the EEPROM
    @details a confirmation or error message is sent to DebugOut
    @return true if the operation is succesful
*/
bool permadata::store_to_EEPROM() {
  if ((EEPROM_BASE_ADDRESS + sizeof(permadata)) > EEPROM.length()) {
    DebugOut.print(F(" Data size "));
    DebugOut.print(sizeof(permadata));
    DebugOut.print(F(" exceed EEPROM len: "));
    DebugOut.print(EEPROM.length());
    return false;
  }
  permadata pdata;
  pdata.copyFromUI();
  EEPROM.put(EEPROM_BASE_ADDRESS, pdata);
  DebugOut.println(F("EEPROM: store ok"));
  return true;
}

/*!
    @brief  retrieve all configuration options to the EEPROM
    @details if successful, the new options take affect immediately.
    A confirmation or error message is sent to DebugOut
    @return true if the operation is succesful
*/
bool permadata::retrieve_from_EEPROM() {
  if ((EEPROM_BASE_ADDRESS + sizeof(permadata)) > EEPROM.length()) {
    DebugOut.print(F("EEPROM: Data size="));
    DebugOut.print(sizeof(permadata));
    DebugOut.print(F(" exceed "));
    DebugOut.println(EEPROM.length());
    return false;
  }
  permadata pdata;
  EEPROM.get(EEPROM_BASE_ADDRESS, pdata);
  if (pdata.magicNumber != magicNumberExpected) {
    DebugOut.println(F("EEPROM: no data"));
    return false;
  }
  if (pdata.revision != revisionExpected) {
    DebugOut.print(F("EEPROM: rev. "));
    DebugOut.print(pdata.revision, HEX);
    DebugOut.print(F(", expected "));
    DebugOut.println(revisionExpected, HEX);
    return false;
  }
  pdata.copyToUI();
  DebugOut.println(F("EEPROM: restore ok"));
  return true;
}
