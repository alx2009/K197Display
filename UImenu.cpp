/**************************************************************************/
/*!
  @file     UImanager.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the UIenu classes, see UImenu.h for the class
  definition
*/
/**************************************************************************/

#include "UImenu.h"

#include "debugUtil.h"
#include "dxUtil.h"

#define MENU_TEXT_OFFSET_X                                                     \
  5 ///< the x offset where the text should be written (relative to the upper
    ///< left corner of a menu item)
#define MENU_TEXT_OFFSET_Y                                                     \
  2 ///< the y offset where the text should be written (relative to the upper
    ///< left corner of a menu item)

UIwindow *UIwindow::currentWindow = NULL; ///< keep track of current window

/*!
   @brief overloads standard strlen for PROGMEM strings passed as a
   __FlashStringHelper *
   @param ifsh the input string (must be stored in PROGMEM)
   @param max the maximu size that can be returned (default 100)
   @return the string lenght (without the terminating null character)
*/
size_t strlen(const __FlashStringHelper *ifsh, size_t max = 100) {
  PGM_P p = reinterpret_cast<PGM_P>(ifsh);
  size_t n;
  for (n = 0; n <= max; n++) {
    if (pgm_read_byte(p++) == 0)
      break;
  }
  return n;
}

/*!
   @brief draw a frame when the menu item is selected
   @param u8g2 a pointer to the u8g2 library object
   @param x the x coordinate of the top/left corner
   @param y the y coordinate of the top/left corner
   @param w the y width of the menu item
   @param selected true if the menu item is selected
*/
void UImenuItem::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w,
                      bool selected) {
  if (selected) {
    u8g2->setDrawColor(1);
    u8g2->setFontMode(0);
    u8g2->drawFrame(x, y, w, getHeight(selected));
  }
  CHECK_FREE_STACK();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
/*!
   @brief handle UI events
   @details this is the default function for the base class, it does nothing
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return true if the event has been entirely handled by this object
*/
bool UImenuItem::handleUIEvent(K197UIeventsource eventSource,
                               K197UIeventType eventType) {
  return false;
  CHECK_FREE_STACK();
}

#pragma GCC diagnostic pop
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
/*!
   @brief draw a line of text (used as a menu separator)
   @param u8g2 a poimnter to the u8g2 library object
   @param x the x coordinate of the top/left corner
   @param y the y coordinate of the top/left corner
   @param w the y width of the menu item
   @param selected true if the menu item is selected
*/
void UIMenuSeparator::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y,
                           u8g2_uint_t w, bool selected) {
  u8g2->setFontMode(0);
  u8g2->setDrawColor(1);
  u8g2->setCursor(x + MENU_TEXT_OFFSET_X, y + MENU_TEXT_OFFSET_Y);
  u8g2->print(reinterpret_cast<const __FlashStringHelper *>(text));
}
#pragma GCC diagnostic pop

/*!
   @brief draw a line of text (used as a menu item)
   @param u8g2 a poimnter to the u8g2 library object
   @param x the x coordinate of the top/left corner
   @param y the y coordinate of the top/left corner
   @param w the y width of the menu item
   @param selected true if the menu item is selected
*/
void UIMenuButtonItem::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y,
                            u8g2_uint_t w, bool selected) {
  UImenuItem::draw(u8g2, x, y, w, selected);
  u8g2->setFontMode(0);
  u8g2->setDrawColor(1);
  u8g2->setCursor(x + MENU_TEXT_OFFSET_X, y + MENU_TEXT_OFFSET_Y);
  u8g2->print(reinterpret_cast<const __FlashStringHelper *>(text));
}

/*!
   @brief handle UI events
   @details triggers the change() method on RCL key click
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return true if the event has been entirely handled by this object
*/
bool UIMenuButtonItem::handleUIEvent(K197UIeventsource eventSource,
                                     K197UIeventType eventType) {
  if (selectable()) {
    if ((eventSource == K197key_RCL) && (eventType == UIeventClick)) {
      change();
      return true;
    }
  }
  return false;
}

/*!
     @brief draw the menu
     @param u8g2 a poimnter to the u8g2 library object
     @param x the x coordinate of the top/left corner
     @param y the y coordinate of the top/left corner
*/
void UImenu::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y) {
  u8g2->setFont(u8g2_font_6x12_mr);
  u8g2->setCursor(x, y);
  u8g2->setFontMode(0);
  u8g2->setDrawColor(0);
  u8g2_uint_t ymax = u8g2->getDisplayHeight();
  makeSelectedItemVisible(y, ymax);
  u8g2->drawBox(x, y, width, ymax - y);
  u8g2->setDrawColor(1);
  for (int i = firstVisibleItem; i < num_items; i++) {
    items[i]->draw(u8g2, x, y, width, i == selectedItem ? true : false);
    y += items[i]->getHeight(i == selectedItem ? true : false);
    if (y > ymax)
      break;
  }
  u8g2->setFontMode(0);
  u8g2->setDrawColor(1);
  u8g2->setFontPosTop();
  u8g2->setFontRefHeightExtendedText();
  u8g2->setFontDirection(0);
}

/*!
     @brief check if the selected item is visible
     @param y0 the y coordinate of the top of the menu
     @param y1 the y coordinate of the menu bottom
     @return true if the selected menu item is visible given the
   firstVisibleItem and the menu window dimension specified by y0 and y1
*/
bool UImenu::selectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1) {
  if (selectedItem < firstVisibleItem)
    return false;
  if (selectedItem == firstVisibleItem)
    return true;
  for (int i = firstVisibleItem; i < num_items; i++) {
    y0 += items[i]->getHeight(i == selectedItem ? true : false);
    if (y0 > y1)
      break;
    if (selectedItem == i)
      return true;
  }
  return false;
}

/*!
     @brief scroll the menu until the current menu item is visible
     @param y0 the y coordinate of the top of the menu
     @param y1 the y coordinate of the menu bottom

*/
void UImenu::makeSelectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1) {
  if (selectedItem < firstVisibleItem) {
    firstVisibleItem = selectedItem;
    return;
  }
  while (!selectedItemVisible(y0, y1)) {
    if (firstVisibleItem >= (num_items - 1))
      break;
    firstVisibleItem++;
  }
}

/*!
     @brief select the first selectable item in a menu
*/
void UImenu::selectFirstItem() {
  for (int i = 0; i < num_items; i++) {
    if (items[i]->selectable()) {
      selectedItem = i;
      return;
    }
  }
}

/*!
     @brief handle UI events
     @details scroll the menu up or down when key REL or DB is pressed
     @param eventSource the source of the event (see K197UIeventsource)
     @param eventType the type of event (see K197UIeventType)
     @return true if the event has been entirely handled by this object
*/
bool UImenu::handleUIEvent(K197UIeventsource eventSource,
                           K197UIeventType eventType) {
  if (items[selectedItem]->handleUIEvent(eventSource, eventType))
    return true;
  if (eventSource == K197key_DB &&
      (eventType == UIeventPress || eventType == UIeventLongPress ||
       eventType == UIeventHold)) { // down
    if (selectedItem >= (num_items - 1))
      return false;
    for (byte i = selectedItem + 1; i < num_items; i++) {
      if (items[i]->selectable()) {
        selectedItem = i;
        return true;
      }
    }
  }
  // In the following we use click events to be able to assign the LongPress to
  // other events
  if (eventSource == K197key_REL && eventType == UIeventClick) { // up
    if (selectedItem == 0)
      return false;
    for (byte i = selectedItem; i > 0; i--) {
      if (items[i - 1]->selectable()) {
        selectedItem = i - 1;
        return true;
      }
    }
    // If we end up here, we could not find any selectable item
    if (firstVisibleItem > 0)
      firstVisibleItem--;
    return true;
  } else if (eventSource == K197key_REL &&
             eventType == UIeventDoubleClick) { // top
    firstVisibleItem = 0;
    selectFirstItem();
    return true;
  }
  return false;
}

/*!
     @brief draw the menu
     @details invoke UIMenuButtonItem::draw, then add a checkbox on the right of
   the allocated space
     @param u8g2 a poimnter to the u8g2 library object
     @param x the x coordinate of the top/left corner
     @param y the y coordinate of the top/left corner
     @param w the y width of the menu item
     @param selected true if the menu item is selected
*/
void MenuInputBool::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y,
                         u8g2_uint_t w, bool selected) {
  UIMenuButtonItem::draw(u8g2, x, y, w, selected);
  u8g2->setDrawColor(1);
  u8g2->setFontMode(0);
  x = x + w - checkbox_size - checkbox_margin;
  y = y + MENU_TEXT_OFFSET_Y;
  u8g2->drawFrame(x, y, checkbox_size, checkbox_size);
  if (getValue()) {
    u8g2->drawLine(x, y, x + checkbox_size, y + checkbox_size);
    u8g2->drawLine(x, y + checkbox_size, x + checkbox_size, y);
  }
}

/*!
   @brief handle UI events
   @details toggles the value when RCL or KEY is clicked, then invoke the change
   method
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return true if the event has been entirely handled by this object
*/
bool MenuInputBool::handleUIEvent(K197UIeventsource eventSource,
                                  K197UIeventType eventType) {
  if ((eventSource == K197key_RCL || eventSource == K197key_STO) &&
      eventType == UIeventClick) {
    setValue(!getValue());
    change();
    return true;
  }
  return UIMenuButtonItem::handleUIEvent(eventSource, eventType);
}

/*!
     @brief draw the menu
     @details invoke UIMenuButtonItem::draw, then add the value on the right.
     If selected, draw a progress bar set to the value
     @param u8g2 a poimnter to the u8g2 library object
     @param x the x coordinate of the top/left corner
     @param y the y coordinate of the top/left corner
     @param w the y width of the menu item
     @param selected true if the menu item is selected
*/
void MenuInputByte::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y,
                         u8g2_uint_t w, bool selected) {
  UIMenuButtonItem::draw(u8g2, x, y, w, selected);
  u8g2->setDrawColor(1);
  u8g2->setFontMode(0);
  u8g2->setCursor(x + w - value_size, y + MENU_TEXT_OFFSET_Y);
  byte draw_value = edit_mode ? value : getValue();
  u8g2->print(draw_value);
  if (selected) {
    x = x + slide_xmargin;
    y = y + height + slide_ymargin0;
    w = w - 2 * slide_xmargin;
    u8g2_uint_t h = height - slide_ymargin0 - slide_ymargin1;
    if (draw_value == 255) {
      u8g2->drawBox(x, y, w, h);
    } else {
      u8g2->drawFrame(x, y, w, h);
      unsigned int m = int(draw_value) * int(w) / int(255);
      u8g2->drawBox(x, y, (u8g2_uint_t)m, h);
    }
  }
}

/*!
     @brief utility function used to simplify MenuInputByte::handleUIEvent()
   code
     @param val the current value of the MenuInputByte
     @param eventSource (used to determine the sign of the value change)
     @param increment the increment
     @return the new value
*/
static byte calcIncrement(const byte val, const K197UIeventsource eventSource,
                          const byte increment) {
  byte newval;
  if (eventSource == K197key_RCL) { // Increment
    newval = val + increment;
    if (newval < val)
      newval = 255;
  } else { // decrement
    newval = val - increment;
    if (newval > val)
      newval = 0;
  }
  //DebugOut.print(F("New inc: "));
  //DebugOut.println(newval);
  return newval;
}

/*!
     @brief utility function used to simplify handleUIEvent()
     @details: note that this function wraps around if max<255 (this is
   intended)
     @param val the current value of the MenuInputByte
     @param eventSource (used to determine the sign of the value change)
     @param increment the increment
     @param max the maximum allowed value
     @return the new value
*/
static byte calcIncrementExt(const byte val,
                             const K197UIeventsource eventSource,
                             const byte increment, const byte max) {
  byte newval;
  if (eventSource == K197key_RCL) { // Increment
    newval = val + increment;
    if (newval < val)
      newval = 0;
  } else { // decrement
    newval = val - increment;
    if (newval > val)
      newval = max;
  }
  return newval;
}

/*!
   @brief handle UI events
   @details increase/decrease the value when STO/RCL pressed (repeatedly when
   hold), then invoke setValue() and change() at release
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return true if the event has been entirely handled by this object
*/
bool MenuInputByte::handleUIEvent(K197UIeventsource eventSource,
                                  K197UIeventType eventType) {
  if ((eventSource == K197key_RCL || eventSource == K197key_STO)) {
    switch (eventType) {
    case UIeventPress:
      value = calcIncrement(getValue(), eventSource, 1);
      edit_mode = true;
      break;
    case UIeventLongPress:
      value = calcIncrement(value, eventSource, 10);
      break;
    case UIeventHold:
      value = calcIncrement(value, eventSource, 5);
      break;
    case UIeventRelease:
      setValue(value);
      change();
      edit_mode = false;
      break;
    default:
      break;
    }
    return true;
  }
  return UIMenuButtonItem::handleUIEvent(eventSource, eventType);
}

/*!
     @brief draw the menu
     @details invoke UIMenuButtonItem::draw, then print the text and the
   selected option to the right.
     @param u8g2 a poimnter to the u8g2 library object
     @param x the x coordinate of the top/left corner
     @param y the y coordinate of the top/left corner
     @param w the y width of the menu item
     @param selected true if the menu item is selected
*/
void MenuInputOptions::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y,
                            u8g2_uint_t w, bool selected) {
  UIMenuButtonItem::draw(u8g2, x, y, w, selected);
  if (value < options_size) {
    u8g2->print(F(": < "));
    u8g2->print(options[value]);
    u8g2->print(F(" >"));
  }
}

/*!
   @brief handle UI events
   @details increase/decrease the value when STO/RCL pressed (repeatedly when
   hold), then invoke change at release
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return true if the event has been entirely handled by this object
*/
bool MenuInputOptions::handleUIEvent(K197UIeventsource eventSource,
                                     K197UIeventType eventType) {
  if ((eventSource == K197key_RCL || eventSource == K197key_STO)) {
    switch (eventType) {
    case UIeventPress:
      setValue(calcIncrementExt(value, eventSource, 1, options_size - 1));
      break;
    case UIeventLongPress:
      setValue(calcIncrementExt(value, eventSource, 1, options_size - 1));
      break;
    case UIeventHold:
      setValue(calcIncrementExt(value, eventSource, 2, options_size - 1));
      break;
    case UIeventRelease:
      change();
      break;
    default:
      break;
    }
    return true;
  }
  return UIMenuButtonItem::handleUIEvent(eventSource, eventType);
}

/*!
   @brief draw the message box
   @details: note that differently from menu items, a message box is drawn above
   its parent (tipically the menu that includes the action that resulted in the
   box being displayed)
   @param u8g2 a poimnter to the u8g2 library object
   @param x the x coordinate of the top/left corner
   @param y the y coordinate of the top/left corner
*/
void UImessageBox::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y) {
  // draw parent and acquire its height
  u8g2_uint_t parent_width;
  if (parent != NULL) {
    parent->draw(u8g2, x, y);
    parent_width = parent->getWidth();
  } else {
    parent_width = u8g2->getDisplayWidth();
  }
  // Calc x and y for this window
  x = x + (parent_width - width) / 2;
  y = (u8g2->getDisplayHeight() + y - height) / 2;

  // Clear the window area
  u8g2->setFont(u8g2_font_6x12_mr);
  u8g2->setCursor(x, y);
  u8g2->setFontMode(0);
  u8g2->setDrawColor(0);
  u8g2->drawBox(x, y, width, height);
  u8g2->setDrawColor(1);

  // Draw window frames
  u8g2->setDrawColor(1);
  u8g2->setFontMode(0);
  u8g2->drawFrame(x, y, width, height);
  u8g2->drawFrame(x + (width - btn_width) / 2, y + btn_Offset, btn_width,
                  btn_height);

  // Set required text attributes
  u8g2->setFontPosTop();
  u8g2->setFontRefHeightExtendedText();
  u8g2->setFontDirection(0);

  // print message
  u8g2_uint_t text_width =
      u8g2->getMaxCharWidth() * strlen(text); // monospace font
  if (text_width > width)
    text_width = width;
  u8g2->setCursor(x + (width - text_width) / 2, y + text_offset_y);
  u8g2->print(text);

  // print "Ok" text
  u8g2->setCursor(x + width / 2 - u8g2->getMaxCharWidth(),
                  y + btn_Offset + (btn_height - u8g2->getMaxCharHeight()) / 2);
  u8g2->print(F("Ok"));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored                                                 \
    "-Wunused-parameter" // A derived class may use different parameters
/*!
   @brief handle UI events for a message box
   @details clicking any button closes the message box. all other evdents are
   ignored (in other graphic environments we could say the message box is
   "modal").
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return always return true
*/
bool UImessageBox::handleUIEvent(K197UIeventsource eventSource,
                                 K197UIeventType eventType) {
  // Click any button to dismiss this message box. Any other event is ignored.
  if (eventType == UIeventClick) {
    closeWindow();
  }
  return true;
}
#pragma GCC diagnostic pop
