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

UImenu *UImenu::currentMenu = NULL;

/*!
   @brief draw a frame when the menu item is selected
   @param u8g2 a poimnter to the u8g2 library object
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
  dxUtil.checkFreeStack();
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
  dxUtil.checkFreeStack();
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
  if (value) {
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
  u8g2->print(value);
  if (selected) {
    x = x + slide_xmargin;
    y = y + height + slide_ymargin0;
    w = w - 2 * slide_xmargin;
    u8g2_uint_t h = height - slide_ymargin0 - slide_ymargin1;
    if (value == 255) {
      u8g2->drawBox(x, y, w, h);
    } else {
      u8g2->drawFrame(x, y, w, h);
      unsigned int m = int(value) * int(w) / int(255);
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
byte calcIncrement(const byte val, const K197UIeventsource eventSource,
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
  return newval;
}

/*!
   @brief handle UI events
   @details increase/decrease the value when STO/RCL pressed (repeatedly when
   hold), then invoke change at release
   @param eventSource the source of the event (see K197UIeventsource)
   @param eventType the type of event (see K197UIeventType)
   @return true if the event has been entirely handled by this object
*/
bool MenuInputByte::handleUIEvent(K197UIeventsource eventSource,
                                  K197UIeventType eventType) {
  if ((eventSource == K197key_RCL || eventSource == K197key_STO)) {
    switch (eventType) {
    case UIeventPress:
      value = calcIncrement(value, eventSource, 1);
      break;
    case UIeventLongPress:
      value = calcIncrement(value, eventSource, 10);
      break;
    case UIeventHold:
      value = calcIncrement(value, eventSource, 5);
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
