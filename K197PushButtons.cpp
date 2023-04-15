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

#include "debugUtil.h"

#include "pinout.h"

k197ButtonCluster pushbuttons; ///< this object is used to interact with the
                               ///< push-button cluster

k197ButtonCluster::buttonCallBack callBack =
    NULL; ///< Stores the call back for each button

// In the following we instantiate a bunch of arrays to keep track of the status
// of the push buttons. Essentially we need timers to keep track of various time
// intervals (debouncing, pressed, released, log press, etc.)
// All timers are in us (microseconds)

const uint8_t buttonPinIn[] PROGMEM = {
    UI_STO, UI_RCL, UI_REL,
    UI_DB}; ///< index to pin mapping for UI push buttons
uint8_t buttonState[] = {
    BUTTON_IDLE_STATE, BUTTON_IDLE_STATE, BUTTON_IDLE_STATE,
    BUTTON_IDLE_STATE}; ///< the current reading from the button pin
unsigned long startPressed[] = {0UL, 0UL, 0UL,
                                0UL}; ///< micros() when last pressed
unsigned long lastHold[] = {0UL, 0UL, 0UL,
                            0UL}; ///< micros() when last hold event generated
unsigned long lastReleased[] = {0UL, 0UL, 0UL,
                                0UL}; ///< micros() when last released
bool enableDoubleClick[] = {true, true, true, true}; ///< 

/*!
    @brief  set a call back for a push button in the cluster

    Set clusterCallBack to NULL to remove call back

    @param clusterCallBack the function (of type
   k197ButtonCluster:buttonCallBack) that will be called to handle button events
*/
void k197ButtonCluster::setCallback(buttonCallBack clusterCallBack) {
  callBack = clusterCallBack;
}

/*!
    @brief  utility function to invoke a call back

    @details This function is used only within K197PushButton.cpp

    @param i the array index assigned to the push button
    @param eventType the button event passed to the call back
*/
static inline void invoke_callback(int i, K197UIeventType eventType) {
  if (callBack != NULL) {
    callBack((K197UIeventsource)pgm_read_byte(&buttonPinIn[i]), eventType);
  }
}

/*!
    @brief  check if a given pushbutton is pressed

    @details This function checks the button status updated to the last event
   processed by check() It does NOT reflect the current HW status. This is
   useful for example inside the event call back in order to check if two
   buttons have been pressed simultaneously. It will behave correctly even if
   there is a delay in the processing of the event, and the button was already
   released.

    @param eventSource the event source corresponding to the push button
    @return bool true if the button was pressed at the time the last event was
   generated
*/
bool k197ButtonCluster::isPressed(K197UIeventsource eventSource) {
  for (unsigned int i = 0; i < (sizeof(buttonState) / sizeof(buttonState[0]));
       i++) {
    if (pgm_read_byte(&buttonPinIn[i]) == (uint8_t)eventSource) {
      return buttonState[i] == BUTTON_PRESSED_STATE ? true : false;
    }
  }
  return false;
}

/*!
    @brief  print event name to DebugOut

    Printing the event name can be useful during troubleshooting

    @param event the event to print
*/
void k197ButtonCluster::DebugOut_printEventName(K197UIeventType event) {
  switch (event) {
  case UIeventClick:
    DebugOut.print(F("evClick"));
    break;
  case UIeventDoubleClick:
    DebugOut.print(F("evDbClick"));
    break;
  case UIeventLongClick:
    DebugOut.print(F("evLgClick"));
    break;
  case UIeventPress:
    DebugOut.print(F("evPress"));
    break;
  case UIeventLongPress:
    DebugOut.print(F("evLgPress"));
    break;
  case UIeventHold:
    DebugOut.print(F("evHold"));
    break;
  case UIeventRelease:
    DebugOut.print(F("evRls"));
    break;
  default:
    DebugOut.print(F("ev?"));
    DebugOut.print(event);
    break;
  }
}

/////////////////////////////////////////////////////////////////////////
// Handling and processing of button events. This is done in two stages:
// The first stage is HW based, using the event system and the 4 CCL (one for
// each button). Button status changes (after CCL filter for debouncing)
// generate CCL interrupts. The handler queues the button state in a fifo queue.
// The events are then extracted from the fifo and processed outside the
// interrupt handler by checkNew(). This member function must be called
// periodically from the main Arduino Loop. Compared to the previous release,
// this implementation will never lose clicks (with reasonably sized fifo). At
// the same time most of the processing is done outside the interrupt handler
// which is kept reasonably small. This is needed to avoid holding other
// interruts [currently we still miss one measurement when buttons are
// pressed very quickly. This should be solved when dxCore start supporting more
// efficient interrupt handlers for the Logic library]
// Despite the need for a fifo queue, the new implementation uses less RAM due
// to HW debouncing and other optimizations
/////////////////////////////////////////////////////////////////////////

#define fifo_NO_DATA                                                           \
  0xff ///< value returned when the fifo is empty (see fifo_pull())
volatile static byte fifo_records[]{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    }; ///< array implementing the FIFO queue, see fifo_pull()
#define fifo_MAX_RECORDS                                                       \
  (sizeof(fifo_records) /                                                      \
   sizeof(fifo_records[0])) ///< max number of records in the FIFO queue, see
                            ///< fifo_pull()

volatile static uint8_t fifo_front = 0x01; // index to the front of the queue, see fifo_pull()
    
//#define fifo_rear GPIOR0 //uncomment to use GPIOR0 to speed up slightly
#ifndef fifo_rear 
static volatile byte fifo_rear = 0x00; ///< index to the rear of the queue, see fifo_pull()
#endif

/*!
    @brief  get the number of records actually stored in the FIFO queue (see
   also fifo_pull())
    @details Note that this function is not thread safe with respect to
   other fifo_xxx functions. The call to this function should be bracketed between cli()/sei()

    @return the number of records currently in the FIFO queue
*/
static inline int fifo_getSize() {
  int size = 0;
  for (unsigned int i = 0; i < fifo_MAX_RECORDS; i++) {
    if (fifo_records[i] != fifo_NO_DATA)
      size++;
  }
  return size;
}

/*!
    @brief  check if the FIFO is empty
    @details    Note that this function is not thread safe with respect to
   other fifo_xxx functions. The call to this function should be bracketed between cli()/sei()

    @return true if the FIFO queue is empty
*/
static inline bool fifo_isEmpty() {
  return fifo_records[fifo_front] == fifo_NO_DATA ?  true : false; // self-explaining :-)
}

/*!
    @brief  check if the FIFO is full
    @details t   Note that this function is not thread safe with respect to
   other fifo_xxx functions. The call to this function should be bracketed between cli()/sei()

    @return true if the FIFO queue is empty
*/
static inline bool fifo_isFull() {
  // If we do not have free space than the queue must be full
  int idx = (fifo_rear + 1) % fifo_MAX_RECORDS;
  byte val = fifo_records[idx];
  if (val == fifo_NO_DATA) {
      return false;
  }
  DebugOut.print(F("+idx=")); DebugOut.print(idx); 
  DebugOut.print(F(", val=")); DebugOut.print(val); 
  DebugOut.print(F(", X=")); DebugOut.println(fifo_NO_DATA); 
  return  true;
}

/*!
    @brief push a record at the rear of the FIFO queue (see also fifo_pull())
    @details This function is optimized for use in an interrupt handler.
    
    If used both in the handler and outside, calls in sections of code that can be interrupted should be bracketed between cli()/sei()
    @param b the record that should be pushed at the rear of the FIFO queue
*/
static inline void fifo_push(byte b) {
  fifo_rear = (fifo_rear + 1) % fifo_MAX_RECORDS;
  fifo_records[fifo_rear] = b;
}

/*!
    @brief pull a record at the rear of the FIFO queue
    @details The fifo_xxx series of variables and functions implement a FIFO
queue which is used to pass "raw" button presses and release from the interrupt
handler to the main application thread. The particular implementation has been
chosen to optimize fifo_push, so it can be used efficiently in an interrupt handler.

    File static scope is used rather than a class as this seem to generate the
smallest and fastest interrupt handler.

    A small handler and the concurrency means that we can minimize the latency
to serve the other interrupts, in particular the SPI handler which need to be
served quickly to avoid losing data from the K197.

   Note that this function is not thread safe with respect to other fifo_xxx functions. 
   The call to this function should be bracketed between cli()/sei()

    @return the record just pulled from the front of the queue (or fifo_NO_DATA
if the queue is empty).
*/
static inline byte fifo_pull() {
  if (fifo_isEmpty())
    return fifo_NO_DATA;
  byte x = fifo_records[fifo_front];
  fifo_records[fifo_front] =
      fifo_NO_DATA; // here we risk a race condition with push(), but only if
                    // the FIFO is full
  fifo_front = (fifo_front + 1) % fifo_MAX_RECORDS;
  return x;
}

/*!
    @brief  Interrupt handler, called for CCL events
    @details Each one of the 4 CCL handles a button (see also
   k197ButtonCluster::setup()). The CCL CHANGE interrupt is used to detect a
   change of button state after the CCL filter (for HW debouncing).

   The handler simply push the logic level of the relevant pins to the rear of
   the fifo queue (see fifo_push().
   
   Note 2: the current implementation relies on STO being on the same I/O port
   as RCL, and REL with DB. A future HW revision will move all 4 buttons to the same I/O
   port, potentially optimizing the interrupt handler further.
*/
ISR(CCL_CCL_vect) { //__vector_7
  CCL.INTFLAGS =  CCL.INTFLAGS; // We prefer to enter the interrupts twice
                                //   rather than missing an event
  fifo_push((UI_STO_VPORT.IN & (UI_STO_bm | UI_RCL_bm)) |
            (UI_REL_VPORT.IN & (UI_REL_bm | UI_DB_bm)));
}

/*!
    @brief utility function, maps from I/O pin level to the correct status of
   the button
    @details the current implemntation assumes 0 means pressed and !=0 means not
   pressed
    @return BUTTON_PRESSED_STATE or BUTTON_IDLE_STATE
*/
static inline uint8_t getButtonState(byte b) {
  return b == 0 ? BUTTON_PRESSED_STATE : BUTTON_IDLE_STATE;
}

/*!
    @brief utility function, initialize one push button
    @param i index of the button
    @param btnow current state of the button (BUTTON_PRESSED_STATE or
   BUTTON_IDLE_STATE)
    @param now current value of micros()
*/
static void initButton(uint8_t i, uint8_t btnow, unsigned long now) {
  buttonState[i] = btnow;
  startPressed[i] = now;
  lastHold[i] = now;
  lastReleased[i] = now;
}

/*!
    @brief  setup the push button cluster.

    @details Must be called first, before any other member function.

    Pushbutton are handled in two stages. The first stage consists of the event
   system and the four CCLs.

    The CCL are used for HW debouncing each button. One CCL for each button. The
   CCL is set to filter any change that doesn't last for at least 4 clock
   cycles. With the CCL clock source set to the 1024Hz clock, the debouncing
   period is about 4 ms. This works well with the buttons used, at least when
   they are new. Should there be a need to increase the debounce period, a timer
   should be connected to input 2 of each CCL via the event system.

    afdter the filter, the CCL CHANGE interrupt is used to detect a change of
   button state after the CCL filter (for HW debouncing). The handler simply
   push the logic level of the relevant pins to the rear of the fifo queue (see
   fifo_push()).

    The second stage pays off when the checkNew() method is called. This method
   will retrieve the pin status from the fifo queue (fifo_pull()), to generate
   the various button events at the right timing.

    Timing of the events is done in checkNew() rather than in the interrupt
   handler. This keeps the interrupt handler short but it means that the timing
   of the log press, hold and double click events may vary a bit from time to
   time. For this application this is fully acceptable.

    The relative sequence of events is always maintained, and no button click
   should be missed.
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

  fifo_front = 0x01;
  fifo_rear = 0x00; // next push will go in 0x01, the front of the queue

  // Route pins for pushbuttons to Logic blocks 0-3
  EVSYS.CHANNEL2 = Ch2_UI_STO_Ev_src;
  EVSYS.CHANNEL3 = Ch3_UI_RCL_Ev_src;
  EVSYS.CHANNEL4 = Ch4_UI_REL_Ev_src;
  EVSYS.CHANNEL5 = Ch5_UI_DB_Ev_src;

  EVSYS.USERCCLLUT0A = EVSYS_USER_CHANNEL2_gc; // channel 2 ==> CCL LUT0 event input A 
  EVSYS.USERCCLLUT1A = EVSYS_USER_CHANNEL3_gc; // channel 3 ==> CCL LUT1 event input A 
  EVSYS.USERCCLLUT2A = EVSYS_USER_CHANNEL4_gc; // channel 4 ==> CCL LUT2 event input A 
  EVSYS.USERCCLLUT3A = EVSYS_USER_CHANNEL5_gc; // channel 5 ==> CCL LUT3 event input A 

  // Makes ure the CCL is disabled before configuration
  CCL.CTRLA = 0x00; // Disable, nor running in standby
  CCL.LUT0CTRLA = 0x00; // Disable CCL LUT0
  CCL.LUT1CTRLA = 0x00; // Disable CCL LUT0
  CCL.LUT2CTRLA = 0x00; // Disable CCL LUT0
  CCL.LUT3CTRLA = 0x00; // Disable CCL LUT0

  //Make sure the sequencers are disabled
  CCL.SEQCTRL0 = 0x00;
  CCL.SEQCTRL0 = 0x00;

  //Make sure Interrupts are disabled
  CCL.INTCTRL0 = 0x00;
  
  // Initialize logic block 0
  // ccl0 event a ==> input 0 (STO via Ev. ch 2)
  CCL.TRUTH0 = 0x55; // 0x55;
  CCL.LUT0CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc; 
  CCL.LUT0CTRLC = CCL_INSEL2_MASK_gc ;                       
  CCL.LUT0CTRLA = CCL_FILTSEL_FILTER_gc | CCL_CLKSRC_OSC1K_gc | CCL_ENABLE_bm ;

  // Initialize logic block 1
  // ccl0 event a ==> input 0 (RCL via Ev. ch 3)
  CCL.TRUTH1 = 0x55; // 0x55;
  CCL.LUT1CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc; 
  CCL.LUT1CTRLC = CCL_INSEL2_MASK_gc ;                       
  CCL.LUT1CTRLA = CCL_FILTSEL_FILTER_gc | CCL_CLKSRC_OSC1K_gc | CCL_ENABLE_bm ;

  // Initialize logic block 2
  // ccl0 event a ==> input 0 (REL via Ev. ch 4)
  CCL.TRUTH2 = 0x55; // 0x55;
  CCL.LUT2CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc; 
  CCL.LUT2CTRLC = CCL_INSEL2_MASK_gc ;                       
  CCL.LUT2CTRLA = CCL_FILTSEL_FILTER_gc | CCL_CLKSRC_OSC1K_gc | CCL_ENABLE_bm ;

  // Initialize logic block 3
  // ccl0 event a ==> input 0 (Db via Ev. ch 5)
  CCL.TRUTH3 = 0x55; // 0x55;
  CCL.LUT3CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc; 
  CCL.LUT3CTRLC = CCL_INSEL2_MASK_gc ;                       
  CCL.LUT3CTRLA = CCL_FILTSEL_FILTER_gc | CCL_CLKSRC_OSC1K_gc | CCL_ENABLE_bm ;

  // Setup interrupts and then enable the CCL peripheral
  CCL.INTCTRL0 = CCL_INTMODE0_BOTH_gc | CCL_INTMODE1_BOTH_gc | CCL_INTMODE2_BOTH_gc | CCL_INTMODE3_BOTH_gc;
  CCL.CTRLA = CCL_ENABLE_bm; // Enabled, nor running in standby

  //DebugOut.print(F("CCL.LUT0CTRLA=")); DebugOut.println(CCL.LUT0CTRLA, HEX);

  // Initialize buttons initial state. Needed to handle buttons already pressed
  // at startup or reset (e.g. watchdog reset).
  unsigned long now = micros();
  cli();
  byte x = (UI_STO_VPORT.IN & (UI_STO_bm | UI_RCL_bm)) |
            (UI_REL_VPORT.IN & (UI_REL_bm | UI_DB_bm));
  sei();
  initButton(0, getButtonState(x & UI_STO_bm), now);
  initButton(1, getButtonState(x & UI_RCL_bm), now);
  initButton(2, getButtonState(x & UI_REL_bm), now);
  initButton(3, getButtonState(x & UI_DB_bm), now);
  // Setup the click timer.
  setupClicktimer();
}

/*!
    @brief  check for button events

    This function should be called frequently, e.g. inside loop()

    It will check if the button pin changed status, debounce and call the call
   back for the relevant events
*/
void k197ButtonCluster::checkNew() {
  unsigned long now = micros();
  for (int i = 0; i < 4; i++) {
    if (buttonState[i] == BUTTON_PRESSED_STATE) {
      checkPressed(i, now);
    }
  }
  cli();
  bool b = fifo_isFull(); // We check now because it is unlikely we could detect a full FIFO otherwise...
  if (b || (!fifo_isEmpty()) ) {
    if (b) {
       DebugOut.print(b);
       DebugOut.print(F(" idx=")); DebugOut.print((fifo_rear + 1) % fifo_MAX_RECORDS); 
       DebugOut.print(F(", val=")); DebugOut.println(fifo_records[(fifo_rear + 1) % fifo_MAX_RECORDS]); 
    }
    
    DebugOut.print(F(" front=")); DebugOut.print(fifo_front); DebugOut.print(F(", rear=")); DebugOut.println(fifo_rear); 
    for (unsigned int i=0; i<fifo_MAX_RECORDS; i++) {
      DebugOut.print(fifo_records[i], HEX); DebugOut.print(CH_SPACE);
    }
    DebugOut.println();
  }
  byte x = fifo_pull();
  sei();
  if (b) {
    DebugOut.println(F("FIFO!"));
  }
  if (x != fifo_NO_DATA) { // We have a new raw event
    now = micros();
    // DebugOut.print(F("Items=")); DebugOut.print(n);
    // DebugOut.print(F(", fifo: 0x")); DebugOut.println(x, HEX);
    checkNew(0, getButtonState(x & UI_STO_bm), now);
    checkNew(1, getButtonState(x & UI_RCL_bm), now);
    checkNew(2, getButtonState(x & UI_REL_bm), now);
    checkNew(3, getButtonState(x & UI_DB_bm), now);
  }
}

/*!
    @brief  check for button events for buttons that are already pressed

    @details when a button is in pressed state, LongPress and Hold events are
   generated if the button is hold more than the longPressTime This function is
   used only within K197PushButton.cpp. Note that:
    - there is no check on i, so the caller must ensure it is in range
    - the caller must ensure a button is in the pressed state before calling
   this function Note that the button state will never change during this
   function call.

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

    @details This function is called when a new raw event is received from the
   HW via the FIFO The function is only used only within K197PushButton.cpp.
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
        enableDoubleClick[i] = false; // This will prevent that the next click is recognized as a double click...
      } else if (startPressed[i] - lastReleased[i] < doubleClicktime) {
        invoke_callback(i, UIeventClick);
        if (enableDoubleClick[i]) invoke_callback(i, UIeventDoubleClick);
        enableDoubleClick[i] = false; // This prevents a third click to give raise to a further double click...
        // DebugOut.print(F("Dbclick time: "));
        // DebugOut.println(now-lastReleased[i]);
      } else {
        enableDoubleClick[i] = true; // This will enable double click for the next click
        invoke_callback(i, UIeventClick);
        // DebugOut.print(F("Click time: "));
        // DebugOut.println(now-startPressed[i]);
      }
      lastReleased[i] = now;
    } else { // btnow == BUTTON_PRESSED_STATE   // button was just pressed
      invoke_callback(i, UIeventPress);
      startPressed[i] = now;
      lastHold[i] = now;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////
// Handling of clicks for REL button
//////////////////////////////////////////////////////////////////////////////////

/*!
    @brief  setup the REL click timer

    @details This function isetup a timer used to simulate button REL clicks
   towards the K197 MB.

    The reason is that we use the long press of REL to trigger the configuration
   menu. So we need to handle this button differently. For the other buttons we
   can mirror the press and release events directly to the K197. For the REL
   button we do not know if the event will be a long or short press. So we need
   to wait for the click event, and then simulate a press and release towards
   the k197 main board. The K197 requires a button to be pressed a relatively
   long time. Furthermore, the button must be idle for about 0.5s before a
   second press can be recognized.

    To avoid blocking processing of events for this much time, we queue the
   button clicks and handle them via a TCA timer (for chips supporting multiple
   TCA the timer to use is defined in pinout.h)
*/
void k197ButtonCluster::setupClicktimer() {
  // Tave over and reset TCAx
  takeOverTCA(); // calls the appropriate dxCore function - force the core to
                 // stop using the timer and reset to the startup configuration
  AVR_TCA_PORT.SINGLE.CTRLA = 0x00; // Make sure the timer is disabled
  AVR_TCA_PORT.SINGLE.CTRLESET =
      TCA_SINGLE_CMD_RESET_gc |
      0x03; // Set CMD to RESET to do a hard reset of the timer
  AVR_TCA_PORT.SINGLE.CTRLD = 0x00;   // Make sure we are in single mode
  AVR_TCA_PORT.SINGLE.INTCTRL = 0x00; // Disable all interrupts
  AVR_TCA_PORT.SINGLE.INTFLAGS =
      TCA_SINGLE_OVF_bm | TCA_SINGLE_CMP0_bm; // Clear interrupt flags
}

//#define click_counter GPIOR2 //Uncomment to use General Purpose Register GPIOR2 to speed up the interrupt handler (slightly)
#ifndef click_counter
static volatile byte click_counter = 0x00;
#endif

/*!
    @brief  schedule one (more) click of the REL button towards the K197 main
   board

    @details the function does not wait for the click to be completed, it
   schedules the click and starts the timer (if not already started). In the
   latter case interrupts are disabled for a short time (while the TCA timer is
   started).
   There is a delay of a full timer cycle before the first click. This is by
   design, so that a double click can cancel the scheduled clicks to
   perform an alternative action

*/
void k197ButtonCluster::clickREL() {
  cli();
  if (click_counter > REL_max_pending_clicks) {
    sei();
    return;
  }
  click_counter++;
  if ((AVR_TCA_PORT.SINGLE.CTRLA & TCA_SINGLE_ENABLE_bm) ==
      0x00) { // We need to start the timer
    // MB_REL_VPORT.DIR |= MB_REL_bm; // Set REL pin to high
    // MB_REL_VPORT.OUT |= MB_REL_bm; // Set REL pin to output
    //  VPORTA.OUT |= 0x80;  // Turn on builtin LED

    AVR_TCA_PORT.SINGLE.CTRLB =
        TCA_SINGLE_WGMODE_NORMAL_gc; // Normal mode. Disabled: Compare outputs,
                                     // lock update.
    AVR_TCA_PORT.SINGLE.EVCTRL &=
        ~(TCA_SINGLE_CNTEI_bm); // Disable event counting
    AVR_TCA_PORT.SINGLE.PER = totalCount;
    AVR_TCA_PORT.SINGLE.CMP0 = pulseCount;
    AVR_TCA_PORT.SINGLE.INTCTRL =
        TCA_SINGLE_OVF_bm | TCA_SINGLE_CMP0_bm; // Enable overflow interrupt
    AVR_TCA_PORT.SINGLE.CTRLA =
        TCA_SINGLE_CLKSEL_DIV1024_gc |
        TCA_SINGLE_ENABLE_bm; // enable the timer with clock DIV1024

    click_counter = 0x01;
    // DebugOut.print(F("Timer started, PER=0x"));
    // DebugOut.print(AVR_TCA_PORT.SINGLE.PER); DebugOut.print(", CMP0=");
    // DebugOut.println(AVR_TCA_PORT.SINGLE.CMP0);

  } else { // engine already running
    // DebugOut.println(F("Timer already running"));
  }
  sei();
}

/*!
    @brief  cancel all the REL button clicks that may have been scheduled
   board

    @details the function reset the counter, which will prevent any new click to
   be initiated Any simulated presses already ongoing are not affected, but no
   new click will be initiated thereafter

*/
void k197ButtonCluster::cancelClickREL() { 
   cli();
   click_counter = 0x00;
   sei();
}

/*!
    @brief  Interrupt handler, called for TCA timer overflow events
    @details This interrupt is called after enough time is passed from the last
   release, so that a new press can be recognized by the k197.
    - If any click click is scheduled (click_counter>0) the MB_REL port is set to high,
   and click_counter is decremented
    - If this was the last scheduled click (click_counter==0) the timer is stopped
   and for good measure TCA interrupts are disabled.
   Note that the TCA instance used is defined in pinout.h
*/
ISR(TCA_OVF_vect) {                                 // __vector_9
  AVR_TCA_PORT.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm; // Clear flag
  if (click_counter > 0) {                // At least one more click to generate
    MB_REL_VPORT.DIR |= MB_REL_bm; // Set REL pin to high
    MB_REL_VPORT.OUT |= MB_REL_bm; // Set REL pin to output
    // VPORTA.OUT |= 0x80;  // Turn on builtin LED
    click_counter--;
  } else {                              // We stop here
    AVR_TCA_PORT.SINGLE.INTCTRL = 0x00; // Disable all interrupts
    AVR_TCA_PORT.SINGLE.CTRLA =
        TCA_SINGLE_CLKSEL_DIV1024_gc; // disable the timer
  }
}

/*!
    @brief  Interrupt handler, called for CMP0 compare events
    @details This interrupt is called after enough time is passed so that a
   press is recognized by the k197. This handler simply set the port to high
   impedence (the pulldown resistor in the k197 main board will pull the pin to
   LOW). The timer will continue to run as we need to wait an extra "idle" time
   before we can generate a new click (see also ISR(TCA_OVF_vect))
*/
ISR(TCA_CMP0_vect) {                                 //__vector_11
  AVR_TCA_PORT.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm; // Clear flag
  MB_REL_VPORT.DIR &= (~MB_REL_bm);                  // Set REL pin to input
  MB_REL_VPORT.OUT &= (~MB_REL_bm);                  // Set REL pin to low
  // VPORTA.OUT &= (~0x80);  // Turn off builtin LED
}
