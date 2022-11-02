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
      u8g2_uint_t height;
   public:
#     pragma GCC diagnostic push
#     pragma GCC diagnostic ignored "-Wunused-parameter"  // A derived class may eturn a different valuse depending on being selected
      virtual u8g2_uint_t getHeight(bool selected=true) {return height;}; 
#     pragma GCC diagnostic pop
      
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

class MenuInputBool : public UIMenuButtonItem {
   protected:
      static const u8g2_uint_t checkbox_size = 10;
      static const u8g2_uint_t checkbox_margin = 5;
   
      bool value = false;

   public:
      MenuInputBool(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      void setValue(bool newValue) {value = newValue;};
      bool getValue() {return value;};

      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

class MenuInputByte : public UIMenuButtonItem {
   protected:
      static const u8g2_uint_t value_size = 30;
      static const u8g2_uint_t slide_margin = 5;
   
      byte value = 0;

   public:
      MenuInputByte(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      virtual u8g2_uint_t getHeight(bool selected=true) {return selected ? 2*height : height;}; 
      void setValue(byte newValue) {value = newValue;};
      byte getValue() {return value;};

      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

extern UImenu
    UImainMenu; ///< this is the predefined oubject that is used with print(),
                ///< etc. (similar to how Serial is used for debug output)


#endif //UIMENU_H__
