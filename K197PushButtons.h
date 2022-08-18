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

#define BUTTON_PRESSED_STATE LOW
#define BUTTON_IDLE_STATE HIGH

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
  typedef void (*buttonCallBack)(uint8_t buttonPinIn, uint8_t buttonEvent);
  void setup();

protected:
  void check(uint8_t i);
  static const unsigned long debounceDelay =
      50; // the debounce time; increase if the output flickers
  static const unsigned long longPressTime =
      800L; // long press event when pressed more than longPressTime ms
  static const unsigned long doubleClicktime =
      500L; // double click event when pressed within doubleClicktime ms from a
            // previous release

public: // Define Button events, switches and callbacks
  static const uint8_t eventClick = 0x01;       // detected after relase
  static const uint8_t eventDoubleClick = 0x02; // detected after relase
  static const uint8_t eventLongPress = 0x03;   // detected after relase
  static const uint8_t eventPress = 0x11;   // detected imediately when pressed
  static const uint8_t eventRelease = 0x12; // detected imediately when released

  boolean
  setCallback(uint8_t in_pin,
              buttonCallBack pinCallBack); // set it to NULL to remove call back

  void check(void);

  static void Serial_printEventName(uint8_t event);
};

#endif //__ABUTTON_H
