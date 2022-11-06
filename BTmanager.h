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
     BTmanager() {};
     void setup();
     BTmanagerResult checkPresence();
     BTmanagerResult checkConnection();
     bool isPresent() {return bt_module_present;};
     bool isConnected() {return bt_module_connected;};
     bool validconnection() {return bt_module_present && bt_module_connected;};
};

extern BTmanager BTman;

#endif //BT_MANAGER_H
