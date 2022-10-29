/**************************************************************************/
/*!
  @file     UImenu.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the UImenu classes

  The classes are responsible for managing the menus for the application
*/
/**************************************************************************/

// USeful functions when drawing menus:
// setClipWindow/setMaxClipWindow, drawFrame, drawRFrame, drawBox, drawRBox, drawButtonUTF8
// 
//

#ifndef UIMENU_H__
#define UIMENU_H__

#include "UIevents.h"
#include <U8g2lib.h>

class UImanager;

class UImenuItem {
   protected:
      friend class UIMenu;
   public:
      u8g2_uint_t height;
      UImenuItem(u8g2_uint_t height) {this->height = height;};
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

class UIMenuButtonItem : public UImenuItem {
   protected:
      const __FlashStringHelper *text;
      
   public:
      UIMenuButtonItem(u8g2_uint_t height, const __FlashStringHelper *text) : UImenuItem(height) {this->text = text;};
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

class UImenu {
   protected:
      UImenuItem **items;
      byte num_items=0;
      
      byte firstVisibleItem=0;
      byte selectedItem=0;
      u8g2_uint_t width=100;
      friend class UImanager;

      bool selectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1);
      void makeSelectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1);

   public:
      UImenu(u8g2_uint_t width) {this->width=width;};
      void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y);     
      bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
      const UImenuItem *getSelectedItem() {return items[selectedItem];};
};

extern UImenu
    UImainMenu; ///< this is the predefined oubject that is used with pribnt(),
                ///< etc. (similar to how Serial is used for debug output)


#endif //UIMENU_H__
