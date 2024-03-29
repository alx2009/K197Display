/**************************************************************************/
/*!
  @file     SPIdevice.cpp

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file implements the SPIdevice class, see SPIdevice.h for the class
  definition

  This class is responsible to receive the raw information from the K197/K197A
  bench meter main board as SPI client

  Acknowledments: SPI client SW Originally wrote for AVR 328 by Nick Gammon,
  http://www.gammon.com.au/spi

  Ported to AVR DB and dxCore by Alx2009

  Use SPI buffered mode (in the legacy AVR buffered mode was only available in
  master mode, and only using the USART perhipheral in Master SPI mode)

  For this particular application buffered mode does not have any significant
  advantage, but since there are almost no example on the net I thought to use
  it as it can be very useful in other cases, and does not add any major
  complication in Client mode

  There are two implementation strategies, selected based on
  DEVICE_USE_INTERRUPT being defined or not
  - if DEVICE_USE_INTERRUPT is defined, SPI data is received with interrupts
  - if DEVICE_USE_INTERRUPT is not defined then polling is used

  Both strategies have pro and cons. As of now, both strategies work equally
  well. Both options are available as one or the other may work better for users
  that want to customize this sketch for their specific needs
*/
/**************************************************************************/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include <string.h>

#include "SPIdevice.h"
#include "debugUtil.h"
#include "pinout.h"

//#define nbyte GPIOR1 //uncomment to use GPIOR1 to speed up interrupt handler
// slightly
#ifndef nbyte
static volatile byte nbyte =
    0x00; ///< keep track of characters received from the SPI client
#endif
//#define SPIflags GPIOR3 //uncomment to use GPIOR3 to speed up interrupt
// handler slightly
#ifndef SPIflags
static volatile byte SPIflags =
    0x00; ///< Various flags used in interupt handlers
#endif
#define SPIdone                                                                \
  0x02 ///< flag used to signal that all SPI data have been received

volatile byte spiBuffer[PACKET_DATA]; ///< buffer used to receive data from SPI

#ifdef DEVICE_USE_INTERRUPT
/*!
  @brief  Interrupt handler, called when the SS pin changes (only when
 DEVICE_USE_INTERRUPT is defined)
 @details we assume that the interrupt is called before a SPI data is ready
 (resetting nbyte to zero while we are reading commands has no effect since they
 are anyway discarded by the SPI interrupt handler). Because the K197 send a
 number of command bytes first, this assumption is always satisfied.
*/
ISR(SPI1_PORT_vect) {                // __vector_30
  SPI1_VPORT.INTFLAGS |= SPI1_SS_bm; // clears interrupt flag
  if (SPI1_VPORT.IN & SPI1_SS_bm) {  // device de-selected
    SPIflags |= SPIdone;
  } else { // device selected
    cli();
    nbyte = 0;
    sei();
  }
}
#endif // DEVICE_USE_INTERRUPT

/*!
    @brief  setup the SPI peripheral. Must be called first, before any other
   member function
*/
void SPIdevice::setup() {
  nbyte = 0x00;
  SPIflags = 0x00;
  pinMode(SPI1_MOSI, INPUT);
  pinMode(MB_CD, INPUT); // Command/Data input - It is configured as inpout, so
                         // it won't be used as MISO by the SPI in slave mode
  pinMode(SPI1_SCK, INPUT);
  pinMode(SPI1_SS, INPUT);

  // turn on SPI in client mode (peripheral is client, Keithley motherboard is
  // the host controlling the client)
  SPI1.CTRLA =
      SPI_ENABLE_bm; // Other flags: SPI_DORD=0 (MSb first), SPI_MASTER=0.
                     // SPI_CLK2X, SPI_PRESC: not used when SPI_MASTER=0

  // Set SPI Mode
  SPI1.CTRLB =
      SPI_BUFEN_bm |
      SPI_MODE_0_gc; // SPI_BUFEN=1 (normal mode). SPI_BUFWR=0 (not
                     // need to transmit); SPI_SSD not used when SPI_MASTER=0

#ifdef DEVICE_USE_INTERRUPT

  // Set the SPI1 INT vector as high priority (can interrupt other interrupt
  // handlers)
  CPUINT.LVL1VEC = SPI1_INT_vect_num;

  // Set the interrupt scheduling to static, with the interrupt used by SPI1
  // pins as highest priority (will be serviced first)
  CPUINT.LVL0PRI = SPI1_PORT_vect_num - 1;

  cli(); // we want interrupts to fire after they are properly configured

  // enable interrupts
  SPI1.INTCTRL = SPI_RXCIE_bm; // Other flags not used

#ifndef CORE_ATTACH_NONE // Manual handling of pin interrupts possible
#error "attachInterrupt must be set to \"only enabled ports\" in the Tools menu"
#endif
  SPI1_PORT.SPI1_PIN_SS_CTRL =
      (SPI1_PORT.SPI1_PIN_SS_CTRL & 0xF8) | PORT_ISC_BOTHEDGES_gc;

  sei(); // now we re-enable interrupts
#endif   // DEVICE_USE_INTERRUPT
}

/*!
    @brief  check if new data has been received from SPI

    After this function returns true, it will continue to return true until the
   data is actually retrived and processed, e.g calling getNewData();

    @return true if new data is available
*/
bool SPIdevice::hasNewData() {
#ifdef DEVICE_USE_INTERRUPT
  return SPIflags & SPIdone;
#else  // No interrupt - this means we need to poll the SPI registers & do all
       // the work here!
  static bool SS_active = false;
  if (SS_active) {
    while (SPI1.INTFLAGS & SPI_RXCIF_bm) { // we have new SPI1.DATA
      volatile byte c =
          SPI1.DATA; // Note: this also clears RXCIF if the buffer is empty
      if (nbyte < PACKET_DATA) {
        if (SPI1_VPORT.IN & MB_CD_bm) { // this is a command, skip
                                        // DO Nothing
        } else {                        // this instead is data
          spiBuffer[nbyte] = c;
          nbyte++;
        }
      }
    }
    if (SPI1_VPORT.IN & SPI1_SS_bm) { // device has been de-selected
      SPIflags |= SPIdone;
      SS_active = false;
    }
  } else {
    if ((SPI1_VPORT.IN & SPI1_SS_bm) == 0x00) { // device has been selected
      SS_active = true;
      nbyte = 0;
    }
  }
  return SPIflags & SPIdone;
#endif // DEVICE_USE_INTERRUPT
}

/*!
      @brief process a new reading that has just been received via SPI and
   return a copy of the SPI data buffer

     @details If a number != 9 is returned, indicates that the data was NOT
   correctly transmitted

   Note that this method will block until hasNewData() returns true.
   If the caller doesn't want to block execution, it has to check hasNewData()
   before calling getNewData()

      @param data byte array that will receive the copy of the data. MUST have
   room for at least PACKET_DATA elements!
      @return the number of bytes copied into data.
*/
byte SPIdevice::getNewData(byte *data) {
  byte returnvalue;
  while (!hasNewData()) {
    ;
  }
  memcpy(data, (void *)spiBuffer, PACKET_DATA);
  cli();
  SPIflags &= (~SPIdone);
  returnvalue = nbyte;
  nbyte = 0;
  sei();
  return returnvalue;
}

/*!
      @brief print a byte buffer to DebugOut

      Normally useful for debugging

      @param data pointer to the buffer
      @param n number of bytes to print

*/
void SPIdevice::debugPrintData(byte *data, byte n) {
  for (int i = 0; i < n; i++) {
    DebugOut.print(F("0x"));
    DebugOut.print(data[i], HEX);
    DebugOut.print(CH_SPACE);
  }
}

#ifdef DEVICE_USE_INTERRUPT
/*!
    @brief  Interrupt handler, called for SPI1 events (only when
   DEVICE_USE_INTERRUPT is defined)

   @details there is a weakness here, in that we assume we can read data fast
   enough that the MB_CD_bm has not changed status after the data is sent. If
   this is not true we may lose one or more data bytes. This will result in less
   than 9 data byteas read, leading to the data set being discarded. The K197
   timing is slow enough that this is not normally a problem, except in specific
   circumstances (e.g. writing to EEPROM) when losing data is acceptable. Note
   however that this also mean using buffered SPI mode doesn't really make a big
   difference because by the same line of reasoning we should always read a byte
   before the next one is received... but it doesn't hurt.

   Note that in setup() we have set this vecotor as the High-Priority Interrupt.
   So while we execute no other task can have access to the data we use.
*/
ISR(SPI1_INT_vect) { // __vector_37  TODO: read all available bytes in one go
  while (SPI1.INTFLAGS & SPI_RXCIF_bm) {
    volatile byte c =
        SPI1.DATA; // Note: this also clears RXCIF if the buffer is empty
    if (nbyte >= PACKET_DATA) {
      return;
    }
    if (SPI1_VPORT.IN & MB_CD_bm) { // this is a command, skip
                                    // DO Nothing
    } else {                        // this instead is data
      spiBuffer[nbyte] = c;
      nbyte++;
    }
  }
}
#endif // DEVICE_USE_INTERRUPT
