/**************************************************************************/
/*!
  @file     K197Display.ino

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This is the main file for the sketch. The following functions are implemented
in this file:
     - Management of the serial user interface
     - Callback for push button events (some handled directly, some passed to
UImanager)
     - Arduino setup: invokes the setup methods of all key objects such as
K197dev, uiman and pushbuttons
     - Arduino main loop function, loop()

  All the magic happens in the main loop function. For each loop() iteration:
  - handle commands from Serial if any has been received
  - check if 197dev has got new data (this should happen 3 times a second)
  - if new data available ask uiman to update the display (and print to Serial
if the relevant flag is set)
  - check if K197dev detected SPI collisions and if the number of data received
is the expected one, and print the information to DebugOut
  - Finally, check for pushbutton events (if there are events, the callback is
called)

    Note: the way we detect SPI client related problems in loop is not
fool-proof, but statistically we should print out something if there are
recurring issues

Currently interrupt handlers are pretty efficient (except the one for the CCL).
They could be optimized further if the need arise, but it would require
moving to inline assembler and naked interrupt handlers

*/
/**************************************************************************/
// TODO wish list:
//  test Save new options to EEPROM
//  test Save & restore current cursor position in settings
//  Save & restore current screen mode in settings
//  Keep hold when switching between display modes ?
// Bug2fix: 
//
// Latest benchmark:
// loop() ==> 195 ms (normal), 120 ms (minmax), 145 ms (menu), 140 ms (menu+
// default logging)
//            195 (normal + max logging), 142 (menu+max logging), 121
//            (minmax+max logging) 164 ms (normal hold + max logging), 76 ms
//            (mimmax hold +maxlogging)
// getNewReading() ==> 135 us
// updateDisplay() ==> 120 ms (normal, minmax), 150ms (menu)
// logData() ==> 7us (no logging), 362 us (default log active), 4ms (all
// options) BT checks ==> 75us (normal, connected), 10 us (options menu)
// Increasing the OLED SPI clock shaves 13 ms to the loop time (195 ms to 182
// ms)
//
#include "K197device.h"

#include "UImanager.h"

#include "K197PushButtons.h"
#include "debugUtil.h"
#include "dxUtil.h"

#include "BTmanager.h"

#include "pinout.h"
const char CH_SPACE = ' '; ///< using a global constant saves some RAM

#ifndef DB_28_PINS
#error AVR32DB28 or AVR64DB28 or AVR128DB28 microcontroller required, using dxCore
#endif

bool msg_printout = false; ///< if true prints raw messages to DebugOut

////////////////////////////////////////////////////////////////////////////////////
// Management of the serial user interface
////////////////////////////////////////////////////////////////////////////////////

/*!
      @brief print the prompt to Serial
*/
void printPrompt() { // Here we want to use Serial, rather than DebugOut
  Serial.println();
  dxUtil.reportStack();
  Serial.print(F(" Max loop time (us): "));
  Serial.println(uiman.looptimerMax);
  uiman.looptimerMax = 0;
  Serial.println(F("> "));
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

void cmdTmpScaling() {
  static char buf[INPUT_BUFFER_SIZE];
  size_t i = Serial.readBytesUntil(CH_SPACE, buf, INPUT_BUFFER_SIZE);
  buf[i] = 0;
  if (i == 0) { // no characters read
    Serial.println(F("Contrast: <no change>"));
    Serial.flush();
    return;
  }
  float value = atof(buf);
  Serial.print(F("X="));
  Serial.println(value);
  Serial.flush();
  //PROFILE_start(DebugOut.PROFILE_MATH);
  k197dev.troubleshootAutoscale(value, value);
  //PROFILE_stop(DebugOut.PROFILE_MATH);
  //PROFILE_println(DebugOut.PROFILE_MATH,
  //                  F("Time spent in troubleshootAutoscale()"));
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
  }  else if ((strcasecmp_P(buf, PSTR("x")) == 0)) {
    cmdTmpScaling();
  } else if ((strcasecmp_P(buf, PSTR(" ")) == 0)) {
    // do nothing;
  } else {
    printError(buf);
  }
}

////////////////////////////////////////////////////////////////////////////////////
// Callback for push button events
////////////////////////////////////////////////////////////////////////////////////

/*!
      @brief Callback for push button events

      @details this function should be set as call back for all push buttons

      @param buttonPinIn pin identifying the UI button
      @param eventType one of the eventXXX constants define in class
   k197ButtonCluster
*/
void myButtonCallback(K197UIeventsource eventSource,
                      K197UIeventType eventType) {
  dxUtil.checkFreeStack();
  if (uiman.handleUIEvent(eventSource,
                          eventType)) { // UI related event, no need to do more
    // DebugOut.print(F("PIN=")); DebugOut.print((uint8_t) eventSource);
    // DebugOut.print(F(" "));
    // k197ButtonCluster::DebugOut_printEventName(eventType);
    // DebugOut.println(F("Btn handled by UI"));
    return;
  }
  if (k197dev.isNotCal() && uiman.isSplitScreen())
    return; // Nothing to do in split screen mode

  // Handle the special case of REL and DB pressed simultaneously (enter cal
  // mode)
  if ((eventSource == K197key_REL || eventSource == K197key_DB) &&
      pushbuttons.isSimultaneousPress(K197key_REL, K197key_DB)) {
    // DebugOut.print(F("REL+DB"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_REL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
      pinConfigure(MB_DB, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_REL, PIN_DIR_INPUT | PIN_OUT_LOW);
      pinConfigure(MB_DB, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    return;
  }

  // DebugOut.print(F("Btn "));
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
    // We cannot use UIeventPress for REL because we need to discriminate a long
    // press from a (short) click
    // DebugOut.print(F("REL"));
    if (k197dev.isNotCal() && eventType == UIeventClick) {
      pushbuttons.clickREL();
    }
    if (k197dev.isCal() && (eventType == UIeventPress)) {
      pinConfigure(MB_REL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (k197dev.isCal() && (eventType == UIeventRelease)) {
      pinConfigure(MB_REL, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  case K197key_DB:
    // DebugOut.print(F("DB"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_DB, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_DB, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  }
  // DebugOut.print(F(" "));
  // k197ButtonCluster::DebugOut_printEventName(eventType);
  // DebugOut.println();
}

////////////////////////////////////////////////////////////////////////////////////
// Arduino setup() & loop()
////////////////////////////////////////////////////////////////////////////////////

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
  pushbuttons.setCallback(myButtonCallback);

  // Handle REL and DB if they were already pushed at startup
  if (pushbuttons.isPressed(K197key_DB))
    pinConfigure(MB_DB, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
  if (pushbuttons.isPressed(K197key_REL))
    pinConfigure(MB_REL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);

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
    false; ///< keep track if a collision was detected by the SPI peripheral

static unsigned long looptimer = 0UL;

/*!
      @brief Arduino loop function
*/
void loop() {
  looptimer = micros();
  PROFILE_start(DebugOut.PROFILE_LOOP);
  if (Serial.available()) {
    handleSerial();
  }

  if (k197dev.hasNewData()) {
    PROFILE_start(DebugOut.PROFILE_DEVICE);
    byte n = k197dev.getNewReading(DMMReading);
    PROFILE_stop(DebugOut.PROFILE_DEVICE);
    PROFILE_println(DebugOut.PROFILE_DEVICE,
                    F("Time spent in getNewReading()"));
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
      PROFILE_start(DebugOut.PROFILE_DISPLAY);
      uiman.updateDisplay();
      PROFILE_stop(DebugOut.PROFILE_DISPLAY);
      PROFILE_println(DebugOut.PROFILE_DISPLAY, F("Time in updateDisplay()"));
      PROFILE_start(DebugOut.PROFILE_DISPLAY);
      uiman.logData();
      PROFILE_stop(DebugOut.PROFILE_DISPLAY);
      PROFILE_println(DebugOut.PROFILE_DISPLAY, F("Time in logData()"));
      dxUtil.checkFreeStack();
      __asm__ __volatile__("wdr" ::);
    }
    PROFILE_start(DebugOut.PROFILE_DISPLAY);
    BTman.checkPresence();
    if (BTman.checkConnection() == BTmoduleTurnedOff)
      uiman.setLogging(false);
    uiman.updateBtStatus();
    PROFILE_stop(DebugOut.PROFILE_DISPLAY);
    PROFILE_println(DebugOut.PROFILE_DISPLAY, F("Time in BT checks()"));
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
  pushbuttons.checkNew();

  PROFILE_stop(DebugOut.PROFILE_LOOP);
  PROFILE_println(DebugOut.PROFILE_LOOP, F("Time spent in loop()"));
  looptimer = micros() - looptimer;
  if (uiman.looptimerMax < looptimer)
    uiman.looptimerMax = looptimer;
}
