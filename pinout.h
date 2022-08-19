/**************************************************************************/
/*!
  @file     pinout.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  In this file we consolidate all I/O pin definitions for this application

  Pin definitions are in the form PIN_Pxn where x is PORT x and n is pin n in
  said port. This is the preferred form for dxCore (rather than the pin number
  commonly used with the AVR cores)

  In addition to pin numbers, we also define VPORT and bitmaps for direct port
  manipulation

  Note for users coming from the legacy AVR! PIN_PC1 would nortmally be the MISO
  pin for the SPI1 port, configured as an output. We use SPI1 to talk to the
  K197 as client. Now, we have no data to send to the 197, so we re-use PIN_PC1
  as an input instead. This would not be possible for the old AVRs, but it is
  supported by the AVR DB when the SPI peripheral is in client mode
*/
/**************************************************************************/

#ifndef PINOUT_H__
#define PINOUT_H__
#include <Arduino.h>
// This is the usage on a AVRxxDB28 (28 pin device) using dxCore

// PORT A
//                PIN_PA0 is serial TX, only used via Serial
//                PIN_PA1 is serial RX, only used via Serial
#define OLED_DC PIN_PA2 ///< connected to OLED Data/command pin
#define OLED_SS PIN_PA3 ///< connected to OLED Slave Select pin
#define OLED_MOSI                                                              \
  PIN_PA4 ///< SPI0 MOSI, normally used via SPI but we need to use pinMode to
          ///< work around a bug in the micro...
//                PIN_PA5 is SPI0 MISO, not connected
//                PIN_PA6 is SPI0 SCK,  used via SPI for the OLED
//                PIN_PA7 is the built i LED, already defined by dxCore as
//                LED_BUILTIN

// PORT C
#define SPI1_MOSI PIN_PC0 ///< MOSI Pin for the Client SPI interface from K197
#define MB_CD                                                                  \
  PIN_PC1 ///< Data/command pin for the  Client SPI interface from K197
#define SPI1_SCK PIN_PC2 ///< SCK Pin for the Client SPI interface from K197
#define SPI1_SS PIN_PC3  ///< SS Pin for the Client SPI interface from K197

// PORT D
#define MB_RCL PIN_PD1 ///< connected to RCL input of the main board
#define MB_STO PIN_PD2 ///< connected to STO input of the main board
#define MB_REL PIN_PD3 ///< connected to REL input of the main board
#define MB_DB PIN_PD4  ///< connected to DB  input of the main board
#define UI_STO PIN_PD5 ///< connected to STO push button
//                PIN_PD6 is not connected
#define UI_RCL PIN_PD7 ///< connected to RCL push button

// PORT F
#define UI_REL PIN_PF0 ///< connected to REL push button
#define UI_DB PIN_PF1  ///< connected to DB push button

// VPORT definitions (when using direct port manipulation)
#define MB_STO_VPORT VPORTD
#define MB_RCL_VPORT VPORTD
#define MB_REL_VPORT VPORTD
#define MB_DB_VPORT VPORTD
#define UI_STO_VPORT VPORTD
#define UI_RCL_VPORT VPORTD
#define UI_REL_VPORT VPORTF
#define UI_DB_VPORT VPORTF

// Bitmap definitions (when using direct port manipulation)
#define MB_CD_bm 0x02
#define SPI1_SS_bm 0x08

#define MB_RCL_bm 0X02
#define MB_STO_bm 0X04
#define MB_REL_bm 0X08
#define MB_DB_bm 0X10
#define UI_STO_bm 0X20
#define UI_RCL_bm 0X80

// PORT F
#define UI_REL_bm 0X01
#define UI_DB_bm 0X02

// SPI swap to use for OLED
#define OLED_SPI_SWAP_OPTION SPI0_SWAP_DEFAULT

#endif // PINOUT_H__
