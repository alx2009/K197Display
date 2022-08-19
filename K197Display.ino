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
     - Management of the serial user interface (for troubleshooting)
     - Callbacks for push button events (only printouts for troubleshooting
purpose)
     - Instantiation and setup  of the key objects K197dev, uiman and
pushbuttons is also done here

  All the magic happens in the main loop. For each loop iteration:
  - handle commands from Serial if any has been received
  - check if 197dev has got new data (this should happen 3 times a second)
  - if new data available ask uiman to update the display (and print to
Serial if the relevant flag is set)
  - if new data is available, toggle the built in led
  - check if K197dev detected SPI collisions and if the number of data received
is the expected one, and print the information to Serial
  - Finally, check for pushbutton events (this will not be detected directly in
loop, but it will trigger the callback)

Note: the way we detect SPI client related problems in loop is not fool-proof,
but statistically we should print out something if there are recurring issues

*/
/**************************************************************************/

#include "K197device.h"
K197device k197dev;

#include "UImanager.h"
UImanager uiman(&k197dev);

#include "dxUtil.h"
// dxUtil util;

#include "K197PushButtons.h"
#include "pinout.h"

#ifndef DB_28_PINS
#error AVR32DB28 or AVR64DB28 or AVR128DB28 microcontroller required, using dxCore
#endif

bool msg_printout = false;

void printPrompt() {
  Serial.println();
  Serial.print(F("> "));
}

void printHelp() {
  Serial.println();
  Serial.println(F(" ? - print this help text"));
  Serial.println(F(" wdt  ==> trigger watchdog reset"));
  Serial.println(F(" swr  ==> triger software reset"));
  Serial.println(F(" jmp0 ==> jump to 0 (dirty reset)"));
  Serial.println(F(" volt ==> check voltages & temperature"));
  Serial.println(F(" msg ==> toggle printoput of data to/from main board"));

  printPrompt();
}

#define INPUT_BUFFER_SIZE 30
void printError(char *buf) {
  Serial.print(F("Invalid command: "));
  Serial.println(buf);
  printHelp();
}

void handleSerial() {
  static char buf[INPUT_BUFFER_SIZE];
  size_t i = Serial.readBytesUntil(' ', buf, INPUT_BUFFER_SIZE);
  buf[i] = 0;
  if (i == 0) { // no characters read
    return;
  }
  // Check help
  if (strcasecmp("?", buf) == 0) {
    printHelp();
    return;
  }

  if (strcasecmp("wdt", buf) == 0) {
    Serial.println(F("Testing watchdog reset"));
    Serial.flush();
    _PROTECTED_WRITE(WDT.CTRLA,
                     WDT_WINDOW_8CLK_gc |
                         WDT_PERIOD_8CLK_gc); // enable the WDT, minimum
                                              // timeout, minimum window.
    while (1)
      __asm__ __volatile__("wdr" ::);
  } else if ((strcasecmp("swr", buf) == 0)) {
    Serial.println(F("Testing SW reset"));
    Serial.flush();
    _PROTECTED_WRITE(RSTCTRL.SWRR, 1);
  } else if ((strcasecmp("jmp0", buf) == 0)) {
    Serial.println(F("Testing dirty reset"));
    Serial.flush();
    asm volatile("jmp 0");
  } else if ((strcasecmp("volt", buf) == 0)) {
    // Serial.println(F("Check voltages")); Serial.flush();
    dxUtil.checkVoltages();
    dxUtil.checkTemperature();
  } else if ((strcasecmp("msg", buf) == 0)) {
    // Serial.println(F("MSG printout toggle")); Serial.flush();
    if (msg_printout)
      msg_printout = false;
    else
      msg_printout = true;
  } else if ((strcasecmp(" ", buf) == 0)) {
    // do nothing;
  } else {
    printError(buf);
  }
}

k197ButtonCluster pushbuttons;

void myButtonCallback(uint8_t buttonPinIn, uint8_t buttonEvent) {
  Serial.print("Btn ");
  switch (buttonPinIn) {
  case UI_STO:
    Serial.print(F("STO"));
    break;
  case UI_RCL:
    Serial.print(F("RCL"));
    break;
  case UI_REL:
    Serial.print(F("REL"));
    break;
  case UI_DB:
    Serial.print(F("DB"));
    break;
  }
  Serial.print(", PIN=");
  Serial.print(buttonPinIn);
  Serial.print(", ");
  k197ButtonCluster::Serial_printEventName(buttonEvent);
  Serial.println();
}

// the setup function runs once when you press reset or power the board
void setup() {
  // Enable slew rate limiting on all ports
  // Note: simple assignment ok on DA, DB, and DD-series parts, no other bits of
  // PORTCTRL are used
  PORTA.PORTCTRL = PORT_SRL_bm;
  PORTC.PORTCTRL = PORT_SRL_bm;
  PORTD.PORTCTRL = PORT_SRL_bm;
  PORTF.PORTCTRL = PORT_SRL_bm;
  pushbuttons.setup();
  pushbuttons.setCallback(UI_STO, myButtonCallback);
  pushbuttons.setCallback(UI_RCL, myButtonCallback);
  pushbuttons.setCallback(UI_REL, myButtonCallback);
  pushbuttons.setCallback(UI_DB, myButtonCallback);
  Serial.begin(115200);
  dxUtil.begin();
  dxUtil.getVdd();
  dxUtil.getVddio2();
  dxUtil.getTKelvin();

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println(F("K197Display running on dxCore"));
  k197dev.setup();

  uiman.setup();

  dxUtil.printResetFlags();
  dxUtil.checkVoltages();
  dxUtil.pollMVIOstatus();
  dxUtil.checkTemperature();
}

byte DMMReading[PACKET];

bool collisionStatus = false;

void loop() {
  if (Serial.available()) {
    handleSerial();
  }

  if (k197dev.hasNewData()) {
    byte n = k197dev.getNewReading(DMMReading);
    if (msg_printout) {
      Serial.print(F("SPI packet - N="));
      Serial.print(n);
      Serial.print(F(": "));
      k197dev.debugPrintData(DMMReading, n);
      Serial.println();
      Serial.print("raw: ");
      Serial.println(k197dev.getRawMessage());
      k197dev.debugPrint();
    }
    digitalWriteFast(LED_BUILTIN, CHANGE); // digitalWriteFast is part of dxCore
                                           // built-in "fast I/O" library
    if (n == 9)
      uiman.updateDisplay();
  }
  bool collision = k197dev.collisionDetected();
  if (collision != collisionStatus) {
    collisionStatus = collision;
    Serial.println(F("Collision "));
    if (collisionStatus) {
      Serial.println(F("DETECTED"));
    } else {
      Serial.println(F("cleared"));
    }
  }
  pushbuttons.check();
}
