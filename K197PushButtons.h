/**************************************************************************/
/*!
  @file     K197PushButtons.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the k197ButtonCluster class, responsible for managing the
  push button cluster on the front panel

*/
/**************************************************************************/
#ifndef __ABUTTON_H
#define __ABUTTON_H
#include <Arduino.h>

#include "UIevents.h"

/**************************************************************************/
/*!
    @brief  Simple class to handle a cluster of buttons.

    This class handles a cluster of buttons. The number and PIN mapping are
   hardwired in the implementation file.

    There are many libraries to handle buttons, but I never found one that
   handles debouncing reliably without resulting in a sluggish response... but
   feel free to clone the repository and replace with your favorite pushbutton
   library!
*/
/**************************************************************************/
class k197ButtonCluster {
public:
  k197ButtonCluster(){}; ///< default constructor
  typedef void (*buttonCallBack)(
      K197UIeventsource eventSource,
      K197UIeventType
          eventType); ///< define the type of the callback function
  void setup();

protected:
  void check(uint8_t i);
  void checkNew(uint8_t i, uint8_t btnow, unsigned long now);
  void checkPressed(uint8_t i, unsigned long now);

  static const unsigned long longPressTime =
      500000L; ///< long press event will be generated when pressed more than
            ///< longPressTime us
  static const unsigned long holdTime =
      200000L; ///< After a LongPress Hold events will be generated every holdTime 
               ///< in us while the button is still pressed
  static const unsigned long doubleClicktime =
      500000L; ///< double click event when pressed within doubleClicktime us from
            ///< a previous release
  void attachTimerInterrupts();

public:
  void setCallback(buttonCallBack pinCallBack);
  void checkNew();
  bool isPressed(K197UIeventsource eventSource);

  
  /*!
      @brief  check if two buttons are pressed simultaneously
      @param btn1 the event source value corresponding to the first button
      @param btn2 the event source value corresponding to the second button
      @return true if the two buttons are pressed simultaneously, false otherwise
  */
  bool isSimultaneousPress(K197UIeventsource btn1, K197UIeventsource btn2) {
      if (isPressed(btn1) && isPressed(btn2)) {
         return true;
      }
      return false;
  }

  static void DebugOut_printEventName(K197UIeventType event);
};

#endif //__ABUTTON_H
