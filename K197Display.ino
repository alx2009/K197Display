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

#include "K197PushButtons.h"
#include "debugUtil.h"
#include "dxUtil.h"

#include "pinout.h"

#ifndef DB_28_PINS
#error AVR32DB28 or AVR64DB28 or AVR128DB28 microcontroller required, using dxCore
#endif

bool msg_printout = false;      ///< if true prints raw messages to DebugOut
bool bt_module_present = false; ///< true if BT module is powered on
bool bt_module_connected =
    false; ///< true if connection detected via BT_STATE pin

/*!
      @brief print the prompt to Serial
*/
void printPrompt() { // Here we want to use Serial, rather than DebugOut
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
  Serial.println(F(" msg ==> toggle printoput of data to/from main board"));
  Serial.println(F(" contrast n ==> set display contrast [0-255]"));

  printPrompt();
}

#define INPUT_BUFFER_SIZE                                                      \
  30 ///< size of the input buffer when reading from Serial

/*!
      @brief error message for invalid command to Serial

      @param buf null terminated char array with the invalid command
*/
void printError(char *buf) { // Here we want to use Serial, rather than DebugOut
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
  static char buf[INPUT_BUFFER_SIZE];
  size_t i = Serial.readBytesUntil(' ', buf, INPUT_BUFFER_SIZE);
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
      @brief handle all Serial commands

      Reads from Serial and execute any command. Should be invoked when there is
   input availble from Serial. Note: It will block for the default Serial
   timeout if a command is not complete, affecting the display. Terminals
   sending complete lines are therefore preferred (e.g. the Serial Monitor in
   the Arduibno GUI)
*/
void handleSerial() { // Here we want to use Serial, rather than DebugOut
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
    dxUtil.checkVoltages(false);
    DebugOut.print(F(", "));
    dxUtil.checkTemperature();
  } else if ((strcasecmp("msg", buf) == 0)) {
    // Serial.println(F("MSG printout toggle")); Serial.flush();
    if (msg_printout)
      msg_printout = false;
    else
      msg_printout = true;
  } else if ((strcasecmp("contrast", buf) == 0)) {
    cmdContrast();
  } else if ((strcasecmp(" ", buf) == 0)) {
    // do nothing;
  } else {
    printError(buf);
  }
}

k197ButtonCluster pushbuttons; ///< this object is used to interact with the
                               ///< push-button cluster

/*!
      @brief Callback for push button events

      @param buttonPinIn pin identifying the UI button
      @param buttonEvent one of the eventXXX constants define in class
   k197ButtonCluster
*/
void myButtonCallback(uint8_t buttonPinIn, uint8_t buttonEvent) {
  // Serial.print("Btn ");
  switch (buttonPinIn) {
  case UI_STO:
    // DebugOut.print(F("STO"));
    break;
  case UI_RCL:
    // DebugOut.print(F("RCL"));
    break;
  case UI_REL:
    // DebugOut.print(F("REL"));
    if (buttonEvent == k197ButtonCluster::eventLongPress) {
      if (uiman.getScreenMode() == UImanager::displayNormal) {
        uiman.setScreenMode(UImanager::displayDebug);
      } else {
        uiman.setScreenMode(UImanager::displayNormal);
      }
    }
    break;
  case UI_DB:
    // DebugOut.print(F("DB"));
    break;
  }
  // DebugOut.print(", PIN="); DebugOut.print(buttonPinIn); DebugOut.print(",
  // "); k197ButtonCluster::DebugOut_printEventName(buttonEvent);
  // DebugOut.println();
}

/*!
      @brief check bluetoot module presence

      Note: works but due to the delay to enter command mode interferes with
   serial debugging Not recommended to be used!
*/
void checkBluetoothModulePresence() {
  if (!bt_module_present)
    return; // when the module is turned on the autoreset circuit takes care of
            // detection
  bool serlogbkp = DebugOut.useSerial();
  DebugOut.useSerial(false);
  Serial.flush(); // first we make sure to empty the queue   // TODO: use
                  // Serial.availableForWrite() to avoid blocking
  if (Serial.available())
    return; // Then we need to be sure we have no data in the receive queue

  // Ok, there is no activity currently on the serial port. We will enter
  // command mode and see if the module is still there...
  digitalWriteFast(BT_EN, HIGH);
  delay(55);
  Serial.print(F("AT\n\r"));
  Serial.flush();
  delay(40);
  int nchar = 0;
  while (Serial.available()) {
    nchar++;
    Serial.read();
  }
  digitalWriteFast(BT_EN, LOW);
  if (nchar > 0) {
    DebugOut.useSerial(serlogbkp);
  } else { // BT Module was turned off
    // DebugOut.useSerial(serlogbkp);
    Serial.end();
    BT_USART.CTRLB &= (~(USART_RXEN_bm |
                         USART_TXEN_bm)); // Disable RX and TX for good measure
    pinConfigure(SERIAL_TX, PIN_DIR_OUTPUT | PIN_OUT_LOW | PIN_INPUT_ENABLE);
    pinConfigure(SERIAL_RX, PIN_DIR_INPUT | PIN_PULLUP_OFF);
    bt_module_present = false;
    DebugOut.println(F("BT turned off"));
  }
  return;
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
  pushbuttons.setCallback(UI_STO, myButtonCallback);
  pushbuttons.setCallback(UI_RCL, myButtonCallback);
  pushbuttons.setCallback(UI_REL, myButtonCallback);
  pushbuttons.setCallback(UI_DB, myButtonCallback);
  DebugOut.begin();

  dxUtil.begin();

  if (dxUtil.resetReasonHWReset() ||
      (SERIAL_VPORT.IN &
       SERIAL_RX_bm)) { // Serial autoreset ==> Serial is in use
    bt_module_present = true;
    Serial.begin(115200);
    DebugOut.useSerial(true);
  } else {
    pinConfigure(SERIAL_TX, PIN_DIR_OUTPUT | PIN_OUT_LOW | PIN_INPUT_ENABLE);
    pinConfigure(SERIAL_RX, PIN_DIR_INPUT | PIN_PULLUP_OFF | PIN_INPUT_ENABLE);
  }
  pinConfigure(BT_EN, PIN_DIR_OUTPUT | PIN_OUT_LOW);

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

  if (bt_module_present) {
    DebugOut.println(F("BT is on"));
  } else {
    DebugOut.println(F("BT is off"));
  }
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
      DebugOut.print("raw: ");
      DebugOut.println(k197dev.getRawMessage());
      k197dev.debugPrint();
    }
    if (n == 9)
      uiman.updateDisplay();
    // checkBluetoothModulePresence();
    if (BT_STATE_VPORT.IN & BT_STATE_bm) { // BT_STATE == HIGH
      bt_module_connected = false;         // no connection
    } else {                               // BT_STATE == LOW ==> connected
      bt_module_connected = true;
    }
    uiman.updateBtStatus(bt_module_present, bt_module_connected);
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
