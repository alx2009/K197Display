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

volatile byte nbyte =
    0; ///< keep track of characters received from the SPI client
volatile byte spiBuffer[PACKET]; ///< buffer used to receive data from SPI
volatile bool done = false; ///< set at the end of the SPI transfer, signals
                            ///< that a complete message is available

#ifdef DEVICE_USE_INTERRUPT
/*!
    @brief  Interrupt handler, called when the SS pin changes (only when
   DEVICE_USE_INTERRUPT is defined)
*/
void ss_changing() {
  if (VPORTC.IN & SPI1_SS_bm) { // device de-selected
    done = true;
  } else { // device selected
    nbyte = 0;
  }
}
#endif // DEVICE_USE_INTERRUPT
/*!
    @brief  setup the SPI peripheral. Must be called first, before any other
   member function
*/
void SPIdevice::setup() {
  pinMode(SPI1_MOSI, INPUT);
  pinMode(MB_CD, INPUT); // Command/Data input - It is configured as inpout, so
                         // it won't be used as MISO by the SPI in slave mode
  pinMode(SPI1_SCK, INPUT);
  pinMode(SPI1_SS, INPUT);
  // pinMode(MB_RCL, OUTPUT);
  // pinMode(MB_STO, OUTPUT);
  // pinMode(MB_REL, OUTPUT);
  // pinMode(MB_DB, OUTPUT);

  // turn on SPI in client mode (peripheral is client, Keithley motherboard is
  // the host controlling the client)
  SPI1.CTRLA =
      SPI_ENABLE_bm; // Other flags: SPI_DORD=0 (MSb first), SPI_MASTER=0.
                     // SPI_CLK2X, SPI_PRESC: not used when SPI_MASTER=0

  // Set SPI Mode
  SPI1.CTRLB =
      SPI_BUFEN_bm |
      SPI_MODE_0_gc; // Other flags: SPI_BUFEN=1 (normal mode). SPI_BUFWR=0 (not
                     // need to transmit); SPI_SSD not used when SPI_MASTER=0

#ifdef DEVICE_USE_INTERRUPT
  cli(); // we want interrupts to fire after they are properly configured

  // enable interrupts
  SPI1.INTCTRL = SPI_RXCIE_bm; // Other flags not used when SPI_BUFEN=0

  attachInterrupt(
      digitalPinToInterrupt(SPI1_SS), ss_changing,
      CHANGE); // TODO: reflash bootloader to avoid using attachInterrupt

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
  return done;
#else  // No interrupt - this means we need to poll the SPI registers & do all
       // the work here!
  static bool SS_active = false;
  if (SS_active) {
    while (SPI1.INTFLAGS & SPI_RXCIF_bm) { // we have new SPI1.DATA
      volatile byte c =
          SPI1.DATA; // Note: this also clears RXCIF if the buffer is empty
      if (nbyte < PACKET) {
        if (VPORTC.IN & MB_CD_bm) { // this is a command, skip
                                    // DO Nothing
        } else {                    // this instead is data
          spiBuffer[nbyte] = c;
          nbyte++;
        }
      }
    }
    if (VPORTC.IN & SPI1_SS_bm) { // device has been de-selected
      done = true;
      SS_active = false;
    }
  } else {
    if ((VPORTC.IN & SPI1_SS_bm) == 0x00) { // device has been selected
      SS_active = true;
      nbyte = 0;
    }
  }
  return done;
#endif // DEVICE_USE_INTERRUPT
}

/*!
      @brief process a new reading that has just been received via SPI and
   return a copy of the SPI data buffer

      If a number != 9 is returned, indicates that the data was NOT correctly
   transmitted

      @param data byte array that will receive the copy of the data. MUST have
   room for at least 9 elements!
      @return the number of bytes copied into data.
*/
byte SPIdevice::getNewData(byte *data) {
  byte returnvalue;
  while (!hasNewData()) {
    ;
  }
  memcpy(data, (void *)spiBuffer, PACKET);
  done = false;
  returnvalue = nbyte;
  nbyte = 0;
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
    DebugOut.print("0x");
    DebugOut.print(data[i], HEX);
    DebugOut.print(' ');
  }
}

#ifdef DEVICE_USE_INTERRUPT
/*!
    @brief  Interrupt handler, called for SPI1 events (only when
   DEVICE_USE_INTERRUPT is defined)
*/
ISR(SPI1_INT_vect) { // TODO: read all available bytes in one go
  while (SPI1.INTFLAGS & SPI_RXCIF_bm) {
    volatile byte c =
        SPI1.DATA; // Note: this also clears RXCIF if the buffer is empty
    // SPI1.INTFLAGS = SPI_RXCIF_bm; // Clear the Interrupt flag by writing 1
    // (probably not needed)

    if (nbyte >= PACKET) {
      return;
    }
    if (VPORTC.IN & MB_CD_bm) { // this is a command, skip
                                // DO Nothing
    } else {                    // this instead is data
      spiBuffer[nbyte] = c;
      nbyte++;
    }
  }
} // end of interrupt service routine (ISR) SPI1_INT_vect
#endif // DEVICE_USE_INTERRUPT
