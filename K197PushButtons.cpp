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
/*
uint8_t buttonPinOut[] = {MB_STO, MB_RCL, MB_REL,
                          MB_DB}; ///< index to pin mapping for output
*/
k197ButtonCluster::buttonCallBack callBack = NULL; ///< Stores the call back for each button
uint8_t buttonState[] = {
    BUTTON_IDLE_STATE, BUTTON_IDLE_STATE, BUTTON_IDLE_STATE,
    BUTTON_IDLE_STATE}; ///< the current reading from the button pin
uint8_t lastButtonState[] = {
    BUTTON_IDLE_STATE, BUTTON_IDLE_STATE, BUTTON_IDLE_STATE,
    BUTTON_IDLE_STATE}; ///< the previous reading from the button pin
unsigned long startPressed[] = {0UL, 0UL, 0UL,
                                0UL}; ///< micros() when last pressed
unsigned long lastHold[] = {0UL, 0UL, 0UL,
                            0UL}; ///< micros() when last hold event generated
unsigned long lastReleased[] = {0UL, 0UL, 0UL,
                                0UL}; ///< micros() when last released

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

  attachTimerInterrupts();
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
void k197ButtonCluster::setCallback(buttonCallBack pinCallBack) {
    callBack = pinCallBack;
}

/*!
    @brief  utility function to invoke a call back

    @details This function is used only within K197PushButton.cpp

    @param i the array index assigned to the push button
    @param eventType the button event passed to the call back
*/
inline void invoke_callback(int i, K197UIeventType eventType) {
  if (callBack != NULL) {
    callBack( (K197UIeventsource) buttonPinIn[i], eventType);
  }
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
// Handling of button events. This is done in two stages:
// The first stage is HW based, using the event system and the 4 CCL (one for each button). 
// Button status changes (after CCL filter for debouncing) generate CCL interrupt. The handler queues the button state in fifo queue
// The events are then extracted from the fifo and processed outside the interrupt handler by checkNew(). This member function must be called periodically from the main Arduino Loop. Change() will:
// compared to the previous release, this implementation will never lose clicks (with reasonably sized fifo). 
// At the same time most of the processing is done outside the interrupt handler which is kept reasonably small. This is needed 
// to avoid holding with other interruts too much [currently we miss one measurement when buttons are pressed very quickly. 
// This should be solved when dxCore start supporting more efficient interrupt handlers for the Logic library]
// Despite the need for a fifo queue, the new implementation uses less RAM due to HW debouncing and other optimizations
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
   for (unsigned int i=0; i<NUM_FIFO_RECORDS; i++) {
       if (fifo[i]!=NO_DATA) size++;
   }
   return size;
}

static inline bool isEmpty() {
  return fifo[front] == NO_DATA; // self-explaining :-)
}

static inline bool isFull() {
    // If we do not have free space than the queue must be full
    return fifo[(rear + 1) % NUM_FIFO_RECORDS] != NO_DATA; 
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

inline uint8_t getButtonState(byte b) {
  return b == 0 ? BUTTON_PRESSED_STATE : BUTTON_IDLE_STATE;
}

void initButton(uint8_t i, uint8_t btnow, unsigned long now) {
    buttonState[i] = btnow;
    lastButtonState[i] = btnow;
    startPressed[i] = now;
    lastHold[i] = now;
    lastReleased[i] = now;
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
  Logic0.truth=0x55; //0x55;
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

  // Initialize buttons initial state 
  unsigned long now = micros(); 
  cli();
  push( (UI_STO_VPORT.IN & (UI_STO_bm | UI_RCL_bm)) | (UI_REL_VPORT.IN & (UI_REL_bm | UI_DB_bm)) ); 
  byte x = pull();
  sei();
  initButton(0, getButtonState(x & UI_STO_bm), now);
  initButton(1, getButtonState(x & UI_RCL_bm), now);
  initButton(2, getButtonState(x & UI_REL_bm), now);
  initButton(3, getButtonState(x & UI_DB_bm), now);
  cli();
  push( (UI_STO_VPORT.IN & (UI_STO_bm | UI_RCL_bm)) | (UI_REL_VPORT.IN & (UI_REL_bm | UI_DB_bm)) ); //Make sure we do not lose any state change  
  sei();
  
}

/*!
    @brief  check for button events

    This function should be called frequently, e.g. inside loop()

    It will check if the button pin changed status, debounce and call the call
   back for the relevant events
*/
void k197ButtonCluster::checkNew() {
   unsigned long now = micros(); 
   for (int i=0; i<4; i++) {       
       if (buttonState[i] == BUTTON_PRESSED_STATE) {
           checkPressed(i, now);
       }
   }
  
   cli();
   int8_t n=getSize();
   bool b = isFull();
   byte x = pull();
   sei();
   if (b) {
       DebugOut.println(F("FIFO Full")); DebugOut.print(n);
   }
   if (x != NO_DATA) { // We have a new raw event
       now = micros(); 
       //DebugOut.print(F("Items=")); DebugOut.print(n);
       //DebugOut.print(F(", fifo: 0x")); DebugOut.println(x, HEX);
       checkNew(0, getButtonState(x & UI_STO_bm), now);
       checkNew(1, getButtonState(x & UI_RCL_bm), now);
       checkNew(2, getButtonState(x & UI_REL_bm), now);
       checkNew(3, getButtonState(x & UI_DB_bm), now);
   }
}

/*!
    @brief  check for button events for buttons that are already pressed

    @details when a button is in pressed state, LongPress and Hold events are generated if the button is hold more than the longPressTime
    This function is used only within K197PushButton.cpp. Note that:
    - there is no check on i, so the caller must ensure it is in range
    - the caller must ensure a button is in the pressed state before calling this function
    Note that the button state will never change during this function call. 

    @param i the array index assigned to the push button
    @param now the current value of micros()    
*/
void k197ButtonCluster::checkPressed(uint8_t i, unsigned long now) {   
    // if the button is already pressed, we handle LongPress & Hold 
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
/*!
    @brief  check for button events for a specific button

    @details This function is called when a new raw event is received from the HW via the FIFO
    The function is only used only within K197PushButton.cpp. 
    that is no check on i, so the caller must ensure it is in range

    @param i the array index assigned to the push button
    @param btnow the current state of the button as indicated in the raw event
    @param now the current value of micros()    
*/
void k197ButtonCluster::checkNew(uint8_t i, uint8_t btnow, unsigned long now) {   
    if (btnow != buttonState[i]) { // state has changed
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
    lastButtonState[i] = btnow;
}
