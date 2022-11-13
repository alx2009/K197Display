/**************************************************************************/
/*!
  @file     UIevents.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the UIevents enums

  The enums are needed to describe the UI events used by push buttons,
  UImanager, etc. display module

*/
/**************************************************************************/
#ifndef UIEVENTS_H__
#define UIEVENTS_H__

#include "pinout.h"

#define BUTTON_PRESSED_STATE LOW ///< logical level when pressed
#define BUTTON_IDLE_STATE HIGH   ///< logical level when not pressed

/**************************************************************************/
/*!
    @brief  Simple enum to identify UI events (e.g. pushbutton events)

    @details there are two main sequence of events:
    - Press-Release-Click
    - Press-Release-LongPress-Release-Longclick (if hold for a longer time)

    A second click event close to the first will become a double click
    Hold is generated after LongPress and before Release (if pressed for a
   sufficiently long time)
*/
/**************************************************************************/

enum K197UIeventType {
  UIeventClick = 0x01,       ///< detected after relase
  UIeventDoubleClick = 0x02, ///< detected after relase
  UIeventLongClick = 0x03,   ///< detected after relase
  UIeventPress = 0x11,       ///< detected imediately when pressed
  UIeventLongPress = 0x12,   ///< detected while still pressed
  UIeventHold = 0x13,        ///< detected while still pressed
  UIeventRelease = 0x14      ///< detected imediately when released
};

/**************************************************************************/
/*!
    @brief  Simple enum to identify UI event source
    @details the values of the enum are the same as the corresponding pins,
    so that we can cast a pin number to the enum
*/
/**************************************************************************/
enum K197UIeventsource {
  K197key_REL = UI_REL, ///< REL key (Alt. functions: up)
  K197key_DB = UI_DB,  ///< DB key  (Alt. functions: down,   mode)
  K197key_STO =
      UI_STO, ///< STO key (Alt. functions: clear, cancel, decrease, left)
  K197key_RCL = UI_RCL, ///< RCL key (Alt. functions: set, Ok, increase, right)
};

#endif // UIEVENTS_H__
