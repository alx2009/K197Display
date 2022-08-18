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
#include "pinout.h"

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI
u8g2(U8G2_R0, OLED_SS,
     OLED_DC /*,reset_pin*/); // See pinout.h for pin definition. At the moment
                              // we do not use the RESET pin. It does seem to
                              // work correctly without.

UImanager::UImanager(K197device *k197) { this->k197 = k197; }

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
  /*
  for (int ix=0; ix<255; ix+=50) {
      u8g2.drawHLine(ix, 5, 40);
  }
  for (int ix=0; ix<255; ix+=20) {
      u8g2.drawHLine(ix, 3, 10);
  }
  */
  // Serial.print("w="); Serial.print(u8g2.getDisplayWidth()); Serial.print(",
  // h=");Serial.println(u8g2.getDisplayHeight()); Serial.print(F("raw msg width
  // = ")); Serial.println(u8g2.getStrWidth("8888888")); Serial.print(F("Ascent
  // = ")); Serial.println(u8g2.getAscent()); Serial.print(F("Descent = "));
  // Serial.println(u8g2.getDescent()); Serial.print(F("Max H   = "));
  // Serial.println(u8g2.getMaxCharHeight()); Serial.print(F("Max W   = "));
  // Serial.println(u8g2.getMaxCharWidth()); u8g2.drawFrame(10, 10, 230, 54);
}

void UImanager::setup() {
  pinMode(OLED_MOSI, OUTPUT); // Needed to work around a bug in the micro or
                              // dxCore with certain swap options
  boolean pmap =
      SPI.swap(OLED_SPI_SWAP_OPTION); // See pinout.h for pin definition
  if (pmap) {
    // Serial.println(F("SPI pin mapping OK!"));
    // Serial.flush();
  } else {
    Serial.println(F("SPI pin mapping failed!"));
    Serial.flush();
    while (true)
      ;
  }
  // u8g2.setBusClock(12000000); // without this call the default clock is about
  // 5 MHz, which is more than good enough.
  u8g2.begin();
  u8g2.clearBuffer();
  setup_draw();
  u8g2.sendBuffer();
}

void UImanager::updateDisplay() {
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
  u8g2.drawUTF8(xunit, yunit, k197->getUnit());

  // set the AC/DC indicator
  // u8g2.setFont(u8g2_font_10x20_mf);
  u8g2.setFont(u8g2_font_9x18_mr);
  const unsigned int xac = xraw + 3;
  const unsigned int yac = 40;
  if (k197->isAC())
    u8g2.drawStr(xac, yac, "AC");
  else
    u8g2.drawStr(xac, yac, "  ");

  // set the other announciators
  // u8g2.setFont(u8g2_font_6x12_mr);
  u8g2.setFont(u8g2_font_8x13_mr);
  unsigned int x = 0;
  unsigned int y = 5;
  const char *AUTO = "AUTO";
  if (k197->isAuto())
    u8g2.drawStr(x, y, AUTO);
  else
    u8g2.drawStr(x, y, "    ");
  x += u8g2.getStrWidth(AUTO);
  x += u8g2.getMaxCharWidth() * 2;
  u8g2.setFont(u8g2_font_6x12_mr);
  if (k197->isBAT())
    u8g2.drawStr(x, y, "BAT");
  else
    u8g2.drawStr(x, y, "   ");

  u8g2.setFont(u8g2_font_8x13_mr);
  y += u8g2.getMaxCharHeight();
  x = 0;
  const char *REL = "REL";
  if (k197->isREL())
    u8g2.drawStr(x, y, REL);
  else
    u8g2.drawStr(x, y, "   ");
  x += u8g2.getStrWidth(REL);
  x += (u8g2.getMaxCharWidth() / 2);
  if (k197->isdB())
    u8g2.drawStr(x, y, "dB");
  else
    u8g2.drawStr(x, y, "  ");

  y += u8g2.getMaxCharHeight();
  x = 0;
  if (k197->isSTO())
    u8g2.drawStr(x, y, "STO");
  else
    u8g2.drawStr(x, y, "   ");

  y += u8g2.getMaxCharHeight();
  if (k197->isRCL())
    u8g2.drawStr(x, y, "RCL");
  else
    u8g2.drawStr(x, y, "   ");

  x = 229;
  y = 0;
  if (k197->isCal())
    u8g2.drawStr(x, y, "Cal");
  else
    u8g2.drawStr(x, y, "   ");

  y += u8g2.getMaxCharHeight() * 3;
  if (k197->isRMT())
    u8g2.drawStr(x, y, "RMT");
  else
    u8g2.drawStr(x, y, "   ");

  u8g2.sendBuffer();
}
