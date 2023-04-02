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
// -
//
// Bug to fix:
// Autoscaling y axis is not always working, sometime the graph is out of scale even 20% of scale value (need more troubleshooting)
// Sometime the graph resets even if no measurement unit has changed...
//
// Latest benchmark (loop time must be kept below 300ms to avoid losing data):
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
// Release 1.0 candidate: after implementation of hold in graph mode, loop time 
// is tipically between 100-150 ms with log active with default parameters,
// Occasionally higher but never observed above 200 ms
// 
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
  Serial.print(F(" Max loop (us): "));
  Serial.println(uiman.looptimerMax);
  uiman.looptimerMax = 0;
  Serial.println(F("> "));
}

/*!
      @brief print help text with supported commands to Serial
*/
void printHelp() { // Here we want to use Serial, rather than DebugOut
  Serial.println();
  Serial.println(F(" ?    > help"));
  Serial.println(F(" swr  > SW reset"));
  Serial.println(F(" volt > show V & T"));
  Serial.println(F(" msg  > messages"));
  Serial.println(F(" log  > logging"));

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
  Serial.print(F("Invalid: "));
  Serial.println(buf);
  printHelp();
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
  if ((strcasecmp_P(buf, PSTR("swr")) == 0)) {
    Serial.println(F("SWR"));
    Serial.flush();
    _PROTECTED_WRITE(RSTCTRL.SWRR, 1);
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
    //DebugOut.print(F("PIN=")); DebugOut.print((uint8_t) eventSource);
    //DebugOut.print(F(" "));
    //k197ButtonCluster::DebugOut_printEventName(eventType);
    //DebugOut.println(F("Btn handled by UI"));
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

  //DebugOut.print(F("Btn "));
  switch (eventSource) {
  case K197key_STO:
    //DebugOut.print(F("STO"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_STO, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_STO, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  case K197key_RCL:
    //DebugOut.print(F("RCL"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_RCL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_RCL, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  case K197key_REL:
    // We cannot use UIeventPress for REL because we need to discriminate a long
    // press from a (short) click
    //DebugOut.print(F("REL"));
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
    //DebugOut.print(F("DB"));
    if (eventType == UIeventPress) {
      pinConfigure(MB_DB, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
    } else if (eventType == UIeventRelease) {
      pinConfigure(MB_DB, PIN_DIR_INPUT | PIN_OUT_LOW);
    }
    break;
  }
  //DebugOut.print(F(" "));
  //k197ButtonCluster::DebugOut_printEventName(eventType);
  //DebugOut.println();
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
  dxUtil.begin();
  pushbuttons.setup();
  pushbuttons.setCallback(myButtonCallback);

  // Handle REL and DB if they were already pushed at startup
  if (pushbuttons.isPressed(K197key_DB))
    pinConfigure(MB_DB, PIN_DIR_OUTPUT | PIN_OUT_HIGH);
  if (pushbuttons.isPressed(K197key_REL))
    pinConfigure(MB_REL, PIN_DIR_OUTPUT | PIN_OUT_HIGH);

  DebugOut.begin();

  BTman.setup();

  // We acquire one value and discard it, this may be needed before we can have
  // a stable value
  dxUtil.getVdd();
  dxUtil.getVddio2();
  dxUtil.getTKelvin();

  pinMode(LED_BUILTIN, OUTPUT);

  k197dev.setup();

  uiman.setup();
  DebugOut.useOled(true);
  DebugOut.println(F("K197Display"));

  dxUtil.printResetFlags();
  dxUtil.checkVoltages(false);
  DebugOut.print(F(", "));
  dxUtil.pollMVIOstatus();
  dxUtil.checkTemperature();

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
static unsigned long lastUpdate = 0UL;

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
      DebugOut.print(F("SPI - N="));
      DebugOut.print(n);
      DebugOut.print(F(": "));
      k197dev.debugPrintData(DMMReading, n);
      DebugOut.println();
      DebugOut.print(F("raw: "));
      DebugOut.println(k197dev.getRawMessage());
      k197dev.debugPrint();
    }
    if (n == 9) {
      lastUpdate=looptimer;
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
    PROFILE_stop(DebugOut.PROFILE_DISPLAY);
    PROFILE_println(DebugOut.PROFILE_DISPLAY, F("Time in BT checks()"));
  }
  bool collision = k197dev.collisionDetected();
  if (collision != collisionStatus) {
    collisionStatus = collision;
    DebugOut.println(F("Coll. "));
    if (collisionStatus) {
      DebugOut.println(F("DETECT"));
    } else {
      DebugOut.println(F("clear"));
    }
  }
  pushbuttons.checkNew();
  if ( (looptimer-lastUpdate) > (k197dev.isRCL() ? 375000l : 1000000l)) { // K197 is not updating data   
    uiman.updateDisplay(false); // We still want to update the display but the doodle should stay the same
    lastUpdate=looptimer;
    BTman.checkPresence();
    if (BTman.checkConnection() == BTmoduleTurnedOff)
      uiman.setLogging(false);
    if (k197dev.isRCL()) { // In RCL mode it is normal that the K197 is not sending anything
        __asm__ __volatile__("wdr" ::);
    } // ELSE if this persists it will cause a watchdog reset 
  }

  PROFILE_stop(DebugOut.PROFILE_LOOP);
  PROFILE_println(DebugOut.PROFILE_LOOP, F("Time spent in loop()"));
  looptimer = micros() - looptimer;
  if (uiman.looptimerMax < looptimer)
    uiman.looptimerMax = looptimer;
}
