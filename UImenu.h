/**************************************************************************/
/*!
  @file     UImenu.h

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
// setClipWindow/setMaxClipWindow, drawFrame, drawRFrame, drawBox, drawRBox,
// drawButtonUTF8
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

    This class encapsulate the basic functionality that all menu items need to
   have.

    All changes to a menuItem are visible the next time the menu is displayed

    The assumption is that the menu is refreshed often enough that there is no
   need to update just after a change

    All text strings used in the menus must be null terminated, constant char
   arrays stored in flash memory. This is taken care automatically if the macros
   are used (which is the intended way of creating menus)

*/
/**************************************************************************/
class UImenuItem {
protected:
  friend class UIMenu; ///< we need UIMenu to be able to manage internal fields
  u8g2_uint_t height;  ///< the height of the menu item
public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored                                                 \
    "-Wunused-parameter" // A derived class may eturn a different valuse
                         // depending on being selected
  /*!
     @brief  Return the height of the menu
     @param selected true if the method should return the height when selected
     @return height of the item in pixels
  */
  virtual u8g2_uint_t getHeight(bool selected = true) {
    return height;
  }; ///< return the height of the menu item
#pragma GCC diagnostic pop

  /*!
     @brief  creator for the object
     @param height of the item in pixels
  */
  UImenuItem(u8g2_uint_t height) { this->height = height; };
  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                    bool selected);
  virtual bool handleUIEvent(K197UIeventsource eventSource,
                             K197UIeventType eventType);
  /*!
     @brief  query if the item can be selected
     @details a subclass may return false if the menu cannot be selected (e.g. a
     separator)
     @return true if the item can be selected
  */
  virtual bool selectable() { return true; };
  /*!
     @brief  this method is triggered any time the value of the item is changed
     or the item is asked to perform its default action
     @details the default implementation does nothing, but a subclass may
     implement any other appropriate behaviour such as closing a menu, etc.
     @param selected true if the method should return the height when selected
  */
  virtual void change(){};
};

/**************************************************************************/
/*!
    @brief  class implementing a separator menu item

    A separator is text that is shown in the menu but cannot be selected and has
   no other function

*/
/**************************************************************************/
class UIMenuSeparator : public UImenuItem {
protected:
  const __FlashStringHelper *text; ///< the text associated with the menu item

public:
  /*!
     @brief  creator for the object
     @param height of the item in pixels
     @param text the text to display
  */
  UIMenuSeparator(u8g2_uint_t height, const __FlashStringHelper *text)
      : UImenuItem(height) {
    this->text = text;
  };
  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                    bool selected);
  /*!
     @brief  query if the item can be selected
     @details a separator cannot be selected, so this method always returns
     false
     @return always return false
  */
  virtual bool selectable() { return false; };
};

/**************************************************************************/
/*!
    @brief  class implementing a separator button item

    A menu butto is a line of text that can be selected. when selected the "Ok"
   pushbutton triggers the "change() method. By default the change() method does
   not do anything, but a subclass can use it to implement any action (see also
   DEF_MENU_ACTION macro)

*/
/**************************************************************************/
class UIMenuButtonItem : public UImenuItem {
protected:
  const __FlashStringHelper *text; ///< the text associated with the menu item

public:
  /*!
     @brief  constructor of the object
     @param height the height of this item in pixels
     @param text the text displayed for this item
  */
  UIMenuButtonItem(u8g2_uint_t height, const __FlashStringHelper *text)
      : UImenuItem(height) {
    this->text = text;
  };
  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                    bool selected);
  virtual bool handleUIEvent(K197UIeventsource eventSource,
                             K197UIeventType eventType);
};

/**************************************************************************/
/*!
    @brief  base class for all windows in the UI
    
    Only the class UImanager
   (declared as friend) can create a working UIwindow.
*/
/**************************************************************************/
class UIwindow {
  protected:
      friend class UImanager;    ///< Windows are managed by UImanager 
      u8g2_uint_t width = 100;   ///< display width of the menu in pixels
      UIwindow *parent = NULL; ///< the parent menu, can only be set via openWindow().
      static UIwindow *currentWindow; ///< Keeps track of the current menu

  public:
      /*!
         @brief  constructor of the object
         @param width width of the menu window
         @param isRoot true if this is the root window (it will be set as default)
      */
      UIwindow(u8g2_uint_t width, bool isRoot = false) {
          this->width = width;  
          if (isRoot) currentWindow = this;
      };

      /*!
         @brief  get width
         @return width of the window
      */
      u8g2_uint_t getWidth() { return width;};
  
  #   pragma GCC diagnostic push
  #   pragma GCC diagnostic ignored                                                 \
        "-Wunused-parameter" // A derived class may eturn a different valuse
                         // depending on being selected
      virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y){};  ///< virtual function, see derived classes
      virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType){
          return false;  
      }; ///< virtual function, see derived classes
  #   pragma GCC diagnostic pop
  
      /*!
          @brief  open a child menu
          @details when a menu is opened in this way, this menu becomes the parent
          menu of the child, then the child becomes the current menu, that is the
          object returned by getcurrentWindow().
          @param child pointer to an object of type UImenu, it will become the
          current menu
      */
      void openWindow(UIwindow *child) {
          child->parent = this;
          currentWindow = child;
      };

      /*!
         @brief  close a menu
         @details when a menu is closed in this way, its parent menu becomes the
         current menu, that is the object returned by getcurrentWindow(). trying to
         close the root menu has no effect (the root menu has the parent set to
         NULL).
      */
      void closeWindow() {
          if (parent == NULL) return;
          currentWindow = parent;
          parent = NULL;
      };

      /*!
         @brief  get the currrent menu
        @return pointer to UImenu object representing the current menu
      */
      static UIwindow *getcurrentWindow() { return currentWindow; };
};

/**************************************************************************/
/*!
    @brief  class implementing a menu

    A menu is a window consisting in a collection of object of type MenuItem. Only the class UImanager
   (declared as friend) can create a working UImenu.

    Scrolling up and down is controlled by the up and down pushbutto source.

    A tree of submenus is suppported, as long as there is only one root.
    Moving within the hyerarchy is done using UIMenuActionOpen and
   UIMenuActionClose, alternatively methods openWindow() and closeMenu()

*/
/**************************************************************************/
class UImenu : public UIwindow {
protected:
  UImenuItem *
      *items; ///< point to an array, storing all menu items in this menu
  byte num_items = 0; ///< number of items in the menu

  byte firstVisibleItem = 0; ///< keep track of the first visible item
  byte selectedItem = 0;     ///< keep track of the selected item
  friend class UImanager;    ///< Menus are managed by UImanager 

  bool selectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1);
  void makeSelectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1);

public:
  /*!
     @brief  constructor of the object
     @param width width of the menu window
     @param isRoot true if this is the root menu (it will be set as default
     menu)
  */
  UImenu(u8g2_uint_t width, bool isRoot = false) : UIwindow(width, isRoot) {};
  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y);
  virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);

  /*!
     @brief  get the currently selected item
     @return Pointer to the currently selected item, of type UImenuItem
  */
  const UImenuItem *getSelectedItem() { return items[selectedItem]; };
  void selectFirstItem();

};

/**************************************************************************/
/*!
    @brief  class implementing a boolean input

    A boolean input menu item is a line of text with a checkbox on the right
   reflecting the boolean value of the item. When selected, the left and right
   pushbuttons toggle the value, triggering the change method. A subclass can
   override the method to implement any action (see DEF_MENU_BOOL_ACT macro).

    setValue and getValue are used to access the value programmatically (note
   that this does not trigger the change method).
*/
/**************************************************************************/
class MenuInputBool : public UIMenuButtonItem {
protected:
  static const u8g2_uint_t checkbox_size =
      10; ///< size of the checkbox in pixels
  static const u8g2_uint_t checkbox_margin =
      5; ///< smargin between checkbox and item display area

  bool value = false; ///< the value of this item

public:
  /*!
     @brief  constructor of the object
     @param height the height of this item in pixels
     @param text the text displayed for this item
  */
  MenuInputBool(u8g2_uint_t height, const __FlashStringHelper *text)
      : UIMenuButtonItem(height, text){};
  /*!
     @brief  set the value for this item
     @param newValue the new value that will be assigned to the item
  */
  virtual void setValue(bool newValue) { value = newValue; };
  /*!
     @brief  get the value for this item
     @return the value assigned to the item
  */
  virtual bool getValue() { return value; };

  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                    bool selected);
  virtual bool handleUIEvent(K197UIeventsource eventSource,
                             K197UIeventType eventType);
};

/**************************************************************************/
/*!
    @brief  class implementing a byte input

    A byte input menu item is a line of text with a number on the right
   reflecting the value of the item. When selected, When selected, the left and
   right pushbuttons decrement and increment the value respectly. Holding the
   key result in faster increments/decrements. Releasing the key triggers the
   "change" method. A subclass can override the method to implement any action
   (see DEF_MENU_BOOL_ACT macro).

    setValue and getValue are used to access the value programmatically (note
   that this does not trigger the change method).
*/
/**************************************************************************/
class MenuInputByte : public UIMenuButtonItem {
protected:
  static const u8g2_uint_t value_size =
      30; ///< the size used to display the value numerically
  static const u8g2_uint_t slide_xmargin =
      5; ///< empty space between the value area and the display area border
  static const u8g2_uint_t slide_ymargin0 =
      0; ///< empty space at the top of the value area
  static const u8g2_uint_t slide_ymargin1 =
      4; ///< empty space at the bottom of the value area

  byte value = 0; ///< the value of this item

public:
  /*!
     @brief  constructor for the object
     @param height the height of this item in pixels
     @param text the text displayed for this item
  */
  MenuInputByte(u8g2_uint_t height, const __FlashStringHelper *text)
      : UIMenuButtonItem(height, text){};
  /*!
     @brief  Return the height of the menu
     @details when not selected the height returned is the one set at
     construction. When selected the height doubles.
     @param selected true if the method should return the height when selected
     @return height of the item in pixels
  */
  virtual u8g2_uint_t getHeight(bool selected = true) {
    return selected ? 2 * height : height;
  };
  /*!
     @brief  set the value for this item
     @param newValue the new value that will be assigned to the item
  */
  virtual void setValue(byte newValue) { value = newValue; };
  /*!
     @brief  get the value for this item
     @return the value assigned to the item
  */
  virtual byte getValue() { return value; };

  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                    bool selected);
  virtual bool handleUIEvent(K197UIeventsource eventSource,
                             K197UIeventType eventType);
};
/**************************************************************************/
/*!
    @brief  class implementing options input

    An options input menu item allows selecting between pre-defined options.
   reflecting the value of the item. When selected, the left and
   right pushbuttons select the option. Holding the
   key result in faster increments/decrements. Releasing the key triggers the
   "change" method. A subclass can override the method to implement any action
   (see DEF_MENU_BOOL_ACT macro).

    setValue and getValue are used to access the value programmatically (note
   that this does not trigger the change method).
*/
/**************************************************************************/
class MenuInputOptions : public UIMenuButtonItem {
protected:
  const __FlashStringHelper **options; ///<array defining the options.  
  const byte options_size = 0; ///< the number of options in options[]
  byte value = 0; ///< the index corresponding to the selected item in options[]

public:
  /*!
     @brief  constructor for the object
     @param height the height of this item in pixels
     @param text the text displayed for this item
  */
  MenuInputOptions(u8g2_uint_t height, const __FlashStringHelper *text, const __FlashStringHelper *myoptions[], byte myoptions_size)
      : UIMenuButtonItem(height, text), options(myoptions), options_size(myoptions_size) {};
  /*!
     @brief  set the value for this item
     @param newValue the new value that will be assigned to the item
  */
  void setValue(byte newValue) { value = newValue; if (value>=options_size) value = 0;};
  /*!
     @brief  get the value for this item
     @return the value assigned to the item
  */
  byte getValue() { return value; };  

  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                    bool selected);
  virtual bool handleUIEvent(K197UIeventsource eventSource,
                             K197UIeventType eventType);
};
/**************************************************************************/
/*!
    @brief  class implementing a menu close action

    A menu close action is a line of text that can be selected. When selected,
   the Ok pushbutton will close the current menu and open the parent menu. If
   this is the root menu the action has no effect.
 */
/**************************************************************************/
class UIMenuActionClose : public UIMenuButtonItem {
public:
  /*!
     @brief  constructor for the object
     @param height the height of this item in pixels
     @param text the text displayed for this item
  */
  UIMenuActionClose(u8g2_uint_t height, const __FlashStringHelper *text)
      : UIMenuButtonItem(height, text){};
  /*!
     @brief  this method is triggered any time the value of the item is changed
     or the item is asked to perform its default action
     @details for this element this method closes the current menu
  */
  virtual void change() { UIwindow::getcurrentWindow()->closeWindow(); };
};

/**************************************************************************/
/*!
    @brief  class implementing a Menu open action

    A menu open action is a line of text that can be selected. When selected,
   the Ok pushbutton will close the current menu and open a child menu passed in
   the constructor.
 */
/**************************************************************************/
class UIMenuActionOpen : public UIMenuButtonItem {
private:
  UImenu *child; ///< pointer to a child menu object

public:
  /*!
     @brief  constructor for the object
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param menu pointer to UIMenu object that should be opened when this menu
     item is chosen
  */
  UIMenuActionOpen(u8g2_uint_t height, const __FlashStringHelper *text,
                   UImenu *menu)
      : UIMenuButtonItem(height, text) {
    child = menu;
  };
  /*!
     @brief  this method is triggered any time the value of the item is changed
     or the item is asked to perform its default action
     @details for this element this method asks the current menu to opens the
     child menu passed at contruction
  */
  virtual void change() { UIwindow::getcurrentWindow()->openWindow(child); };
};

/**************************************************************************/
/*!
    @brief  class implementing a message box

    A message box is a window that display a message and a "Ok" button
    
    pressing the button will dismiss the message

*/
/**************************************************************************/
class UImessageBox : public UIwindow {
protected:
  const __FlashStringHelper *text; ///< the message to be displayed
  static const u8g2_uint_t height=42;
  static const u8g2_uint_t text_offset_y=3;
  static const u8g2_uint_t btn_Offset=20;
  static const u8g2_uint_t btn_height=17;
  static const u8g2_uint_t btn_width=35;

public:
  /*!
     @brief  constructor of the object
     @param width width of the box
     @param text the message to be displayed (must fit on one line)
  */
  UImessageBox(u8g2_uint_t width, const __FlashStringHelper *text) : UIwindow(width, false) {
      this->text = text;
  };
  virtual void draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y);
  virtual bool handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType);
  void show() {
      UIwindow *current=getcurrentWindow();
      if (current!=NULL && current != this) current->openWindow(this);};
};

// *********************************************************************************
// *  The following macros do not add any extra functionality, but simplify
// *  the definition of menu classes and subclasses
// *  all macros need a terminating ';' character when they are used
// *  so that they look as instructions
// *********************************************************************************

/*!
     @def DEF_MENU_CLASS(class_name, instance_name, height, text)
     @brief  macro, used used to simplify class creation
     @param class_name name of the class. instance_name will be an object of
   class class_name
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
*/
#define DEF_MENU_CLASS(class_name, instance_name, height, text)                \
  const char __txt_##instance_name[] PROGMEM = text;                           \
  class_name instance_name(                                                    \
      height,                                                                  \
      reinterpret_cast<const __FlashStringHelper *>(__txt_##instance_name));

/*!
   \def DEF_MENU_SEPARATOR(instance_name, height, text)

     @brief  macro, used to simplify definition of menu separators
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
*/
#define DEF_MENU_SEPARATOR(instance_name, height, text)                        \
  DEF_MENU_CLASS(UIMenuSeparator, instance_name, height, text);

/*!
     @brief  macro, used to simplify definition of menu buttons
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
*/
#define DEF_MENU_BUTTON(instance_name, height, text)                           \
  DEF_MENU_CLASS(UIMenuButtonItem, instance_name, height, text)

/*!
     @brief  macro, used to simplify definition of menu bool inputs
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
*/
#define DEF_MENU_BOOL(instance_name, height, text)                             \
  DEF_MENU_CLASS(MenuInputBool, instance_name, height, text)

/*!
     @brief  macro, used to simplify definition of menu byte inputs
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     item is chosen
*/
#define DEF_MENU_BYTE(instance_name, height, text)                             \
  DEF_MENU_CLASS(MenuInputByte, instance_name, height, text)

/*!
     @brief  macro, used to simplify definition of menu options
     @details creates a constant char array in PROGMEM, name 
     and a constant symbol= value
     intended to use together with OPT() and DEF_MENU_OPTION_INPUT
     @param instance_name name of the option 
     @param symbol symbolic name for the option 
     @param value value corresponding to symbolic name (0 is the first option in DEF_MENU_OPTION_INPUT, 1 the secomnd and so on) 
     @param text the text displayed for this option
*/
#define DEF_MENU_OPTION(instance_name, symbol, value, text)           \
     static const uint8_t symbol = value;                   \
     const char instance_name[] PROGMEM = text;

/*!
     @brief  macro, used to simplify definition of menu options
     @details like DEF_MENU_OPTION but does not create a symbolic name
     intended to use together with OPT() and DEF_MENU_OPTION_INPUT
     to bind text to an existing enum
     The enum values should always be sequential, starting from 0
     Note that the macro does not actually bind anything, value is there only to document the binding 
     @param instance_name name of the option 
     @param value value corresponding to symbolic name  
     @param text the text displayed for this option
*/
#define BIND_MENU_OPTION(instance_name, value, text)           \
     const char instance_name[] PROGMEM = text;

/*!
     @brief  macro, used to simplify definition of menu options
     @details convert a const char array allocated in progmem to const __FlashStringHelper *
         intended to use together with OPT() and DEF_MENU_OPTION_INPUT
     @param PROGMEM_char_array name of the option 
*/
#define OPT(PROGMEM_char_array) \
    reinterpret_cast<const __FlashStringHelper *>(PROGMEM_char_array)

/*!
     @brief  macro, used to simplify definition of menu options inputs
     @details defines a menu item used to enter a number of pre-defined options
      Each option must be defined with a unique name in DEF_MENU_OPTION
      All the instance_names defined by DEF_MENU_OPTION must then be passed 
      as OPT(instance_name). The value of the first option must be 0, the second 1 and so on,
      otherwise getValue() will not return a value corresponding to the option
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param options_array a pointer to an array defining the options  
*/
#define DEF_MENU_OPTION_INPUT(instance_name, height, text, options...)         \
  const __FlashStringHelper *__optarr_##instance_name[] = { options };         \
  const char __txt_##instance_name[] PROGMEM = text;                           \
  MenuInputOptions instance_name(                                              \
      height,                                                                  \
      reinterpret_cast<const __FlashStringHelper *>(__txt_##instance_name),    \
      __optarr_##instance_name,                                                           \
      sizeof(__optarr_##instance_name)/sizeof(__optarr_##instance_name[0]));

/*!
     @brief  macro, used to simplify definition of menu options inputs
     @details like DEF_MENU_OPTION_INPUT, but it also redefines getValue and setValue
     so that they return an enum type rather than a byte
     @param enum_type enum that must be returned by setValue and getValue
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param options_array a pointer to an array defining the options  
*/
#define DEF_MENU_ENUM_INPUT(enum_type, instance_name, height, text, options...)\
  const __FlashStringHelper *__optarr_##instance_name[] = { options };         \
  class __class_##instance_name : public MenuInputOptions {                    \
  public:                                                                      \
    __class_##instance_name() : MenuInputOptions(height, F(text),              \
      __optarr_##instance_name,                                                \
      sizeof(__optarr_##instance_name)/sizeof(__optarr_##instance_name[0])){}; \
      void setValue(enum_type newValue) {                                      \
             MenuInputOptions::setValue((byte)newValue);};                     \
      enum_type getValue() { return  (enum_type) MenuInputOptions::getValue(); \
              };                                                               \
  } instance_name
      
/*!
     @brief  macro, used to simplify definition of menu close actions
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
*/
#define DEF_MENU_CLOSE(instance_name, height, text)                            \
  DEF_MENU_CLASS(UIMenuActionClose, instance_name, height, text)

/*!
     @brief  macro, used to simplify definition of a menu button to open a
   submenu
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param menuptr menu pointer to UIMenu object that should be opened when
   this menu item is chosen
*/
#define DEF_MENU_OPEN(instance_name, height, text, menuptr)                    \
  const char __txt_##instance_name[] PROGMEM = text;                           \
  UIMenuActionOpen instance_name(                                              \
      height,                                                                  \
      reinterpret_cast<const __FlashStringHelper *>(__txt_##instance_name),    \
      menuptr)

/*!
     @brief  macro, used used to simplify class creation
     @param parent_class name of the parent class. instance_name will be an
   object of a new class derived from parent_class
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param action_code code to be executed when this item is chosen
*/
#define DEF_MENU_ACTION_SUBCLASS(parent_class, instance_name, height, text,    \
                                 action_code)                                  \
  /*! define a new class derived from parent_class */                          \
  class __class_##instance_name : public parent_class {                        \
  public:                                                                      \
    __class_##instance_name() : parent_class(height, F(text)){};               \
    virtual void change(){action_code};                                        \
  } instance_name

/*!
     @brief  macro, used to simplify definition of a menu button to trigger an
   arbitrary action
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param action_code code to be executed when this item is chosen
*/
#define DEF_MENU_ACTION(instance_name, height, text, action_code)              \
  DEF_MENU_ACTION_SUBCLASS(UIMenuButtonItem, instance_name, height, text,      \
                           action_code)
/*!
     @brief  macro, used to simplify definition of a menu bool input that also
   triggers an arbitrary action when the value is changed via UI
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param action_code code to be executed when this item is chosen
*/
#define DEF_MENU_BOOL_ACT(instance_name, height, text, action_code)            \
  DEF_MENU_ACTION_SUBCLASS(MenuInputBool, instance_name, height, text,         \
                           action_code)
/*!
     @brief  macro, used to simplify definition of a menu byte input that also
   triggers an arbitrary action when the value is changed via UI
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param action_code code to be executed when this item is chosen
*/
#define DEF_MENU_BYTE_ACT(instance_name, height, text, action_code)            \
  DEF_MENU_ACTION_SUBCLASS(MenuInputByte, instance_name, height, text,         \
                           action_code)

/*!
     @brief  macro, used used to simplify class creation overriding the setValue(newValue) and getValue() methods
     @param parent_class name of the parent class. instance_name will be an
   object of a new class derived from parent_class
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param value_type the type returned by getValue() and passed in setValue(newValue)
     @param set_code code to be executed inside the setValue(newValue) method. 
     @param get_code code to be executed inside the getValue() method. It should end with a return statement.
*/
#define DEF_MENU_SETGET_SUBCLASS(parent_class, instance_name, height, text,    \
                                 value_type, set_code, get_code)               \
  /*! define a new class derived from parent_class */                          \
  class __class_##instance_name : public parent_class {                        \
  public:                                                                      \
    __class_##instance_name() : parent_class(height, F(text)){};               \
    virtual void setValue(value_type newValue){set_code};                      \
    virtual value_type getValue(){get_code};                                   \
  } instance_name

/*!
     @brief  macro, used to simplify definition of a menu bool input overriding the setValue(newValue) and getValue() methods
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param set_code code to be executed inside the setValue(newValue) method. 
     @param get_code code to be executed inside the getValue() method. It should end with a return statement.
*/
#define DEF_MENU_BOOL_SETGET(instance_name, height, text, set_code, get_code)  \
  DEF_MENU_SETGET_SUBCLASS(MenuInputBool, instance_name, height, text,         \
                           bool, set_code, get_code)
/*!
     @brief  macro, used to simplify definition of a menu byte input overriding the setValue(newValue) and getValue() methods
     @param instance_name name of the object instance defined by the macro
     @param height the height of this item in pixels
     @param text the text displayed for this item
     @param set_code code to be executed inside the setValue(newValue) method. 
     @param get_code code to be executed inside the getValue() method. It should end with a return statement.
*/
#define DEF_MENU_BYTE_SETGET(instance_name, height, text, set_code, get_code)  \
  DEF_MENU_SETGET_SUBCLASS(MenuInputByte, instance_name, height, text,         \
                           byte, set_code, get_code)
/*!
   \def DEF_MESSAGE_BOX(instance_name, width, text)

     @brief  macro, used to simplify definition of message boxes
     @param instance_name name of the object instance defined by the macro
     @param width the width of the window
     @param text the message to be displayed (make sure it fits in the window, no check)
*/
#define DEF_MESSAGE_BOX(instance_name, width, text)                        \
  DEF_MENU_CLASS(UImessageBox, instance_name, width, text); // reuse macro DEF_MENU_CLASS

#endif // UIMENU_H__
