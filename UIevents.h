/**************************************************************************/
/*!
  @file     UIevents.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the UIevents enums

  The enums are needed to describe the UI events used by push buttons, UImanager, etc.
  display module

*/
/**************************************************************************/
#ifndef UIEVENTS_H__
#define UIEVENTS_H__

#define BUTTON_PRESSED_STATE LOW ///< logical level when pressed
#define BUTTON_IDLE_STATE HIGH   ///< logical level when not pressed

/**************************************************************************/
/*!
    @brief  Simple enum to identify UI events (e.g. pushbutton events)
*/
/**************************************************************************/

enum K197UIeventType {
  UIeventClick = 0x01,       ///< detected after relase
  UIeventDoubleClick = 0x02, ///< detected after relase
  UIeventLongPress = 0x03,   ///< detected after relase
  UIeventPress = 0x11, ///< detected imediately when pressed
  UIeventRelease = 0x12 ///< detected imediately when released
};

/**************************************************************************/
/*!
    @brief  Simple enum to identify UI event source
*/
/**************************************************************************/
enum K197UIeventsource {
   K197key_REL   = 0x01,  ///< REL key (Alt. functions: down, select) 
   K197key_DB   = 0x02,   ///< DB key  (Alt. functions: up,   mode)
   K197key_STO   = 0x03,  ///< STO key (Alt. functions: cancel, decrease, left)
   K197key_RCL   = 0x04,  ///< RCL key (Alt. functions: Ok, increase, right)
};

#endif //UIEVENTS_H__
