/**************************************************************************/
/*!
  @file     dxUtil.h

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
#include <limits.h> // needed for INT_MAX

//#define CHECK_STACK_SIZE ///< when defined, add the possibility to check stack size

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
  static const short MVIO_unknown =
      -1; ///< Indicates the status of MVIO is not known
  static const short MVIO_belowRange = 0; ///< Indicates MVIO below range
  static const short MVIO_ok = 1; ///< Indicates MVIO within working range

  short MVIO_status = MVIO_unknown; ///< Keep track of MVIO status

  uint8_t reset_flags = 0x00; ///< save the latest reset flags
  bool firstBegin =
      true; ///< true if the next/current begin is the first one called

public:
  void begin(); // Must be called again if another function modifies the ADC
                // reference and/or resolution

  float getVdd();
  float getVddio2();
  float getTKelvin();
  float getTCelsius();

  void printResetFlags();
  bool pollMVIOstatus();
  void checkVoltages(bool newline = true);
  void checkTemperature(bool newline = true);

  /*!
      @brief  test if reset was due to UPDI
      @return returns true if the reset was due to UPDI, false otherwise
  */
  bool resetReasonUPDI() { return (reset_flags & RSTCTRL_UPDIRF_bm) != 0x00; };
  /*!
      @brief  test if reset was due to Watchdog timeout (WDT)
      Note that if the bootloader is used, it will be set almost all the time
     (see
     https://github.com/SpenceKonde/DxCore/blob/master/megaavr/extras/Ref_Reset.md)
      @return returns true if the reset was due to WDT, false otherwise
  */
  bool resetReasonWDT() {
    return reset_flags == RSTCTRL_WDRF_bm;
  }; // Bootloader always cause WDT restart. It is truly WDT if it is the only
     // flag set
  /*!
      @brief  test if reset was due to SW reset
      @return returns true if the reset was due to SW reset, false otherwise
  */
  bool resetReasonSWReset() { return (reset_flags & RSTCTRL_SWRF_bm) != 0x00; };
  /*!
      @brief  test if reset was due to HW reset
       @return returns true if the reset was due to HW reset, false otherwise
  */
  bool resetReasonHWReset() {
    return (reset_flags & RSTCTRL_EXTRF_bm) != 0x00;
  };
  /*!
      @brief  test if reset was due to brownout reset
      @return returns true if the reset was due to brownout, false otherwise
  */
  bool resetReasonBrownout() {
    return (reset_flags & RSTCTRL_BORF_bm) != 0x00;
  };
  /*!
      @brief  test if reset was due to power on reset
      @return returns true if the reset was due to power on, false otherwise
  */
  bool resetReasonPowerOn() { return (reset_flags & RSTCTRL_PORF_bm) != 0x00; };

#ifdef CHECK_STACK_SIZE
private:
  int minStack = INT_MAX; ///< keep track of the minimum free stack
  int minFreeStack();
public:
  int checkFreeStack();
  void reportFreeStack(bool reportAlways = true);
# define CHECK_FREE_STACK() CHECK_FREE_STACK()
# define REPORT_FREE_STACK(...) dxUtil.reportFreeStack(__VA_ARGS__)
#else
#  define CHECK_FREE_STACK()  ///< Does nothing when not profiling
#  define REPORT_FREE_STACK(...)  ///< Does nothing when not profiling
#endif // CHECK_STACK_SIZE
};

extern dxUtilClass dxUtil; ///< Predefined dxUtilClass object to use

#endif // DXUTIL_H__
