/**************************************************************************/
/*!
  @file     BTmanager.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the BTmanager class, see BTmanager.h for the class
  definition

  This class is responsible for managing the Bluetooth module

*/
/**************************************************************************/
#include "BTmanager.h"
#include <Arduino.h>

#include "debugUtil.h"
#include "dxUtil.h"

#include "pinout.h"

BTmanager BTman;

/*!
      @brief initial setup of Bluetooth module

      @details this method should be called within setup(), and in any case
   after the constructor but before any other method. It will (attempt) to
   detect the presence of the BT module and configure it if present

      Note: Reliable detection requires the BT_POWER pin to be defined and
   connected to detect wether or not the module has power
*/
void BTmanager::setup() {
#ifdef BT_POWER
  // Check BT_POWER pin to understand if BT Power is on
  pinConfigure(SERIAL_RX, PIN_DIR_INPUT | PIN_PULLUP_OFF | PIN_INPUT_ENABLE);
  if (digitalReadFast(BT_POWER)) {
    bt_module_present = true;
  }
#else  // BT_POWER not defined
  // We don't have a pin to sense the BT Power, so we infer its status in a
  // different way
  if (dxUtil.resetReasonHWReset() ||
      (SERIAL_VPORT.IN &
       SERIAL_RX_bm)) { // Serial autoreset ==> Serial is in use
    bt_module_present = true;
  }
#endif // BT_POWER

  if (bt_module_present) {
    Serial.begin(115200);
    Serial.setTimeout(Serial_timeout);
    DebugOut.useSerial(true);
  } else {
    pinConfigure(SERIAL_TX, PIN_DIR_OUTPUT | PIN_OUT_LOW | PIN_INPUT_ENABLE);
    pinConfigure(SERIAL_RX, PIN_DIR_INPUT | PIN_PULLUP_OFF | PIN_INPUT_ENABLE);
  }
  pinConfigure(BT_EN, PIN_DIR_OUTPUT | PIN_OUT_LOW);
}

/*!
      @brief check if a bluetooth connection is present

      @details The BT_STATE pin is checked. this is connected to the "STATE" pin
   of the BT module, which is pulled low when the modulke is connected. Using
   enum BTmanagerResult as return type enables the caller to take actions that
   only need to be taken at the start/end.

      This function should be called in the main loop() after check Presence(),
   so that other functions can rely on isConnected() and validConnection();

      Note that if the autostart is used the start of a new session causes a
   reset of the AVR.

      @return the result of the check

*/
BTmanagerResult BTmanager::checkConnection() {
  CHECK_FREE_STACK();
  bool bt_module_connected_now;
  if (BT_STATE_VPORT.IN & BT_STATE_bm) { // BT_STATE == HIGH
    bt_module_connected_now = false;     // no connection
  } else if (bt_module_present) { // BT_STATE == LOW ==> connected if BT module
                                  // present
    bt_module_connected_now = true;
  } else { // cannot be connected if BT module not present
    bt_module_connected_now = false;
  }
  if (bt_module_connected == bt_module_connected_now)
    return bt_module_connected ? BTmoduleOn : BTmoduleOff;
  bt_module_connected = bt_module_connected_now;
  return bt_module_connected ? BTmoduleTurnedOn : BTmoduleTurnedOff;
}

/*!
      @brief check bluetooth module presence and reconfigure accordingly

      @details The BT_POWER pin is checked (if defined at compile time, see also
   pinout.h). This pin is connected to the BT module power with a voltage
   divider (Bt module is powered at 5V, versus 3.3V for the AVR).

      Using enum BTmanagerResult as return type enables the caller to take
   actions that only need to be taken when the module is powered on or off.

      This function should be called in the main loop(), so that other functions
   can rely on isPresent();

      This method has no effect unless BT_POWER pin is defined and connected to
   detect wether or not the Bt module has power

      @return the result of the check
*/
BTmanagerResult BTmanager::checkPresence() {
#ifdef BT_POWER
  bool bt_module_present_now = digitalReadFast(BT_POWER);
  if (bt_module_present == bt_module_present_now)
    return BTmoduleOn; // Nothing to do
  bt_module_present = bt_module_present_now;
  if (bt_module_present) { // Module was turned on after setup().
    Serial.begin( // Note: If this is the second time Serial.begin is called, a
        115200);  // bug in Serial may hang the SW, and cause a WDT reset
    Serial.setTimeout(Serial_timeout);
    DebugOut.useSerial(true);
    // DebugOut.println(F("BT turned on"));
    return BTmoduleTurnedOn;
  } else { // Module was turned off
    Serial.end();
    BT_USART.CTRLB &= (~(USART_RXEN_bm |
                         USART_TXEN_bm)); // Disable RX and TX for good measure
    pinConfigure(SERIAL_TX, PIN_DIR_OUTPUT | PIN_OUT_LOW | PIN_INPUT_ENABLE);
    pinConfigure(SERIAL_RX, PIN_DIR_INPUT | PIN_PULLUP_OFF);
    // DebugOut.println(F("BT turned off"));
    return BTmoduleTurnedOff;
  }
#else
  return bt_module_present ? BTmoduleOn : BTmoduleOff;
#endif // BT_POWER
}
