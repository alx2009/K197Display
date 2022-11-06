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
      virtual bool selectable() {return true;};
      virtual void change() {};
};

class UIMenuSeparator : public UImenuItem {
   protected:
      const __FlashStringHelper *text;
      
   public:
      UIMenuSeparator(u8g2_uint_t height, const __FlashStringHelper *text) : UImenuItem(height) {this->text = text;};
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool selectable() {return false;};
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

      UImenu *parent=NULL;
      static UImenu *currentMenu; ///< Keeps track of the current menu

   public:
      UImenu(u8g2_uint_t width, bool isRoot=false) {this->width=width; if(isRoot) currentMenu=this;};
      void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y);     
      bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
      const UImenuItem *getSelectedItem() {return items[selectedItem];};
      void selectFirstItem();
      void openMenu(UImenu *child) {child->parent=this; currentMenu=child;};
      void closeMenu() {if(parent==NULL) return; currentMenu=parent; parent=NULL; };
      static UImenu *getCurrentMenu() {return currentMenu;};
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
      static const u8g2_uint_t slide_xmargin = 5;
      static const u8g2_uint_t slide_ymargin0 = 0;
      static const u8g2_uint_t slide_ymargin1 = 4;
   
      byte value = 0;

   public:
      MenuInputByte(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      virtual u8g2_uint_t getHeight(bool selected=true) {return selected ? 2*height : height;}; 
      void setValue(byte newValue) {value = newValue;};
      byte getValue() {return value;};

      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

class UIMenuActionClose : public UIMenuButtonItem {
   public:
      UIMenuActionClose(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      virtual void change() {UImenu::getCurrentMenu()->closeMenu();};
};

class UIMenuActionOpen : public UIMenuButtonItem {
  private:
     UImenu *child;
   
   public:
      UIMenuActionOpen(u8g2_uint_t height, const __FlashStringHelper *text, UImenu *menu) : UIMenuButtonItem(height, text) {child=menu;};
      virtual void change() {UImenu::getCurrentMenu()->openMenu(child);};
};

// *********************************************************************************
// *  The following macros do not add any extra functionality, but simplify
// *  the definition of menu classes and subclasses
// *  all macros need a terminating ';' character when they are used
// *  so that they look as instructions
// *********************************************************************************

#define DEF_MENU_CLASS(class_name, instance_name, height, text)          \
const char __txt_##instance_name[] PROGMEM = text;                       \
class_name instance_name(height,                                         \
   reinterpret_cast<const __FlashStringHelper *>(__txt_##instance_name)) \

#define DEF_MENU_SEPARATOR(instance_name, height, text) DEF_MENU_CLASS(UIMenuSeparator, instance_name, height, text)
#define DEF_MENU_BUTTON(instance_name, height, text) DEF_MENU_CLASS(UIMenuButtonItem, instance_name, height, text)
#define DEF_MENU_BOOL(instance_name, height, text) DEF_MENU_CLASS(MenuInputBool, instance_name, height, text)
#define DEF_MENU_BYTE(instance_name, height, text) DEF_MENU_CLASS(MenuInputByte, instance_name, height, text)

#define DEF_MENU_CLOSE(instance_name, height, text) DEF_MENU_CLASS(UIMenuActionClose, instance_name, height, text)

#define DEF_MENU_OPEN(instance_name, height, text, menuptr)                       \
const char __txt_##instance_name[] PROGMEM = text;                                \
UIMenuActionOpen instance_name(height,                                            \
   reinterpret_cast<const __FlashStringHelper *>(__txt_##instance_name), menuptr)

#define DEF_MENU_ACTION_SUBCLASS(parent_class, instance_name, height, text, action_code) \
class __class_##instance_name : public parent_class {                                    \
  public:                                                                                \
     __class_##instance_name() : parent_class(height, F(text)) {};                       \
     virtual void change() {                                                             \
      action_code                                                                        \
     };                                                                                  \
} instance_name                                                                     

#define DEF_MENU_ACTION(instance_name, height, text, action_code)   DEF_MENU_ACTION_SUBCLASS(UIMenuButtonItem, instance_name, height, text, action_code)
#define DEF_MENU_BOOL_ACT(instance_name, height, text, action_code) DEF_MENU_ACTION_SUBCLASS(MenuInputBool, instance_name, height, text, action_code)
#define DEF_MENU_BYTE_ACT(instance_name, height, text, action_code) DEF_MENU_ACTION_SUBCLASS(MenuInputByte, instance_name, height, text, action_code)

#endif //UIMENU_H__
