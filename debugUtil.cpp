/**************************************************************************/
/*!
  @file     debugUtil.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the debugUtil class/functions and defines the DebugOut
  object

*/
/**************************************************************************/
#include "debugUtil.h"

#include "UImanager.h" // for the definition of u8g2log

debugUtil DebugOut;

/*!
        @brief  write one character to the alll the enabled debug streams

        Note: it is ublikely the return value is used: if there is an eror
   writing to the debug stream, there is usually very little that can be done by
   the caller. But the Print interface requires us to provide a return value and
   we try our best here

      @param  c the character to print

   @return return the result from the stream that has the best outcome (or zero
   if no output stream is enabled)
*/
size_t debugUtil::write(uint8_t c) {
  size_t b1 = 0, b2 = 0;
  if (use_serial)
    b1 = Serial.write(c);
  if (use_oled)
    b2 = u8g2log.write(c);
  return b1 > b2 ? b1 : b2;
}

/*!
        @brief  write a zero terminated string to the all the enabled debug
   streams

        Note: it is ublikely the return value is used: if there is an eror
   writing to the debug stream, there is usually very little that can be done by
   the caller. But the Print interface requires us to provide a return value and
   we try our best here

      @param  str the string to print

      @return return the result from the stream that has the best outcome (or
   zero if no output stream is enabled)
*/
size_t debugUtil::write(const char *str) {
  size_t s1 = 0, s2 = 0;
  if (use_serial)
    s1 = Serial.write(str);
  if (use_oled) {
    s2 = strlen(str);
    u8g2log.writeString(str);
  }
  return s1 > s2 ? s1 : s2;
}

/*!
        @brief  write a binary buffer to all the enabled debug streams

        Note: it is ublikely the return value is used: if there is an eror
   writing to the debug stream, there is usually very little that can be done by
   the caller. But the Print interface requires us to provide a return value and
   we try our best here

      @param buffer pointer to the data buffer
      @param size the number of bytes to write

      @return return the result from the stream that has the best outcome (or
   zero if no output stream is enabled)
*/
size_t debugUtil::write(const uint8_t *buffer, size_t size) {
  size_t b1 = 0, b2 = 0;
  if (use_serial)
    b1 = Serial.write(buffer, size);
  if (use_oled)
    b2 = u8g2log.write(buffer, size);
  return b1 > b2 ? b1 : b2;
}

/*!
        @brief  check if the stream is available for writing

        If Serial is used, the return value is the determined by
   Serial.availableForWrite(). Otherwise INT_MAX is returned (the Oled never
   blocks)

        Note: it is ublikely this is used for a debug stream, but it is provided
   just in case.

      @return the number of bytes that it is possible to write without blocking
   the write operation.
*/
int debugUtil::availableForWrite() {
  if (use_serial) {
    return Serial.availableForWrite();
  } else {
    return INT_MAX;
  }
}

/*!
        @brief  Waits for the transmission of outgoing debug data to complete.

        If Serial is used, Serial.flush() is called. If the Oled is used,
   u8g2log.flush() is called (albeit this should have no effect)
*/
void debugUtil::flush() {
  if (use_serial) {
    Serial.flush();
  }
  if (use_oled) {
    u8g2log.flush();
  }
}
