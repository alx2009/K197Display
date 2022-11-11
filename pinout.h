/**************************************************************************/
/*!
  @file     pinout.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  In this file we consolidate all I/O pin definitions for this application. In
  addition, we include generally useful constants

  Pin definitions are in the form PIN_Pxn where x is PORT x and n is pin n in
  said port. This is the preferred form for dxCore (rather than the pin number
  commonly used with the AVR cores)

  In addition to pin numbers, we also define VPORT and bitmaps for direct port
  manipulation and a number of useful constants

  Note for users coming from the legacy AVR! PIN_PC1 would normally be the MISO
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
#define SERIAL_TX PIN_PA0 ///< pin corresponding to Serial TX
#define SERIAL_RX PIN_PA1 ///< pin corresponding to Serial RX
// If the OLED is configured for 3Wire SPI then PA2 is used to detect BT module,
// otherwise it is used as D/C pin for OLED
#define BT_POWER PIN_PA2 ///< high when BT module has power
//#define OLED_DC PIN_PA2   ///< OLED Data/command pin [4Wire SPI]
#define OLED_SS PIN_PA3 ///< OLED Slave Select pin
#define OLED_MOSI                                                              \
  PIN_PA4 ///< SPI0 MOSI, normally used via SPI but we need to use pinMode to
          ///< work around a bug in the micro...
#define BT_STATE                                                               \
  PIN_PA5 ///< Bluetooth module STATE, also SPI0 MISO (since we do not need MISO
          ///< for the OLED we use it for BT_STATE)
//                PIN_PA6 is SPI0 SCK,  used via SPI for the OLED
//                PIN_PA7 is the built i LED, already defined by dxCore as
//                LED_BUILTIN

// PORT C
#define SPI1_MOSI PIN_PC0 ///< MOSI Pin for the Client SPI from K197
#define MB_CD PIN_PC1     ///< Data/command pin for the Client SPI from K197
#define SPI1_SCK PIN_PC2  ///< SCK Pin for the Client SPI from K197
#define SPI1_SS PIN_PC3   ///< SS Pin for the Client SPI from K197

// PORT D
#define MB_RCL PIN_PD1 ///< connected to RCL input of the main board
#define MB_STO PIN_PD2 ///< connected to STO input of the main board
#define MB_REL PIN_PD3 ///< connected to REL input of the main board
#define MB_DB PIN_PD4  ///< connected to DB  input of the main board
#define UI_STO PIN_PD5 ///< connected to STO push button
#define BT_EN PIN_PD6  ///< connected to the ENable pin of the bluetooth module
#define UI_RCL PIN_PD7 ///< connected to RCL push button

// PORT F
#define UI_REL PIN_PF0 ///< connected to REL push button
#define UI_DB PIN_PF1  ///< connected to DB push button

// VPORT definitions (when using direct port manipulation)
#define SERIAL_VPORT VPORTA   ///< VPORT for Serial pins
#define BT_STATE_VPORT VPORTA ///< VPORT for BT_STATE pin
#define MB_STO_VPORT VPORTD   ///< VPORT for MB_STO pin
#define MB_RCL_VPORT VPORTD   ///< VPORT for MB_RCL pin
#define MB_REL_VPORT VPORTD   ///< VPORT for MB_REL pin
#define MB_DB_VPORT VPORTD    ///< VPORT for MB_DB pin
#define UI_STO_VPORT VPORTD   ///< VPORT for UI_STO pin
#define UI_RCL_VPORT VPORTD   ///< VPORT for UI_RCL pin
#define UI_REL_VPORT VPORTF   ///< VPORT for UI_REL pin
#define UI_DB_VPORT VPORTF    ///< VPORT for UI_DB pin

// UART definitions
#define BT_USART USART0 ///< This is the UART connected to the bluetooth module

// Bitmap definitions (when using direct port manipulation)
#define SERIAL_RX_bm 0x02 ///< Bitmap for Serial RX pin
#define MB_CD_bm 0x02     ///< Bitmap for MB_CD pin
#define BT_STATE_bm 0x20  ///< Bitmap for BT_STATE pin
#define SPI1_SS_bm 0x08   ///< Bitmap for SPI1 SS pin

#define MB_RCL_bm 0X02 ///< Bitmap for MB_RCL pin
#define MB_STO_bm 0X04 ///< Bitmap for MB_STO pin
#define MB_REL_bm 0X08 ///< Bitmap for MB_REL pin
#define MB_DB_bm 0X10  ///< Bitmap for MB_DB pin
#define UI_STO_bm 0X20 ///< Bitmap for UI_STO pin
#define UI_RCL_bm 0X80 ///< Bitmap for UI_RCL pin

// PORT F
#define UI_REL_bm 0X01 ///< Bitmap for UI_REL pin
#define UI_DB_bm 0X02  ///< Bitmap for UI_DB pin

// PIN SWAP Options
#define OLED_SPI_SWAP_OPTION SPI0_SWAP_DEFAULT ///< SPI swap to use for OLED

// Generally useful constants
extern const char
    CH_SPACE; ///< External constant used whenever we need a ' ' character.
              ///< Having it as global constant saves a few bytes of RAM
#endif        // PINOUT_H__
