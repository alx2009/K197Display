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

#include "UImanager.h"
#include "debugUtil.h"
#include "pinout.h"

/*!
    @brief  Constructor, see
   https://github.com/olikraus/u8g2/wiki/setup_tutorial

   @return no return value
*/
// At the moment we do not use the RESET pin. It does seem to work correctly without.
#ifdef OLED_DC //4Wire SPI
   U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI
   u8g2(U8G2_R0, OLED_SS,
       OLED_DC /*,reset_pin*/); ///< u8g2 object. See pinout.h for pin definition.
#else // 3Wire SPI
   U8G2_SSD1322_NHD_256X64_F_3W_HW_SPI
   u8g2(U8G2_R0, OLED_SS
       /*,reset_pin*/); ///< u8g2 object. See pinout.h for pin definition.
#endif

#define U8LOG_WIDTH 28                            ///< Size of the log window
#define U8LOG_HEIGHT 9                            ///< Height of the log window
uint8_t u8log_buffer[U8LOG_WIDTH * U8LOG_HEIGHT]; ///< buffer for the log window
U8G2LOG u8g2log;                                  ///< the log window

/*!
    @brief  Constructor for the class

   @param k197 pointer to the K197device object to display
*/
UImanager::UImanager(K197device *k197) { this->k197 = k197; }

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
  DebugOut.print(F("Disp. H=")); DebugOut.print(u8g2.getDisplayHeight());
  DebugOut.print(F(", font h=")); DebugOut.print(u8g2.getAscent()); 
  DebugOut.print(F("/")); DebugOut.print(u8g2.getDescent());
  DebugOut.print(F("/")); DebugOut.println(u8g2.getMaxCharHeight());
}

/*!
    @brief  setup the display and clears the screen. Must be called before any
   other member function
*/
void UImanager::setup() {
  pinMode(OLED_MOSI, OUTPUT); // Needed to work around a bug in the micro or
                              // dxCore with certain swap options
  bool pmap =
      SPI.swap(OLED_SPI_SWAP_OPTION); // See pinout.h for pin definition
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

  setupMainMenu();
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
  if (screen_mode == K197sc_normal)
    updateDisplayNormal();
  else
    updateDisplaySplit();
}

/*!
    @brief  update the display, used when in debug and other modes with split
   screen.
*/
void UImanager::updateDisplaySplit() {
  u8g2_uint_t x = 147;
  u8g2_uint_t y = 5;
  u8g2.setFont(u8g2_font_8x13_mr);
  u8g2.setCursor(x, y);
  if (k197->isAuto())
    u8g2.print(F("AUTO "));
  else
    u8g2.print(F("     "));
  if (k197->isBAT())
    u8g2.print(F("BAT "));
  else
    u8g2.print(F("    "));
  if (k197->isREL())
    u8g2.print(F("REL "));
  else
    u8g2.print(F("    "));
  if (k197->isdB())
    u8g2.print(F("dB "));
  else
    u8g2.print(F("   "));
  if (k197->isCal())
    u8g2.print(F("Cal   "));
  else
    u8g2.print(F("      "));

  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_9x18_mr);
  u8g2.print(k197->getMessage());
  u8g2.print(' ');
  u8g2.setFont(u8g2_font_9x15_m_symbols);
  u8g2.print(k197->getUnit());
  y += u8g2.getMaxCharHeight();
  u8g2.setFont(u8g2_font_9x18_mr);
  if (k197->isAC())
    u8g2.print(F(" AC   "));
  else
    u8g2.print(F("      "));

  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_8x13_mr);
  if (k197->isSTO())
    u8g2.print(F("STO "));
  else
    u8g2.print(F("    "));
  if (k197->isRCL())
    u8g2.print(F("RCL "));
  else
    u8g2.print(F("    "));
  if (k197->isRMT())
    u8g2.print(F("RMT   "));
  else
    u8g2.print(F("      "));

  if (screen_mode == K197sc_debug) {
      u8g2.setFont(u8g2_font_5x7_mr); // set the font for the terminal window
      u8g2.drawLog(0, 0, u8g2log);    // draw the terminal window on the display
  }  else {
      UImainMenu.draw(&u8g2, 0, 10); 
  }
  u8g2.sendBuffer();
}

/*!
    @brief  update the display, used when in normal mode. See updateDisplay().
*/
void UImanager::updateDisplayNormal() {
  u8g2.setFont(
      u8g2_font_inr30_mf); // width =25  points (7 characters=175 points)
  const unsigned int xraw = 49;
  const unsigned int yraw = 15;
  unsigned int dpsz_x = 3;  // decimal point size in x direction
  unsigned int dpsz_y = 3;  // decimal point size in y direction
  unsigned int dphsz_x = 2; // decimal point "half size" in x direction
  unsigned int dphsz_y = 2; // decimal point "half size" in y direction

  u8g2.drawStr(xraw, yraw, k197->getRawMessage());
  for (byte i = 1; i <= 7; i++) {
    if (k197->isDecPointOn(i)) {
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
  u8g2.print(k197->getUnit());

  // set the AC/DC indicator
  // u8g2.setFont(u8g2_font_10x20_mf);
  u8g2.setFont(u8g2_font_9x18_mr);
  const unsigned int xac = xraw + 3;
  const unsigned int yac = 40;
  u8g2.setCursor(xac, yac);
  if (k197->isAC())
    u8g2.print(F("AC"));
  else
    u8g2.print(F("  "));

  // set the other announciators
  // u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setFont(u8g2_font_8x13_mr);
  unsigned int x = 0;
  unsigned int y = 5;
  u8g2.setCursor(x, y);
  if (k197->isAuto())
    u8g2.print(F("AUTO"));
  else
    u8g2.print(F("    "));
  x = u8g2.tx;
  x += u8g2.getMaxCharWidth() * 2;
  u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setCursor(x, y);
  if (k197->isBAT())
    u8g2.print(F("BAT"));
  else
    u8g2.print(F("   "));

  u8g2.setFont(u8g2_font_8x13_mr);
  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);
  if (k197->isREL())
    u8g2.print(F("REL"));
  else
    u8g2.print(F("   "));
  x = u8g2.tx;
  x += (u8g2.getMaxCharWidth() / 2);
  u8g2.setCursor(x, y);
  if (k197->isdB())
    u8g2.print(F("dB"));
  else
    u8g2.print(F("  "));

  y += u8g2.getMaxCharHeight();
  x = 0;
  u8g2.setCursor(x, y);
  if (k197->isSTO())
    u8g2.print(F("STO"));
  else
    u8g2.print(F("   "));

  y += u8g2.getMaxCharHeight();
  u8g2.setCursor(x, y);
  if (k197->isRCL())
    u8g2.print(F("RCL"));
  else
    u8g2.print(F("   "));

  x = 229;
  y = 0;
  u8g2.setCursor(x, y);
  if (k197->isCal())
    u8g2.print(F("Cal"));
  else
    u8g2.print(F("   "));

  y += u8g2.getMaxCharHeight() * 3;
  u8g2.setCursor(x, y);
  if (k197->isRMT())
    u8g2.print(F("RMT"));
  else
    u8g2.print(F("   "));

  if (k197->isTKModeActive()) { // Display local temperature
      char buf[8];
      dtostrf(k197->getTColdJunction(), K197_MSG_SIZE-1, 2, buf);
      unsigned int x = 140;
      unsigned int y = 2;
      u8g2.setCursor(x, y);
      u8g2.setFont(u8g2_font_5x7_mr);
      u8g2.print(buf); u8g2.print(k197->getUnit());
  }

  u8g2.sendBuffer();
}

/*!
      @brief set the screen mode

      As of now two modes are defined: normal mode and debug mode. In debug mode
   the measurements are displays on the right of the screen, while the left part
   is reserved for debug messages. This is intended for debugging the code that
   interacts with the serial port itself, so that Serial cannot be used.

      @param mode can be displayNormal or displayDebug for normal and debug mode
   respectively
*/
void UImanager::setScreenMode(K197screenMode mode) {
  screen_mode = mode;
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

/*!
      @brief display the BT module status (detected or not detected)

      @param present true if a BT module is detected, false otherwise
      @param connected true if a BT connection is detected, false otherwise
*/
void UImanager::updateBtStatus(bool present, bool connected) {
  if (screen_mode != K197sc_normal)
    return;
  unsigned int x = 95;
  unsigned int y = 2;
  u8g2.setCursor(x, y);
  u8g2.setFont(u8g2_font_5x7_mr);
  if (present) {
    u8g2.print(F("bt "));
  } else {
    u8g2.print(F("   "));
  }
  x += u8g2.getStrWidth("   ");
  u8g2.setCursor(x, y);  
  if (connected && k197->isLogging()) {
    u8g2.print(F("<=>"));
  } else if (connected) {
    u8g2.print(F("<->"));
  } else {
    u8g2.print(F("   "));
  }
  u8g2.sendBuffer();
}

//TODO: documentation
const char  extraModes_txt[] PROGMEM = "Extra Modes";
const char  btDatalog_txt[] PROGMEM = "BT datalog";
const char  bluetoothMenu_txt[] PROGMEM = "Bluetooth";
const char  contrastCtrl_txt[] PROGMEM = "Contrast";
const char  closeMenu_txt[] PROGMEM = "Exit";
const char  saveSettings_txt[] PROGMEM = "Save settings";
const char  reloadSettings_txt[] PROGMEM = "reload settings";
const char  openLog_txt[] PROGMEM = "Show log";

MenuInputBool additionalModes(15, reinterpret_cast<const __FlashStringHelper *>(extraModes_txt));
MenuInputBool btDatalog(15, reinterpret_cast<const __FlashStringHelper *>(btDatalog_txt));
UIMenuButtonItem bluetoothMenu(15, reinterpret_cast<const __FlashStringHelper *>(bluetoothMenu_txt));
MenuInputByte contrastCtrl(15, reinterpret_cast<const __FlashStringHelper *>(contrastCtrl_txt));
UIMenuButtonItem closeMenu(15, reinterpret_cast<const __FlashStringHelper *>(closeMenu_txt));
UIMenuButtonItem saveSettings(15, reinterpret_cast<const __FlashStringHelper *>(saveSettings_txt));
UIMenuButtonItem reloadSettings(15, reinterpret_cast<const __FlashStringHelper *>(reloadSettings_txt));
UIMenuButtonItem openLog(15, reinterpret_cast<const __FlashStringHelper *>(openLog_txt));

UImenuItem *mainMenuItems[] = {&additionalModes, &btDatalog, &bluetoothMenu, &contrastCtrl, &closeMenu, &saveSettings, &reloadSettings, &openLog};

/*!
    @brief  handle UI event

    @details handle UI events (pushbutton presses) that should be handled locally, according to display mode and K197 current status 

    @param eventSource identifies the source of the event (REL, DB, etc.)
    @param eventType identifies the source of the event (REL, DB, etc.)

    @return true if the event has been completely handled, false otherwise
*/
bool UImanager::handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType) {
    if (screen_mode != K197sc_mainMenu) return false;
    if (UImainMenu.handleUIEvent(eventSource, eventType) ) return true;
    const UImenuItem *selectedItem = UImainMenu.getSelectedItem();
    if ( (selectedItem==&contrastCtrl) && (eventType==UIeventRelease) ) { // Possible change of value
        if ( (eventSource==K197key_RCL || eventSource==K197key_STO) ) { // change of value
            u8g2.setContrast(contrastCtrl.getValue());
        }
    }
    if ( (eventSource !=K197key_RCL) || (eventType!=UIeventClick) ) return false;
    // If we are here we have a menu selection event
    if (selectedItem == &closeMenu) {
        setScreenMode(K197sc_normal);
        return true;
    }
    if (selectedItem == &openLog) {
        setScreenMode(K197sc_debug);
        return true;      
    }
    return false;
}

void UImanager::setupMainMenu() {
  additionalModes.setValue(true);
  btDatalog.setValue(true);
  UImainMenu.items = mainMenuItems;
  UImainMenu.num_items = sizeof(mainMenuItems)/sizeof(UImenuItem *);
}

/*!
      @brief set the display contrast

      @param value contrast value (0 to 255)
*/
void UImanager::setContrast(uint8_t value) { 
  u8g2.setContrast(value);
  contrastCtrl.setValue(value);  
}

bool UImanager::isExtraModeEnabled() {return additionalModes.getValue(); };

bool UImanager::isBtDatalogEnabled() {return btDatalog.getValue(); };
