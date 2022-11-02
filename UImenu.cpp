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

#define MENU_TEXT_OFFSET_X 5
#define MENU_TEXT_OFFSET_Y 2

UImenu UImainMenu(130);

//TODO: documentation

void UImenuItem::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected) {
  if (selected) {
    u8g2->setDrawColor(1);
    u8g2->setFontMode(0);
    u8g2->drawFrame(x, y, w, getHeight(selected));
  }
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool UImenuItem::handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType) {
  return false;
}
#pragma GCC diagnostic pop


void UIMenuButtonItem::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected) {
  UImenuItem::draw(u8g2, x, y, w, selected);
  u8g2->setFontMode(0);
  u8g2->setDrawColor(1);
  u8g2->setCursor(x+MENU_TEXT_OFFSET_X, y+MENU_TEXT_OFFSET_Y);
  u8g2->print( reinterpret_cast<const __FlashStringHelper *>(text) );
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool UIMenuButtonItem::handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType) {
  return false;
}
#pragma GCC diagnostic pop

void UImenu::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y) {
    u8g2->setFont(u8g2_font_6x12_mr);
    u8g2->setCursor(x, y);
    u8g2->setFontMode(0);
    u8g2->setDrawColor(0);
    u8g2_uint_t ymax=u8g2->getDisplayHeight();
    makeSelectedItemVisible(y, ymax);
    u8g2->drawBox(x, y, width, ymax-y);
    u8g2->setDrawColor(1);
    for (int i=firstVisibleItem; i<num_items; i++) {
        items[i]->draw(u8g2, x, y, width,  i==selectedItem ? true : false);
        y+=items[i]->getHeight(i==selectedItem ? true : false);
        if (y>ymax) break;
    }
    u8g2->setFontMode(0);
    u8g2->setDrawColor(1);
    u8g2->setFontPosTop();
    u8g2->setFontRefHeightExtendedText();
    u8g2->setFontDirection(0);
}

bool UImenu::selectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1) {
    if (selectedItem < firstVisibleItem) return false;
    if (selectedItem==firstVisibleItem) return true;
    for (int i=firstVisibleItem; i<num_items; i++) {
        y0+=items[i]->getHeight(i==selectedItem ? true : false);
        if (y0>y1) break;
        if (selectedItem==i) return true;
    }  
    return false;
}

void UImenu::makeSelectedItemVisible(u8g2_uint_t y0, u8g2_uint_t y1) {
  if (selectedItem < firstVisibleItem) {
    firstVisibleItem = selectedItem;
    return;
  }
  while(!selectedItemVisible(y0, y1)) {
     if (firstVisibleItem >= (num_items-1)) break;
     firstVisibleItem++;
  }
}

bool UImenu::handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType) {
  if (items[selectedItem]->handleUIEvent(eventSource, eventType)) return true;
  if (eventSource==K197key_DB && eventType==UIeventClick) { // down
     if (selectedItem  >= (num_items-1)) return true;
     selectedItem++;
  }
  if (eventSource==K197key_REL && eventType==UIeventClick) { // up
     if (selectedItem <= 0) return true;
     selectedItem--;
  }
  return false;
}

void MenuInputBool::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected) {
     UIMenuButtonItem::draw(u8g2, x, y, w, selected);
     u8g2->setDrawColor(1);
     u8g2->setFontMode(0);
     x=x+w-checkbox_size-checkbox_margin; 
     y=y+MENU_TEXT_OFFSET_Y;
     u8g2->drawFrame(x, y, checkbox_size, checkbox_size);
     if (value) {
           u8g2->drawLine(x, y, x+checkbox_size, y+checkbox_size);    
           u8g2->drawLine(x, y+checkbox_size, x+checkbox_size, y);    
     }
}

bool MenuInputBool::handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType) {
    if ( (eventSource==K197key_RCL || eventSource==K197key_STO) && eventType==UIeventClick) {
       setValue(!getValue()); 
       return true;  
    }
    return UIMenuButtonItem::handleUIEvent(eventSource, eventType);
}

void MenuInputByte::draw(U8G2 *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, bool selected) {
     UIMenuButtonItem::draw(u8g2, x, y, w, selected);
     u8g2->setDrawColor(1);
     u8g2->setFontMode(0);
     u8g2->setCursor(x+w-value_size, y+MENU_TEXT_OFFSET_Y);
     u8g2->print(value);
     if (selected) {
         x=x+slide_xmargin;
         y=y+height+slide_ymargin0;
         w=w-2*slide_xmargin;
         u8g2_uint_t h=height-slide_ymargin0-slide_ymargin1;
         if (value==255) {
             u8g2->drawBox(x, y, w, h);
         } else {
             u8g2->drawFrame(x, y, w, h);
             unsigned int m=int(value)*int(w)/int(255);
             u8g2->drawBox(x, y, (u8g2_uint_t) m, h);
         }
     }
}

byte calcIncrement(const byte val, const K197UIeventsource eventSource, const byte increment) {
   byte newval; 
   if(eventSource==K197key_RCL) { // Increment
       newval=val+increment; 
       if (newval<val) newval=255; 
   } else { // decrement
       newval=val-increment; 
       if (newval>val) newval=0; 
   }
   return newval;
}

bool MenuInputByte::handleUIEvent(K197UIeventsource eventSource, K197UIeventType eventType) {
    if ( (eventSource==K197key_RCL || eventSource==K197key_STO) ) { 
       switch (eventType) {
           case UIeventPress:     value=calcIncrement(value, eventSource,  1); break;
           case UIeventLongPress: value=calcIncrement(value, eventSource, 10); break;
           case UIeventHold:      value=calcIncrement(value, eventSource,  5); break;
           case UIeventRelease:   return false;
           default: break;
       }
       return true;  
    }
    return UIMenuButtonItem::handleUIEvent(eventSource, eventType);
}
