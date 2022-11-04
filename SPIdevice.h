/**************************************************************************/
/*!
  @file     SPIdevice.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the SPIdevice class

  This class is responsible to receive the raw information from the K197/K197A
  bench meter main board as SPI client
*/
/**************************************************************************/
#ifndef SPI_DEVICE_H
#define SPI_DEVICE_H
#include <Arduino.h>
extern const char CH_SPACE;

#define PACKET 18      ///< our SPI packet is 18 bytes max
#define PACKET_DATA 17 ///< size of the packet when it contains normal data

#define DEVICE_USE_INTERRUPT ///< when defined, the code will use interrupt to
                             ///< interface to the SPI peripheral. Otherwise it
                             ///< will use polling.

/**************************************************************************/
/*!
    @brief  Simple class to handle a cluster of buttons.

    This class is responsible to handle the SPI client interface and receive the
   raw information from the 197/197A main board

    It is expected that this class is used as a base class rather than directly
   (albeit this is not a hard requirement)

*/
/**************************************************************************/
class SPIdevice {
public:
  SPIdevice(){};
  void setup();
  bool hasNewData();
  byte getNewData(byte *data); // This function must be called as soon as
                               // hasNewData() returns true, otherwise data may
                               // be ovewritten by the next update
  void debugPrintData(byte *data, byte n = PACKET_DATA);

  /*!
      @brief  check if a buffer overflow has been detected by the SPI peripheral
      @return true if collision detcted (buffer overflow)
  */
  bool collisionDetected() {
    return SPI1.INTFLAGS & SPI_BUFOVF_bm ? true : false;
  };
};

#endif // SPI_DEVICE_H
