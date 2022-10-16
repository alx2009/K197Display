/**************************************************************************/
/*!
  @file     K197PushButtons.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the k197ButtonCluster class, see k197ButtonCluster.h for
  the class definition

  Currently button presses from the UI bush buttons are always mirrored towards
  the motherboard. However the call back is also called so that
  additional/complementary action can be taken by this sketch

*/
/**************************************************************************/

#include "K197PushButtons.h"
#include <Arduino.h>

#include "debugUtil.h"

#include "pinout.h"

// In the following we instantiate a bunch of arrays to keep track of the status
// of each push button Maybe a bit confusing as it is a copy/paste from previous
// projects But essentially we need timers to keep track of various time
// intervals (debouncing, pressed, released, log press, etc.)

uint8_t buttonPinIn[] = {UI_STO, UI_RCL, UI_REL,
                         UI_DB}; ///< index to pin mapping for UI push buttons
uint8_t buttonPinOut[] = {MB_STO, MB_RCL, MB_REL,
                          MB_DB}; ///< index to pin mapping for output
k197ButtonCluster::buttonCallBack callBack[] = {
    NULL, NULL, NULL, NULL}; ///< Stores the call back for each button
uint8_t buttonState[] = {
    BUTTON_IDLE_STATE, BUTTON_IDLE_STATE, BUTTON_IDLE_STATE,
    BUTTON_IDLE_STATE}; ///< the current reading from the button pin
uint8_t lastButtonState[] = {
    BUTTON_IDLE_STATE, BUTTON_IDLE_STATE, BUTTON_IDLE_STATE,
    BUTTON_IDLE_STATE}; ///< the previous reading from the button pin
unsigned long lastDebounceTime[] = {
    0UL, 0UL, 0UL, 0UL}; ///< millis() the last time the button pin was toggled
unsigned long startPressed[] = {0UL, 0UL, 0UL,
                                0UL}; ///< millis() when last pressed
unsigned long lastReleased[] = {0UL, 0UL, 0UL,
                                0UL}; ///< millis() when last released

// The following four functions are the interrupt handlers for each push button.
// They are normal functions because we are using attachInterrupt (not the most
// efficient way with dxCore, but it is the default and it works for now) The
// interrupt handler will simulate the push of the button on the 197/197A main
// board this is completely independent from the handling of the butto events in
// the rest of the sketch

/*!
      @brief interrupt handler for STO push button

      The interrupt handler is used when the push button event is sent
   transparently to the meter
*/
void UI_STO_changing() {
  if (UI_STO_VPORT.IN & UI_STO_bm) { // Button is not pressed
    MB_STO_VPORT.DIR &=
        (~MB_STO_bm); // Clear DIR bit (set direction to input ==> floating)
    MB_STO_VPORT.OUT &= (~MB_STO_bm); // Set output value to 0
  } else {                            // device selected
    MB_STO_VPORT.OUT |= MB_STO_bm;    // Set output value to 1
    MB_STO_VPORT.DIR |= MB_STO_bm;    // Set DIR bit (set direction to output)
  }
}

/*!
      @brief interrupt handler for RCL push button

      The interrupt handler is used when the push button event is sent
   transparently to the meter
*/
void UI_RCL_changing() {
  if (UI_RCL_VPORT.IN & UI_RCL_bm) { // Button is not pressed
    MB_RCL_VPORT.DIR &=
        (~MB_RCL_bm); // Clear DIR bit (set direction to input ==> floating)
    MB_RCL_VPORT.OUT &= (~MB_RCL_bm); // Set output value to 0
  } else {                            // device selected
    MB_RCL_VPORT.OUT |= MB_RCL_bm;    // Set output value to 1
    MB_RCL_VPORT.DIR |= MB_RCL_bm;    // Set DIR bit (set direction to output)
  }
}

/*!
      @brief interrupt handler for REL push button

      The interrupt handler is used when the push button event is sent
   transparently to the meter
*/
void UI_REL_changing() {
  if (UI_REL_VPORT.IN & UI_REL_bm) { // Button is not pressed
    MB_REL_VPORT.DIR &=
        (~MB_REL_bm); // Clear DIR bit (set direction to input ==> floating)
    MB_REL_VPORT.OUT &= (~MB_REL_bm); // Set output value to 0
  } else {                            // device selected
    MB_REL_VPORT.OUT |= MB_REL_bm;    // Set output value to 1
    MB_REL_VPORT.DIR |= MB_REL_bm;    // Set DIR bit (set direction to output)
  }
}

/*!
      @brief interrupt handler for DB push button

      The interrupt handler is used when the push button event is sent
   transparently to the meter
*/
void UI_DB_changing() {
  if (UI_DB_VPORT.IN & UI_DB_bm) { // Button is not pressed
    MB_DB_VPORT.DIR &=
        (~MB_DB_bm); // Clear DIR bit (set direction to input ==> floating)
    MB_DB_VPORT.OUT &= (~MB_DB_bm); // Set output value to 0
  } else {                          // device selected
    MB_DB_VPORT.OUT |= MB_DB_bm;    // Set output value to 1
    MB_DB_VPORT.DIR |= MB_DB_bm;    // Set DIR bit (set direction to output)
  }
}

/*!
      @brief default constructor for class k197ButtonCluster
*/
k197ButtonCluster::k197ButtonCluster() {}

/*!
    @brief  setup the push butto cluster. Must be called first, before any other
   member function

    Note that for simplicity the pins used are hardwired in setup()
*/
void k197ButtonCluster::setup() {
  pinConfigure(UI_STO, (PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INVERT_OFF |
                        PIN_INLVL_SCHMITT | PIN_ISC_ENABLE));
  pinConfigure(UI_RCL, (PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INVERT_OFF |
                        PIN_INLVL_SCHMITT | PIN_ISC_ENABLE));
  pinConfigure(UI_REL, (PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INVERT_OFF |
                        PIN_INLVL_SCHMITT | PIN_ISC_ENABLE));
  pinConfigure(UI_DB, (PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INVERT_OFF |
                       PIN_INLVL_SCHMITT | PIN_ISC_ENABLE));
  attachInterrupt(
      digitalPinToInterrupt(UI_STO), UI_STO_changing,
      CHANGE); // TODO: reflash bootloader to avoid using attachInterrupt
  attachInterrupt(
      digitalPinToInterrupt(UI_RCL), UI_RCL_changing,
      CHANGE); // TODO: reflash bootloader to avoid using attachInterrupt
  attachInterrupt(
      digitalPinToInterrupt(UI_REL), UI_REL_changing,
      CHANGE); // TODO: reflash bootloader to avoid using attachInterrupt
  attachInterrupt(
      digitalPinToInterrupt(UI_DB), UI_DB_changing,
      CHANGE); // TODO: reflash bootloader to avoid using attachInterrupt
  UI_STO_changing();
  UI_RCL_changing();
  UI_REL_changing();
  UI_DB_changing();
}

/*!
    @brief  set a call back for a push button in the cluster

    Set pinCallBack to NULL to remove call back

    @param pin the pin identifying the button
    @param pinCallBack the function (of type k197ButtonCluster:buttonCallBack)
   that will be called to handle button events
    @return true if pin corresponds to one of the buttons in the cluster,
   idnicating that the callback has been set or removed succesfully. False
   otherwise.
*/
boolean k197ButtonCluster::setCallback(uint8_t pin,
                                       buttonCallBack pinCallBack) {
  for (unsigned int i = 0; i < sizeof(buttonPinIn) / sizeof(buttonPinIn[0]);
       i++) {
    if (buttonPinIn[i] == pin) { // we found the slot...
      callBack[i] = pinCallBack;
      return true;
    }
  }
  return false;
}

/*!
    @brief  utility function to invoke a call back

    This function is used only within K197PushButton.cpp

    @param i the array index assigned to the push button
    @param buttonEvent the button event passed to the call back
*/
inline void invoke_callback(int i, uint8_t buttonEvent) {
  if (callBack[i] != NULL)
    callBack[i](buttonPinIn[i], buttonEvent);
}

/*!
    @brief  utility function to check for button events for a specific button

    This function is used only within K197PushButton.cpp

    @param i the array index assigned to the push button
*/
void k197ButtonCluster::check(
    uint8_t i) { // Check Index i (note: we assume the caller function checks
                 // that i is in range
  unsigned long now = millis();
  int btnow = digitalRead(buttonPinIn[i]);
  if (btnow != lastButtonState[i]) {
    // reset the debouncing timer
    lastDebounceTime[i] = now;
    // DebugOut.print(F("pin ")); DebugOut.print(buttonPinIn[i]);
    // DebugOut.print(F(" now="));DebugOut.println(btnow);
  }

  if ((now - lastDebounceTime[i]) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (btnow != buttonState[i]) {
      buttonState[i] = btnow;
      // The following actions are taken at Button release
      if (btnow == BUTTON_IDLE_STATE) { // button was just released
        invoke_callback(i, UIeventRelease);
        if ((now - startPressed[i]) > longPressTime) {
          invoke_callback(i, UIeventLongPress);
        } else if (startPressed[i] - lastReleased[i] < doubleClicktime) {
          invoke_callback(i, UIeventDoubleClick);
        } else {
          invoke_callback(i, UIeventClick);
        }
        lastReleased[i] = now;
      } else { // btnow == BUTTON_PRESSED_STATE   // button was just pressed
        invoke_callback(i, UIeventPress);
        startPressed[i] = now;
      }
    }
  }
  lastButtonState[i] = btnow;
}

/*!
    @brief  check for button events

    This function should be called frequently, e.g. inside loop()

    It will check if the button pin changed status, debounce and call the call
   back for the relevant events
*/
void k197ButtonCluster::check(void) {
  for (unsigned int i = 0; i < (sizeof(callBack) / sizeof(callBack[0])); i++) {
    check(i);
  }
}

/*!
    @brief  print event name to DebugOut

    Printing the event name can be useful during troubleshooting

    @param event the event to print
*/
void k197ButtonCluster::DebugOut_printEventName(uint8_t event) {
  switch (event) {
  case UIeventClick:
    DebugOut.print(F("eventClick"));
    break;
  case UIeventDoubleClick:
    DebugOut.print(F("eventDoubleClick"));
    break;
  case UIeventLongPress:
    DebugOut.print(F("eventLongPress"));
    break;
  case UIeventPress:
    DebugOut.print(F("eventPress"));
    break;
  case UIeventRelease:
    DebugOut.print(F("eventRelease"));
    break;
  default:
    DebugOut.print(F("unknown ev. "));
    DebugOut.print(event);
    break;
  }
}
