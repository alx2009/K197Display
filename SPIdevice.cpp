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

*/
/**************************************************************************/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include <string.h>

#include "SPIdevice.h"
#include "pinout.h"

volatile byte nbyte = 0;
volatile byte spiBuffer[PACKET];
volatile bool done = false;

#ifdef DEVICE_USE_INTERRUPT
void ss_changing() {
  if (VPORTC.IN & SPI1_SS_bm) { // device de-selected
    done = true;
  } else { // device selected
    nbyte = 0;
  }
}
#endif // DEVICE_USE_INTERRUPT

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

void SPIdevice::debugPrintData(byte *data, byte n) {
  for (int i = 0; i < n; i++) {
    Serial.print("0x");
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
}

#ifdef DEVICE_USE_INTERRUPT
// SPI interrupt routine
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
