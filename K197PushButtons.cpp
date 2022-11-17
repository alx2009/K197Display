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

*/
/**************************************************************************/

#include "K197PushButtons.h"
#include <Arduino.h>
#include <Event.h>
#define CCL_CLKSEL_gm 1
#include <Logic.h>

#include "debugUtil.h"

#include "pinout.h"

// In the following we instantiate a bunch of arrays to keep track of the status
// of each push button. Essentially we need timers to keep track of various time
// intervals (debouncing, pressed, released, log press, etc.)
// All timers are in us (microseconds)

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
    0UL, 0UL, 0UL, 0UL}; ///< micros() the last time the button pin was toggled
unsigned long startPressed[] = {0UL, 0UL, 0UL,
                                0UL}; ///< micros() when last pressed

unsigned long lastHold[] = {0UL, 0UL, 0UL,
                            0UL}; ///< micros() when last hold event generated

unsigned long lastReleased[] = {0UL, 0UL, 0UL,
                                0UL}; ///< micros() when last released

// The following four functions are the interrupt handlers for each push button.
// They are normal functions because we are using attachInterrupt (dxCore have
// more efficient alternatives, but attachInterrupt is kind of the default and
// it works for now). The interrupt handler will simulate the push of the button
// on the 197/197A main board in transparent mode. This is completely independent from the handling of the button events in the rest of the sketch

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
    @brief  setup the push button cluster. Must be called first, before any
   other member function Note that for simplicity the pins used are hardwired in
   setup()
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
  if (transparentMode)
    attachPinInterrupts();

  attachTimerInterrupts();
}

/*!
    @brief  protected member function used to attach interrupts when enabling
   transparent mode
    @details This function is used only within K197PushButton.cpp
*/
void k197ButtonCluster::attachPinInterrupts() {
  DebugOut.println(F("Attach Interrupts"));
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
    @brief  protected member function used to deattach interrupts when disabling
   transparent mode
    @details This function is used only within K197PushButton.cpp
*/
void k197ButtonCluster::detachPinInterrupts() {
  DebugOut.println(F("Detach Interrupts"));
  detachInterrupt(digitalPinToInterrupt(UI_DB));
  detachInterrupt(digitalPinToInterrupt(UI_REL));
  detachInterrupt(digitalPinToInterrupt(UI_RCL));
  detachInterrupt(digitalPinToInterrupt(UI_STO));

  setup();

  for (unsigned int i = 0; i < (sizeof(callBack) / sizeof(callBack[0])); i++) {
    unsigned long now = micros();
    int btnow = digitalRead(buttonPinIn[i]);
    // DebugOut.print(F("Btn. ")); DebugOut.print(i); DebugOut.print(F(", Pin
    // ")); DebugOut.print(buttonPinIn[i]); DebugOut.print(F("="));
    // DebugOut.println(btnow);
    lastButtonState[i] = btnow;
    buttonState[i] = btnow;
    lastDebounceTime[i] = now;
  }
}

/*!
    @brief enable or disable transparent mode
    @details when transparent mode is enabled, push button presses and releases
   are passed to the K197 in real time via interrupt handler. when disabled, the
   push button events must be explicitly passed to the K197 from the callback
   function. In such a way events can be filtered as required
    @param newMode true enables transparent mode, false disables it
*/
void k197ButtonCluster::setTransparentMode(bool newMode) {
  if (newMode == transparentMode)
    return; // Nothing to do
  transparentMode = newMode;
  if (newMode)
    attachPinInterrupts();
  else
    detachPinInterrupts();
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
bool k197ButtonCluster::setCallback(K197UIeventsource eventSource, buttonCallBack pinCallBack) {
  uint8_t pin = (uint8_t) eventSource;
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

    @details This function is used only within K197PushButton.cpp

    @param i the array index assigned to the push button
    @param eventType the button event passed to the call back
*/
inline void invoke_callback(int i, K197UIeventType eventType) {
  if (callBack[i] != NULL)
    callBack[i]( (K197UIeventsource) buttonPinIn[i], eventType);
}

/*!
    @brief  check for button events for a specific button

    This function is used only within K197PushButton.cpp

    @param i the array index assigned to the push button
*/
void k197ButtonCluster::check(
    uint8_t i) { // Check Index i (note: we assume the caller function checks
                 // that i is in range
  unsigned long now = micros();
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

    // if the button is pressed, we handle LongPress & Hold
    if (buttonState[i] == BUTTON_PRESSED_STATE) {
      if (now - startPressed[i] > longPressTime) {
        if (startPressed[i] == lastHold[i]) { // 1st hold event is a LongPress
          invoke_callback(i, UIeventLongPress);
          lastHold[i] = now;
        } else if (now - lastHold[i] > holdTime) { // hold event
          invoke_callback(i, UIeventHold);
          lastHold[i] = now;
        }
      }
    }

    // if the button state has changed:
    if (btnow != buttonState[i]) {
      buttonState[i] = btnow;
      // The following actions are taken at Button release
      if (btnow == BUTTON_IDLE_STATE) { // button was just released
        invoke_callback(i, UIeventRelease);
        if ((now - startPressed[i]) > longPressTime) {
          invoke_callback(i, UIeventLongClick);
        } else if (startPressed[i] - lastReleased[i] < doubleClicktime) {
          invoke_callback(i, UIeventClick);
          invoke_callback(i, UIeventDoubleClick);
        } else {
          invoke_callback(i, UIeventClick);
        }
        lastReleased[i] = now;
      } else { // btnow == BUTTON_PRESSED_STATE   // button was just pressed
        invoke_callback(i, UIeventPress);
        startPressed[i] = now;
        lastHold[i] = now;
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
  checkNew();
}

/*!
    @brief  print event name to DebugOut

    Printing the event name can be useful during troubleshooting

    @param event the event to print
*/
void k197ButtonCluster::DebugOut_printEventName(K197UIeventType event) {
  switch (event) {
  case UIeventClick:
    DebugOut.print(F("eventClick"));
    break;
  case UIeventDoubleClick:
    DebugOut.print(F("eventDoubleClick"));
    break;
  case UIeventLongClick:
    DebugOut.print(F("eventLongClick"));
    break;
  case UIeventPress:
    DebugOut.print(F("eventPress"));
    break;
  case UIeventLongPress:
    DebugOut.print(F("eventLongPress"));
    break;
  case UIeventHold:
    DebugOut.print(F("eventHold"));
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

/////////////////////////////////////////////////////////////////////////
// New implementation, interrupt based
/////////////////////////////////////////////////////////////////////////

#define NO_DATA 0xff
volatile static byte fifo[] {0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff};
#define NUM_FIFO_RECORDS ( sizeof(fifo) / sizeof(fifo[0]) )

volatile static int8_t front=0;
// We use rear in the interrupt handler, hence the use of GPIO3
// Initialized inside k197ButtonCluster::attachTimerInterrupts()
#define rear GPIOR3 

static inline int getSize() {
   int size=0;
   for (int i=0; i<NUM_FIFO_RECORDS; i++) {
       if (fifo[i]!=NO_DATA) size++;
   }
   return size;
}

static inline bool isEmpty() {
  return fifo[front] == NO_DATA; // self-explaining :-)
}

static inline bool isFull() {
    // If we do not have free space than the queue must be full
    fifo[(rear + 1) % NUM_FIFO_RECORDS] != NO_DATA; 
}

// Utility function to push an item at the rear of the fifo
// No check of FIFO size because a) it should not happen: no user can be that fast b) we are going to lose clicks anyway c) the user or the WDT will recover (depending on the cause) 
// TODO: add a way to detect and recover in pull()
static inline void push(byte b) { 
    rear = (rear+1) % NUM_FIFO_RECORDS;
    fifo[rear] = b;
}

// Utility function to pull the item at the front of the fifo
static inline byte pull() {
  if (isEmpty()) return NO_DATA;
  byte x = fifo[front];
  fifo[front] = NO_DATA; // Now this slot is empty...
  front = (front + 1) % NUM_FIFO_RECORDS;
  return x;  
}

// Interrupt handler for the CCL vector. Whatever the CCL causing the event, we push a copy of the pushbutton pins to the fifo queue 
// This should work in dxCore 1.5.0 when it is released...
//ISR(CCL_CCL_vect) {

//Normal function, required with current release of dxCore
void CCL_interrupt_handler() {
  //CCL.INTFLAGS =  CCL.INTFLAGS; // We prefer to enter the interrupts twice rather than missing an event
  push( (UI_STO_VPORT.IN & (UI_STO_bm | UI_RCL_bm)) | (UI_REL_VPORT.IN & (UI_REL_bm | UI_DB_bm)) ); 
}

void k197ButtonCluster::attachTimerInterrupts() {
  rear=(uint8_t) -1; //Initialize register
  
  // Route pins for pushbuttons to Logic block 0 & 1
  UI_STO_Event.set_generator(UI_STO);
  UI_STO_Event.set_user(user::ccl0_event_a); 
  UI_RCL_Event.set_generator(UI_RCL);
  UI_RCL_Event.set_user(user::ccl1_event_a); 
  UI_REL_Event.set_generator(UI_REL);
  UI_REL_Event.set_user(user::ccl2_event_a); 
  UI_DB_Event.set_generator(UI_DB);
  UI_DB_Event.set_user(user::ccl3_event_a); 

  // Initialize logic block 0
  Logic0.enable = true;         // Enable logic block 0
  Logic0.input0 = in::event_a;  // Connect input 0 to ccl0_event_a (STO pin via UI_STO_Event)
  //Logic0.input1 = in::event_b;  // Connect input 0 to ccl0_event_a (STO pin via UI_STO_Event)
  Logic0.clocksource=clocksource::osc1k; //1024Hz clock
  Logic0.filter=filter::filter; // Debounce (must be stable for 4 clock cycles)
  Logic0.truth=0x55; //0x77;
  Logic0.init();

  // Initialize logic block 1
  Logic1.enable = true;         // Enable logic block 0
  Logic1.input0 = in::event_a;  // Connect input 0 to ccl1_event_a (RCL pin via UI_RCL_Event)
  Logic1.clocksource=clocksource::osc1k; //1024Hz clock
  Logic1.filter=filter::filter; // Debounce (must be stable for 4 clock cycles)
  Logic1.truth=0x55;
  Logic1.init();

  // Initialize logic block 2
  Logic2.enable = true;         // Enable logic block 0
  Logic2.input0 = in::event_a;  // Connect input 0 to ccl2_event_a (REL pin via UI_REL_Event)
  Logic2.clocksource=clocksource::osc1k; //1024Hz clock
  Logic2.filter=filter::filter; // Debounce (must be stable for 4 clock cycles)
  Logic2.truth=0x55;
  Logic2.init();

  // Initialize logic block 3
  Logic3.enable = true;         // Enable logic block 0
  Logic3.input0 = in::event_a;  // Connect input 0 to ccl3_event_a (DB pin via UI_DB_Event)
  Logic3.clocksource=clocksource::osc1k; //1024Hz clock
  Logic3.filter=filter::filter; // Debounce (must be stable for 4 clock cycles)
  Logic3.truth=0x55;
  Logic3.init();

  //Event0.set_generator(gen::ccl0_out);
  //Event0.set_user(user::evouta_pin_pa7); 
  //Event0.start();

  UI_STO_Event.start();
  UI_RCL_Event.start();
  UI_REL_Event.start();
  UI_DB_Event.start();
  Logic::start();

  Logic0.attachInterrupt(CCL_interrupt_handler, CHANGE);
  Logic1.attachInterrupt(CCL_interrupt_handler, CHANGE);
  Logic2.attachInterrupt(CCL_interrupt_handler, CHANGE);
  Logic3.attachInterrupt(CCL_interrupt_handler, CHANGE);

  //CCL.INTCTRL0|=0b00001111; //Will replace attachInterrupt when dxCore 1.5.0 will be released...
  //DebugOut.print(F("CCL.LUT0CTRLA=")); DebugOut.println(CCL.LUT0CTRLA, HEX);
}

void k197ButtonCluster::checkNew() {
   cli();
   int8_t n=getSize();
   bool b = isFull();
   byte x = pull();
   sei();
   if (b) {
       DebugOut.println(F("<========== FIFO is Full !!! ================>")); DebugOut.print(n);
   }
   if (x != NO_DATA) {
       DebugOut.print(F("Items=")); DebugOut.print(n);
       DebugOut.print(F(", fifo: 0x")); DebugOut.println(x, HEX);
   }
}
