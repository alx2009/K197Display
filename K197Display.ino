/**************************************************************************/
/*!
  @file     K197Display.ino

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This is the main file for the sketch. In addition to setup() and loop(), the
following functions are implemented in this file:
     - Management of the serial user interface
     - Callback for push button events (some handled directly, some passed to
UImanager)
     - Setup invokes the setup methods of all key objects such as K197dev, uiman
and pushbuttons

  All the magic happens in the main loop. For each loop iteration:
  - handle commands from Serial if any has been received
  - check if 197dev has got new data (this should happen 3 times a second)
  - if new data available ask uiman to update the display (and print to Serial
if the relevant flag is set)
  - check if K197dev detected SPI collisions and if the number of data received
is the expected one, and print the information to DebugOut
  - Finally, check for pushbutton events (this will trigger the callback)

    At startup button presses from the UI push buttons are mirrored towards the
motherboard (transparent mode). At the end of the arduino setup() transparent
mode is turned off, so that we can assign new functions to the buttons

    Note: the way we detect SPI client related problems in loop is not
fool-proof, but statistically we should print out something if there are
recurring issues

*/
/**************************************************************************/
// TODO wish list:
//  streamline event handling
//  improve responsiveness of double click
//  Autohold
//  Save/retrieve setting to EEPROM
//  Graph mode
// Bug: Enable scrolling menu backward even if the item is not selectable

#include "K197device.h"

#include "UImanager.h"

#include "K197PushButtons.h"
#include "debugUtil.h"
#include "dxUtil.h"

#include "BTmanager.h"

#include "pinout.h"
const char CH_SPACE =
    ' '; ///< using a constant (defined in pinout.h) saves some RAM

#ifndef DB_28_PINS
#error AVR32DB28 or AVR64DB28 or AVR128DB28 microcontroller required, using dxCore
#endif

bool msg_printout = false; ///< if true prints raw messages to DebugOut

/*!
      @brief print the prompt to Serial
*/
void printPrompt() { // Here we want to use Serial, rather than DebugOut
  dxUtil.reportStack();
  Serial.println();
  Serial.print(F("> "));
}

/*!
      @brief print help text with supported commands to Serial
*/
void printHelp() { // Here we want to use Serial, rather than DebugOut
  Serial.println();
  Serial.println(F(" ? - print this help text"));
  Serial.println(F(" wdt  ==> trigger watchdog reset"));
  Serial.println(F(" swr  ==> triger software reset"));
  Serial.println(F(" jmp0 ==> jump to 0 (dirty reset)"));
  Serial.println(F(" volt ==> check voltages & temperature"));
  Serial.println(F(" msg ==> toggle printout of data to/from main board"));
  Serial.println(F(" log ==> toggle data logging"));
  Serial.println(F(" contrast n ==> set display contrast [0-255]"));

  printPrompt();
}

#define INPUT_BUFFER_SIZE                                                      \
  30 ///< size of the input buffer when reading from Serial

/*!
      @brief error message for invalid command to Serial
      @param buf null terminated char array with the invalid command
*/
void printError(
    const char *buf) { // Here we want to use Serial, rather than DebugOut
  Serial.print(F("Invalid command: "));
  Serial.println(buf);
  printHelp();
}

/*!
      @brief handle the "contrast" command
      Reads the contrast value from serial and change contrast accordingly
*/
void cmdContrast() { // Here we want to use Serial, rather than DebugOut (which
                     // does not even support input)
  dxUtil.checkFreeStack();
  static char buf[INPUT_BUFFER_SIZE];
  size_t i = Serial.readBytesUntil(CH_SPACE, buf, INPUT_BUFFER_SIZE);
  buf[i] = 0;
  if (i == 0) { // no characters read
    Serial.println(F("Contrast: <no change>"));
    Serial.flush();
    return;
  }
  uint8_t value = atoi(buf);
  Serial.print(F("Contrast="));
  Serial.println(value);
  Serial.flush();
  uiman.setContrast(value);
}

/*!
      @brief toggle data logging
*/
void cmdLog() { uiman.setLogging(!uiman.isLogging()); }

/*!
      @brief handle all Serial commands
      @details Reads from Serial and execute any command. Should be invoked when
   there is input availble from Serial. Note: It will block for the default
   Serial timeout if a command is not complete, affecting the display. Terminals
   sending complete lines are therefore preferred (e.g. the Serial Monitor in
   the Arduibno GUI)
*/
void handleSerial() { // Here we want to use Serial, rather than DebugOut
  dxUtil.checkFreeStack();
  static char buf[INPUT_BUFFER_SIZE];
  size_t i = Serial.readBytesUntil(CH_SPACE, buf, INPUT_BUFFER_SIZE);
  buf[i] = 0;
  if (i == 0) { // no characters read
    return;
  }
  // Check help
  if (strcasecmp_P(buf, PSTR("?")) == 0) {
    printHelp();
    return;
  }

  if (strcasecmp_P(buf, PSTR("wdt")) == 0) {
    Serial.println(F("Testing watchdog reset"));
    Serial.flush();
    _PROTECTED_WRITE(WDT.CTRLA,
                     WDT_WINDOW_8CLK_gc |
                         WDT_PERIOD_8CLK_gc); // enable the WDT, minimum
                                              // timeout, minimum window.
    while (1)
      __asm__ __volatile__("wdr" ::);
  } else if ((strcasecmp_P(buf, PSTR("swr")) == 0)) {
    Serial.println(F("Testing SW reset"));
    Serial.flush();
    _PROTECTED_WRITE(RSTCTRL.SWRR, 1);
  } else if ((strcasecmp_P(buf, PSTR("jmp0")) == 0)) {
    Serial.println(F("Testing dirty reset"));
    Serial.flush();
    asm volatile("jmp 0");
  } else if ((strcasecmp_P(buf, PSTR("volt")) == 0)) {
    // Serial.println(F("Check voltages")); Serial.flush();
    dxUtil.checkVoltages(false);
    DebugOut.print(F(", "));
    dxUtil.checkTemperature();
  } else if ((strcasecmp_P(buf, PSTR("msg")) == 0)) {
    // Serial.println(F("MSG printout toggle")); Serial.flush();
    if (msg_printout)
      msg_printout = false;
    else
      msg_printout = true;
  } else if ((strcasecmp_P(buf, PSTR("log")) == 0)) {
    cmdLog();
  } else if ((strcasecmp_P(buf, PSTR("contrast")) == 0)) {
    cmdContrast();
  } else if ((strcasecmp_P(buf, PSTR(" ")) == 0)) {
    // do nothing;
  } else {
    printError(buf);
  }
}

k197ButtonCluster pushbuttons; ///< this object is used to interact with the
                               ///< push-button cluster

#define K197_MB_CLICK_TIME                                                     \
  75 ///< how much a click should last when sent to the K197

/*!
      @brief Callback for push button events

      @details this function should be set as call back for all push buttons

      @param buttonPinIn pin identifying the UI button
      @param eventType one of the eventXXX constants define in class
   k197ButtonCluster
*/
void myButtonCallback(K197UIeventsource eventSource, K197UIeventType eventType) {
  dxUtil.checkFreeStack();
  if (uiman.handleUIEvent(eventSource, eventType)) { // UI related event, no need to do more
    // DebugOut.print(F(", PIN=")); DebugOut.print((uint8_t) eventSource);
    // DebugOut.print(F(" "));
    // k197ButtonCluster::DebugOut_printEventName(eventType);
    // DebugOut.println(F("Btn handled by UI"));
    return;
  }
  if (uiman.isSplitScreen()) return; // Nothing to do in split screen mode
  
  // DebugOut.print(F("Btn "));
  if(pushbuttons.isTransparentMode()) return; // No need to do anything here
  switch (eventSource) {
  case K197key_STO:
    // DebugOut.print(F("STO"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_STO, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_STO, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  case K197key_RCL:
    // DebugOut.print(F("RCL"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_RCL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_RCL, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  case K197key_REL:
    // DebugOut.print(F("REL"));
    if (eventType == UIeventClick) {
      pinConfigure(MB_REL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
      delay(K197_MB_CLICK_TIME); // TODO: implement differently in order to
                                 // remove delay()
      pinConfigure(MB_REL, PIN_DIR_INPUT | PIN_OUT_LOW);
    } else if (eventType == UIeventDoubleClick) k197dev.resetStatistics();
    break;
  case K197key_DB:
    // DebugOut.print(F("DB"));
    if (eventType == UIeventPress) {
      if (uiman.isExtraModeEnabled() && k197dev.isV() && k197dev.ismV() &&
          k197dev.isDC()) {
        if (!k197dev.getTKMode()) { // TK mode is not yet enabled
          k197dev.setTKMode(true);  // Activate TK mode
          break;                    // break so we do not activate the Db mode
        } else {
          k197dev.setTKMode(false); // No break so we also activate dB mode
        }
      }
      pinConfigure(MB_DB, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_DB, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  }
  // DebugOut.print(F(", PIN=")); DebugOut.print(buttonPinIn);
  // DebugOut.print(F(" "));
  // k197ButtonCluster::DebugOut_printEventName(eventType);
  // DebugOut.println();

}

/*!
      @brief Arduino setup function
*/
void setup() {
  // Enable slew rate limiting on all ports
  // Note: simple assignment ok on DA, DB, and DD-series parts, no other bits of
  // PORTCTRL are used
  PORTA.PORTCTRL = PORT_SRL_bm;
  PORTC.PORTCTRL = PORT_SRL_bm;
  PORTD.PORTCTRL = PORT_SRL_bm;
  PORTF.PORTCTRL = PORT_SRL_bm;
  pushbuttons.setup();
  pushbuttons.setCallback(K197key_STO, myButtonCallback);
  pushbuttons.setCallback(K197key_RCL, myButtonCallback);
  pushbuttons.setCallback(K197key_REL, myButtonCallback);
  pushbuttons.setCallback(K197key_DB, myButtonCallback);
  DebugOut.begin();

  dxUtil.begin();
  BTman.setup();

  // We acquire one value and discard it, this may be needed before we can have
  // a stable value
  dxUtil.getVdd();
  dxUtil.getVddio2();
  dxUtil.getTKelvin();

  pinMode(LED_BUILTIN, OUTPUT);

  DebugOut.println(F("K197Display running on dxCore"));

  k197dev.setup();

  uiman.setup();
  u8g2log.println(F("K197Display"));
  DebugOut.useOled(true);

  dxUtil.printResetFlags();
  dxUtil.checkVoltages(false);
  DebugOut.print(F(", "));
  dxUtil.pollMVIOstatus();
  dxUtil.checkTemperature();

  if (BTman.isPresent()) {
    DebugOut.println(F("BT is on"));
  } else {
    DebugOut.println(F("BT is off"));
  }

  pushbuttons.setTransparentMode(false);
  delay(100);

  dxUtil.checkFreeStack();

  // Setup watchdog
  _PROTECTED_WRITE(
      WDT.CTRLA,
      WDT_WINDOW_8CLK_gc |
          WDT_PERIOD_8KCLK_gc); // enable the WDT, 8s timeout, minimum window.
}
byte DMMReading[PACKET]; ///< buffer used to store the raw data received from
                         ///< the voltmeter main board

bool collisionStatus =
    false; ///< keep track if a collision was detcted by the SPI peripheral

/*!
      @brief Arduino loop function
*/
void loop() {
  if (Serial.available()) {
    handleSerial();
  }

  if (k197dev.hasNewData()) {
    byte n = k197dev.getNewReading(DMMReading);
    if (msg_printout) {
      DebugOut.print(F("SPI packet - N="));
      DebugOut.print(n);
      DebugOut.print(F(": "));
      k197dev.debugPrintData(DMMReading, n);
      DebugOut.println();
      DebugOut.print(F("raw: "));
      DebugOut.println(k197dev.getRawMessage());
      k197dev.debugPrint();
    }
    if (n == 9) {
      uiman.updateDisplay();
      uiman.logData();
      dxUtil.checkFreeStack();
      __asm__ __volatile__("wdr" ::);
    }
    BTman.checkPresence();
    if (BTman.checkConnection() == BTmoduleTurnedOff)
      uiman.setLogging(false);
    uiman.updateBtStatus();
  }
  bool collision = k197dev.collisionDetected();
  if (collision != collisionStatus) {
    collisionStatus = collision;
    DebugOut.println(F("Collision "));
    if (collisionStatus) {
      DebugOut.println(F("DETECTED"));
    } else {
      DebugOut.println(F("cleared"));
    }
  }
  pushbuttons.check();
}
