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
    k197ButtonCluster();
    typedef void (*buttonCallBack)(
        uint8_t buttonPinIn,
        K197UIeventType buttonEvent); ///< define the type of the callback function
    void setup();

  protected:
    void check(uint8_t i);
    static const unsigned long debounceDelay =
        1; ///< the debounce time; decrease if the button is not responsive
            ///< enough, increase in case yoyu experience unintended double
            ///< presses
    static const unsigned long longPressTime =
        500L; ///< long press event will be generated when pressed more than
              ///< longPressTime ms
    static const unsigned long holdTime = 200L; ///< After a LongPress Hold events will be generated every holdTime while the butto is still pressed
    static const unsigned long doubleClicktime =
        500L; ///< double click event when pressed within doubleClicktime ms from
              ///< a previous release
    bool transparentMode = true;
    void attachInterrupts();
    void detachInterrupts();

public: 
  bool setCallback(uint8_t in_pin, buttonCallBack pinCallBack);

  void check(void);

  static void DebugOut_printEventName(K197UIeventType event);

  bool isTransparentMode() {return transparentMode;};
  void setTransparentMode(bool newMode);
};

#endif //__ABUTTON_H
