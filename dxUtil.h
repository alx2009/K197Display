/**************************************************************************/
/*!
  @file     dxUtil.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the dxUtil class

  This class encapsulate a number of utility functions for the AVR DB series
  and/or dxCore The intention is to have one place to consolidate functions
  providing information that may be useful to debug the application

*/
/**************************************************************************/
#ifndef DXUTIL_H__
#define DXUTIL_H__
#include <Arduino.h>

/**************************************************************************/
/*!
    @brief  Simple utility class

    This class encapsulate a number of utility functions for the AVR DB series
  and/or dxCore The intention is to have one place to consolidate functions
  providing information that may be useful to debug the application

*/
/**************************************************************************/
class dxUtilClass {
protected:
  const short MVIO_unknown = -1;
  const short MVIO_belowRange = 0;
  const short MVIO_ok = 1;

  short MVIO_status = MVIO_unknown;

public:
  void begin(); // Must be called again if another function modifies the ADC
                // reference and/or resolution

  float getVdd();
  float getVddio2();
  float getTKelvin();
  float getTCelsius();

  void printResetFlags();
  bool pollMVIOstatus();
  void checkVoltages();
  void checkTemperature();
};

extern dxUtilClass dxUtil;

#endif // DXUTIL_H__
