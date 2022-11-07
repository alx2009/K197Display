/**************************************************************************/
/*!
  @file     BTmanager.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the BTmanager class

  This class is responsible for managing the Bluetooth module

*/
/**************************************************************************/
#ifndef BT_MANAGER_H
#define BT_MANAGER_H

enum BTmanagerResult {
    BTmoduleOff       = 0x01, ///< No change, status is off
    BTmoduleOn        = 0x02, ///< No change, status is on
    BTmoduleTurnedOn  = 0x11, ///< status changed detected, now on 
    BTmoduleTurnedOff = 0x12  ///< status changed detected, now off
};

class BTmanager {
  private:
      bool bt_module_present = false; ///< true if BT module is powered on
      bool bt_module_connected =
         false; ///< true if connection detected via BT_STATE pin

  public:
     BTmanager() {}; ///< Default constructor
     void setup();
     BTmanagerResult checkPresence();
     BTmanagerResult checkConnection();
     /*!
      @brief query if the Bluetooth module is present (powered on)
      @return true if the module was detected last time checkPresence() was called
     */
     bool isPresent() {return bt_module_present;};
     /*!
      @brief query if the Bluetooth module is connected
      @return true if the connection was detected last time checkConnection() was called
     */
     bool isConnected() {return bt_module_connected;};
     /*!
      @brief query if the Bluetooth module is present and connected
      @details in normal operation validconnection() and is connected should return the same result, but due to timing of the check in rare occasions they may differ. 
      @return true if both isPresent() and isConnected() return true
     */
     bool validconnection() {return bt_module_present && bt_module_connected;};
};

extern BTmanager BTman;

#endif //BT_MANAGER_H
