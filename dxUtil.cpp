/**************************************************************************/
/*!
  @file     dxUtil.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the dxUtil class, see dxUtil.h for the class definition

*/
/**************************************************************************/
#include "dxUtil.h"
#include <Arduino.h>

// static const float vstep = 10.24 / 4096.0; // conversion from ADC reading to
// Volts (assuming 1.024V reference and 12 bit resolution)
static const float vstep =
    20.48 / 4096.0; // conversion from ADC reading to Volts (assuming 2.048V
                    // reference and 12 bit resolution)

dxUtilClass dxUtil;

// using 1.024 Volt would lead to in creased precision for V, but the Temp.
// sensor generates > 1.024V around ambient temperature... the precision
// with 2.048 is more than enough for our purposes, so we use 2.048V in this
// application
void dxUtilClass::begin() {
  // analogReference(INTERNAL1V024);
  analogReference(INTERNAL2V048);
  analogReadResolution(12);
}

void dxUtilClass::printResetFlags() {
  uint8_t reset_flags =
      GPR.GPR0; // Optiboot stashes the reset flags here before clearing them to
                // honor entry conditions
  if (reset_flags & RSTCTRL_UPDIRF_bm) {
    Serial.println(F("Reset by UPDI (code just uploaded now)"));
  }
  if (reset_flags & RSTCTRL_WDRF_bm) {
    Serial.println(F("reset by WDT timeout"));
  }
  if (reset_flags & RSTCTRL_SWRF_bm) {
    Serial.println(F("reset at request of user code."));
  }
  if (reset_flags & RSTCTRL_EXTRF_bm) {
    Serial.println(F("Reset because reset pin brought low"));
  }
  if (reset_flags & RSTCTRL_BORF_bm) {
    Serial.println(F("Reset by voltage brownout"));
  }
  if (reset_flags & RSTCTRL_PORF_bm) {
    Serial.println(F("Reset by power on"));
  }
}

bool dxUtilClass::pollMVIOstatus() {
  // Polling the VDDIO2S bit
  if (MVIO.STATUS & MVIO_VDDIO2S_bm) { // MVIO within usable range
    if (MVIO_status != MVIO_ok) {
      Serial.println(F("MVIO ok!"));
      MVIO_status = MVIO_ok;
      return true;
    }
  } else { // MVIO outside usable range
    if (MVIO_status != MVIO_belowRange) {
      Serial.println(F("MVIO not ok!"));
      MVIO_status = MVIO_belowRange;
      return true;
    }
  }
  return false;
}

float dxUtilClass::getVdd() {
  int adc_reading =
      analogRead(ADC_VDDDIV10); // Note: temp. will be way out of range in case
                                // analogRead reports an ADC error
  float vdd = adc_reading * vstep;
  return vdd;
}

float dxUtilClass::getVddio2() {
  int adc_reading =
      analogRead(ADC_VDDIO2DIV10); // Note: temp. will be way out of range in
                                   // case analogRead reports an ADC error
  float vddio2 = adc_reading * vstep;
  return vddio2;
}

float dxUtilClass::getTKelvin() {
  uint16_t adc_reading =
      analogRead(ADC_TEMPERATURE); // Note: temp. will be way out of range in
                                   // case analogRead reports an ADC error
  // Serial.print("ADC="); Serial.println(adc_reading);

  // Using the datasheet recommended method with precision 1/4th Kelvin
  uint16_t sigrow_offset =
      SIGROW.TEMPSENSE1; // Read unsigned value from signature row
  uint16_t sigrow_slope =
      SIGROW.TEMPSENSE0; // Read unsigned value from signature row
  uint32_t temp = sigrow_offset - adc_reading;
  temp *= sigrow_slope; // Result will overflow 16-bit variable
  temp += 0x0200;       // Add 4096/8 to get correct rounding on division below
  temp >>= 10; // Round off to nearest degree in Kelvin, by dividing with 2^102
               // (1024)
  // uint16_t temperature_in_K = temp;

  float ftempk = float(temp) / 4.0;
  return ftempk;
}

float dxUtilClass::getTCelsius() { return getTKelvin() - 273.15; }

void dxUtilClass::checkVoltages() {
  Serial.print(F("Vdd="));
  Serial.print(getVdd());
  Serial.print(F(" V, Vddio2="));
  Serial.print(getVddio2());
  Serial.println(F(" V"));
}

void dxUtilClass::checkTemperature() {
  Serial.print(F("T="));
  Serial.print(getTCelsius());
  Serial.println(F(" C"));
}
