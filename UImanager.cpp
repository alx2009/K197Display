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

#include "BTmanager.h"
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

/*!
    @brief  this function is intended to include any drawing command that may be
   needed the very first time the display is used

    Called from setup, it should not be called from elsewhere
*/
void setup_draw(void) {
  // u8g2.setFont(u8g2_font_inr38_mf);  // width = 31 points (7 characters=217
  // points)
  u8g2.setFont(
      u8g2_font_inr30_mf); // width =25  points (7 characters=175 points)
  u8g2.setFontMode(0);
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontRefHeightExtendedText();
  u8g2.setFontDirection(0);

  u8g2.setFont(u8g2_font_6x12_mr);
  DebugOut.print(F("Disp. H="));
  DebugOut.print(u8g2.getDisplayHeight());
  DebugOut.print(F(", font h="));
  DebugOut.print(u8g2.getAscent());
  DebugOut.print(F("/"));
  DebugOut.print(u8g2.getDescent());
  DebugOut.print(F("/"));
  DebugOut.println(u8g2.getMaxCharHeight());
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
  // u8g2.setBusClock(12000000); // without this call the default clock is about
  // 5 MHz, which is more than good enough.
  u8g2.begin();
  setContrast(DEFAULT_CONTRAST);
  u8g2.enableUTF8Print();
  u8g2log.begin(U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);

  u8g2.clearBuffer();
  setup_draw();
  u8g2.sendBuffer();

  setupMenus();
}

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
  switch (screen_mode) {
      case K197sc_normal: updateNormalScreen(); break;
      case K197sc_minmax: updateMinMaxScreen(); break;
      default:            updateSplitScreen();  break;
  }
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
  u8g2.setFont(u8g2_font_9x18_mr);
  u8g2.print(k197dev.getMessage());
  u8g2.print(CH_SPACE);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197dev.getUnit(true));
  y += u8g2.getMaxCharHeight();
  u8g2.setFont(u8g2_font_9x18_mr);
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

  if (screen_mode == K197sc_debug) {
    u8g2.setFont(u8g2_font_5x7_mr); // set the font for the terminal window
    u8g2.drawLog(0, 0, u8g2log);    // draw the terminal window on the display
  } else {
    UImenu::getCurrentMenu()->draw(&u8g2, 0, 10);
  }
  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
    @brief  update the display, used when in normal screen mode. See also updateDisplay().
*/
void UImanager::updateNormalScreen() {
  u8g2.setFont(
      u8g2_font_inr30_mf); // width =25  points (7 characters=175 points)
  const unsigned int xraw = 49;
  const unsigned int yraw = 15;
  unsigned int dpsz_x = 3;  // decimal point size in x direction
  unsigned int dpsz_y = 3;  // decimal point size in y direction
  unsigned int dphsz_x = 2; // decimal point "half size" in x direction
  unsigned int dphsz_y = 2; // decimal point "half size" in y direction

  u8g2.drawStr(xraw, yraw, k197dev.getRawMessage());
  for (byte i = 1; i <= 7; i++) {
    if (k197dev.isDecPointOn(i)) {
      u8g2.drawBox(xraw + i * u8g2.getMaxCharWidth() - dphsz_x,
                   yraw + u8g2.getAscent() - dphsz_y, dpsz_x, dpsz_y);
    }
  }

  // set the unit
  // u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  const unsigned int xunit = 229;
  const unsigned int yunit = 20;
  u8g2.setCursor(xunit, yunit);
  u8g2.print(k197dev.getUnit());

  // set the AC/DC indicator
  // u8g2.setFont(u8g2_font_10x20_mf);
  u8g2.setFont(u8g2_font_9x18_mr);
  const unsigned int xac = xraw + 3;
  const unsigned int yac = 40;
  u8g2.setCursor(xac, yac);
  if (k197dev.isAC())
    u8g2.print(F("AC"));
  else
    u8g2.print(F("  "));

  // set the other announciators
  // u8g2.setFont(u8g2_font_6x12_mr);
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

  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);
  if (k197dev.isSTO())
    u8g2.print(F("STO"));
  else
    u8g2.print(F("   "));

  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);
  if (k197dev.isRCL())
    u8g2.print(F("RCL"));
  else
    u8g2.print(F("   "));

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
  if (k197dev.isTKModeActive()) { // Display local temperature
    char buf[K197_MSG_SIZE];
    dtostrf(k197dev.getTColdJunction(), K197_MSG_SIZE - 1, 2, buf);
    u8g2.print(buf);
    u8g2.print(k197dev.getUnit());
  } else {
    u8g2.print(F("          "));
  }

  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
    @brief  update the display, used when in minmax screen mode. See also updateDisplay().
*/
void UImanager::updateMinMaxScreen() {
  u8g2.setFont(
      u8g2_font_inr30_mf); // width =25  points (7 characters=175 points)
  const unsigned int xraw = 49;
  const unsigned int yraw = 15;
  unsigned int dpsz_x = 3;  // decimal point size in x direction
  unsigned int dpsz_y = 3;  // decimal point size in y direction
  unsigned int dphsz_x = 2; // decimal point "half size" in x direction
  unsigned int dphsz_y = 2; // decimal point "half size" in y direction

  u8g2.drawStr(xraw, yraw, k197dev.getRawMessage());
  for (byte i = 1; i <= 7; i++) {
    if (k197dev.isDecPointOn(i)) {
      u8g2.drawBox(xraw + i * u8g2.getMaxCharWidth() - dphsz_x,
                   yraw + u8g2.getAscent() - dphsz_y, dpsz_x, dpsz_y);
    }
  }

  // set the unit
  // u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  const unsigned int xunit = 229;
  const unsigned int yunit = 20;
  u8g2.setCursor(xunit, yunit);
  u8g2.print(k197dev.getUnit(true));

  // set the AC/DC indicator
  // u8g2.setFont(u8g2_font_10x20_mf);
  u8g2.setFont(u8g2_font_9x18_mr);
  const unsigned int xac = xraw + 3;
  const unsigned int yac = 40;
  u8g2.setCursor(xac, yac);
  if (k197dev.isAC())
    u8g2.print(F("AC"));
  else
    u8g2.print(F("  "));

  // set the other announciators
  // u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setFont(u8g2_font_8x13_mr);
  unsigned int x = 0;
  unsigned int y = 5;
  u8g2.setFont(u8g2_font_8x13_mr);
  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);
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

  x = 140;
  y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (k197dev.isTKModeActive()) { // Display local temperature
    char buf[K197_MSG_SIZE];
    dtostrf(k197dev.getTColdJunction(), K197_MSG_SIZE - 1, 2, buf);
    u8g2.print(buf);
    u8g2.print(k197dev.getUnit());
  } else {
    u8g2.print(F("          "));
  }

  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
      @brief set the screen mode

   @details  Three modes are defined: normal mode, menu mode and debug mode.
   In debug and menu mode the measurements are displays on the right of the
   screen (split screen), while the left part is reserved for debug messages and
   menu items respectively. Normal mode is a fulol screen mode equivalent to the original K197 display. 
   As the name suggest debug mode is intended for
   debugging the code that interacts with the serial port/bluetooth module
   itself, so that Serial cannot be used.

      @param mode can be displayNormal or displayDebug for normal and debug mode
   respectively
*/
void UImanager::setScreenMode(K197screenMode mode) {
  screen_mode = mode;
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  dxUtil.checkFreeStack();
}

/*!
      @brief display the BT module status (detected or not detected)

      @param present true if a BT module is detected, false otherwise
      @param connected true if a BT connection is detected, false otherwise
*/
void UImanager::updateBtStatus() {
  if (screen_mode != K197sc_normal)
    return;
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
//  Menu definition/handling
// ***************************************************************************************

DEF_MENU_CLOSE(closeMenu, 15,
               "< Back"); ///< Menu close action (used in multiple menus)
DEF_MENU_ACTION(
    exitMenu, 15, "Exit",
    uiman.setScreenMode(
        K197sc_normal);); ///< Menu close action (used in multiple menus)

DEF_MENU_SEPARATOR(mainSeparator0, 15, "< Options >"); ///< Menu separator
DEF_MENU_BOOL(additionalModes, 15, "Extra Modes");     ///< Menu input
DEF_MENU_BOOL(reassignStoRcl, 15, "Reassign STO/RCL"); ///< Menu input
DEF_MENU_OPEN(btDatalog, 15, "Data logging >>>", &UIlogMenu); ///< Open submenu
DEF_MENU_BUTTON(bluetoothMenu, 15,
                "Bluetooth"); ///< TBD: submenu not yet implemented
DEF_MENU_BYTE_ACT(contrastCtrl, 15, "Contrast",
                  u8g2.setContrast(getValue());); ///< set contrast
DEF_MENU_BUTTON(saveSettings, 15,
                "Save settings"); ///< TBD: submenu not yet implemented
DEF_MENU_BUTTON(reloadSettings, 15,
                "Reload settings"); ///< TBD: submenu not yet implemented
DEF_MENU_ACTION(openLog, 15, "Show log",
                dxUtil.reportStack();
                uiman.setScreenMode(K197sc_debug);); ///< TBD: show debug log

UImenuItem *mainMenuItems[] = {
    &mainSeparator0, &additionalModes, &reassignStoRcl, &btDatalog,
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
  if (screen_mode == K197sc_mainMenu) {
    if (UImenu::getCurrentMenu()->handleUIEvent(eventSource, eventType))
      return true;
    if (eventSource == K197key_REL && eventType == UIeventLongPress) {
      setScreenMode(K197sc_normal);
      return true;
    }
  } else if (screen_mode == K197sc_debug) {
    if (eventType == UIeventClick || eventType == UIeventLongClick) {
      setScreenMode(K197sc_normal);
      return true;
    }
  }
  return false;
  dxUtil.checkFreeStack();
}

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
      @brief  query if the extra modes are enabled
      @details extra mode are modes that are implemented in this application
   rather than the original k197 device
      @return returns true if logging is active
*/
bool UImanager::isExtraModeEnabled() { return additionalModes.getValue(); };

/*!
      @brief  query if the STO and RCL buttons should be re-assigned to other
   functions
      @details note that this method only keeps track of the setting in the UI.
      @return returns true if logging is active
*/
bool UImanager::reassignStoRcl() { return ::reassignStoRcl.getValue(); }

/*!
      @brief  setup the menu
      @details this method setup all the menus. It must be called before the
   menu can be displayed.
*/
void UImanager::setupMenus() {
  additionalModes.setValue(true);
  ::reassignStoRcl.setValue(true);
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
}

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
const char *formatNumber(char buf[K197_MSG_SIZE], float f) {
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
  return dtostrf(f, K197_MSG_SIZE - 1, ndec, buf);
}

/*!
    @brief  data logging to Serial
    @details does the actual data logging when called. It has no effect if
   datalogging is disabled or in no connection has been detected
*/
void UImanager::logData() {
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
  Serial.print(k197dev.getMessage());
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
    char buf[K197_MSG_SIZE];
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
