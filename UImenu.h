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

class UImanager; ///< forward class definition of UImanager

/**************************************************************************/
/*!
    @brief  base class for all menu items

    This class encapsulate the basic functionality that all menu items need to have.

    All changes to a menuItem are visible the next time the menu is displayed

    The assumption is that the menu is refreshed often enough that there is no need to update just after a change

    All text strings used in the menus must be null terminated, constant char arrays stored in flash memory. This is taken care automatically if the macros are used (which is the intended way of creating menus)
    
*/
/**************************************************************************/
class UImenuItem {
   protected:
      friend class UIMenu; ///< we need UIMenu to be able to manage internal fields
      u8g2_uint_t height;  ///< the height of the menu item
   public:
#     pragma GCC diagnostic push
#     pragma GCC diagnostic ignored "-Wunused-parameter"  // A derived class may eturn a different valuse depending on being selected
      /*!
         @brief  Return the height of the menu 
         @param selected true if the method should return the height when selected
         @return height of the item in pixels
      */
      virtual u8g2_uint_t getHeight(bool selected=true) {return height;};  ///< return the height of the menu item
#     pragma GCC diagnostic pop
      
      /*!
         @brief  set the height of a normal menu item 
         @details a subclass may apply a scaling factor depending on the specific type of item
         @param selected true if the method should return the height when selected
         @return height of the item in pixels
      */
      UImenuItem(u8g2_uint_t height) {this->height = height;};
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
      /*!
         @brief  query if the item can be selected 
         @details a subclass may return false if the menu cannot be selected (e.g. a separator)
         @return true if the item can be selected
      */
      virtual bool selectable() {return true;};
      /*!
         @brief  this method is triggered any time the value of the item is changed or the item is asked to perform its default action
         @details the default implementation does nothing, but a subclass may implement any other appropriate behaviour such as closing a menu, etc.
         @param selected true if the method should return the height when selected
      */
      virtual void change() {};
};

/**************************************************************************/
/*!
    @brief  class implementing a separator menu item

    A separator is text that is shown in the menu but cannot be selected and has no other function 

*/
/**************************************************************************/
class UIMenuSeparator : public UImenuItem {
   protected:
      const __FlashStringHelper *text; ///< the text associated with the menu item
      
   public:
      UIMenuSeparator(u8g2_uint_t height, const __FlashStringHelper *text) : UImenuItem(height) {this->text = text;};
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      /*!
         @brief  query if the item can be selected 
         @details a separator cannot be selected, so this method always returns false
         @return always return false
      */
      virtual bool selectable() {return false;};
};

/**************************************************************************/
/*!
    @brief  class implementing a separator button item

    A menu butto is a line of text that can be selected. when selected the "Ok" pushbutton triggers the "change() method.
    By default the change() method does not do anything, but a subclass can use it to implement any action (see also DEF_MENU_ACTION macro) 

*/
/**************************************************************************/
class UIMenuButtonItem : public UImenuItem {
   protected:
      const __FlashStringHelper *text; ///< the text associated with the menu item
      
   public:
      UIMenuButtonItem(u8g2_uint_t height, const __FlashStringHelper *text) : UImenuItem(height) {this->text = text;};
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

/**************************************************************************/
/*!
    @brief  class implementing a menu

    A menu is a collection of object of type MenuItem. Only the class UImanager (declared as friend) can create a working UImenu.

    Scrolling up and down is controlled by the up and down pushbutto source.
    
    A tree of submenus is suppported, as long as there is only one root. 
    Moving within the hyerarchy is done using UIMenuActionOpen and UIMenuActionClose, alternatively methods openMenu() and closeMenu()

*/
/**************************************************************************/
class UImenu {
   protected:
      UImenuItem **items;  ///< point to an array, storing all menu items in this menu
      byte num_items=0;    ///< number of items in the menu
      
      byte firstVisibleItem=0; ///< keep track of the first visible item 
      byte selectedItem=0;     ///< keep track of the selected item 
      u8g2_uint_t width=100;   ///< display width of the menu in pixels 
      friend class UImanager;  ///< Menus are managed by UImanager and no other object 

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
      /*!
         @brief  get the currrent menu
         @return pointer to UImenu object representing the current menu
      */
      static UImenu *getCurrentMenu() {return currentMenu;};
};

/**************************************************************************/
/*!
    @brief  class implementing a boolean input 

    A boolean input menu item is a line of text with a checkbox on the right reflecting the boolean value of the item. 
    When selected, the left and right pushbuttons toggle the value, triggering the change method. 
    A subclass can override the method to implement any action (see DEF_MENU_BOOL_ACT macro).
    
    setValue and getValue are used to access the value programmatically (note that this does not trigger the change method). 
*/
/**************************************************************************/
class MenuInputBool : public UIMenuButtonItem {
   protected:
      static const u8g2_uint_t checkbox_size = 10;  ///< size of the checkbox in pixels
      static const u8g2_uint_t checkbox_margin = 5; ///< smargin between checkbox and item display area
   
      bool value = false;  ///< the value of this item

   public:
      /*!
         @brief  constructor of the object
         @param height the height of this item in pixels
         @param text the text displayed for this item
      */
      MenuInputBool(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      /*!
         @brief  set the value for this item
         @param newValue the new value that will be assigned to the item
      */
      void setValue(bool newValue) {value = newValue;};
      /*!
         @brief  get the value for this item
         @return the value assigned to the item
      */
      bool getValue() {return value;};

      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

/**************************************************************************/
/*!
    @brief  class implementing a byte input 

    A byte input menu item is a line of text with a number on the right reflecting the value of the item. 
    When selected, 
    When selected, the left and right pushbuttons decrement and increment the value respectly. Holding the key result in faster increments/decrements. 
    Relesing the key triggers the "change" method. 
    A subclass can override the method to implement any action (see DEF_MENU_BOOL_ACT macro).
    
    setValue and getValue are used to access the value programmatically (note that this does not trigger the change method). 
*/
/**************************************************************************/
class MenuInputByte : public UIMenuButtonItem {
   protected:
      static const u8g2_uint_t value_size = 30;   ///< the size used to display the value numerically
      static const u8g2_uint_t slide_xmargin = 5; ///< empty space between the value area and the display area border
      static const u8g2_uint_t slide_ymargin0 = 0; ///< empty space at the top of the value area
      static const u8g2_uint_t slide_ymargin1 = 4; ///< empty space at the bottom of the value area
   
      byte value = 0; ///< the value of this item

   public:
      /*!
         @brief  constructor for the object
         @param height the height of this item in pixels
         @param text the text displayed for this item
      */
      MenuInputByte(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      /*!
         @brief  Return the height of the menu 
         @details when not selected the height returned is the one set at construction. When selected the height doubles.
         @param selected true if the method should return the height when selected
         @return height of the item in pixels
      */
      virtual u8g2_uint_t getHeight(bool selected=true) {return selected ? 2*height : height;}; 
      /*!
         @brief  set the value for this item
         @param newValue the new value that will be assigned to the item
      */
      void setValue(byte newValue) {value = newValue;};
      /*!
         @brief  get the value for this item
         @return the value assigned to the item
      */
      byte getValue() {return value;};

      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected);
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
};

/**************************************************************************/
/*!
    @brief  class implementing a menu close action 

    A menu close action is a line of text that can be selected. When selected, the Ok pushbutton will close the current menu and open the parent menu.
    If this is the root menu the action has no effect.
 */
/**************************************************************************/
class UIMenuActionClose : public UIMenuButtonItem {
   public:
      /*!
         @brief  constructor for the object
         @param height the height of this item in pixels
         @param text the text displayed for this item
      */
      UIMenuActionClose(u8g2_uint_t height, const __FlashStringHelper *text) : UIMenuButtonItem(height, text) {};
      /*!
         @brief  this method is triggered any time the value of the item is changed or the item is asked to perform its default action
         @details for this element this method closes the current menu 
      */
      virtual void change() {UImenu::getCurrentMenu()->closeMenu();};
};

/**************************************************************************/
/*!
    @brief  class implementing a Menu open action 

    A menu open action is a line of text that can be selected. When selected, the Ok pushbutton will close the current menu and open a child menu passed in the constructor.
 */
/**************************************************************************/
class UIMenuActionOpen : public UIMenuButtonItem {
  private:
     UImenu *child;  ///< pointer to a child menu object
   
   public:
      /*!
         @brief  constructor for the object
         @param height the height of this item in pixels
         @param text the text displayed for this item
         @param menu pointer to UIMenu object that should be opened when this menu item is chosen
      */
      UIMenuActionOpen(u8g2_uint_t height, const __FlashStringHelper *text, UImenu *menu) : UIMenuButtonItem(height, text) {child=menu;};
      /*!
         @brief  this method is triggered any time the value of the item is changed or the item is asked to perform its default action
         @details for this element this method asks the current menu to opens the child menu passed at contruction 
      */
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
