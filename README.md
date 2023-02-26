# K197Display
This is a Arduino sketch for a display board intended as a replacement for a 197/197A bench multimeter. The board uses a 2.8" 256x64 OLED (SSD1322) and AVR DB on dxCore (see references below) 

DISCLAIMER: Please note that the purpose of this repository is educational. Any use of the information for any other purpose is under own responsibility.

Releasing this sketch was inspired from discussions in the EEVBlog forum: 
https://www.eevblog.com/forum/testgear/keithley-197a-owners_-corner/
and
https://www.eevblog.com/forum/projects/replacement-display-board-for-keithley-197a/msg4232665/#msg4232665 

Prerequisites
-------------
The sketch have been developed and tested using a AVR64DB28 microcontroller. It should run with AVR128DB28 and can be easily adapted to packages with 32, 48 and 64 pins. It will not run with 32 Kbit chips. It may run with the DA chips, however those chips will require some kind of voltage conversion somewhere, considering that the display runs at 3.3V and the 197/197A main board interface is done at 5V levels.

The display need to be configured to use SPI (usually this means moving one resistor, see references)

You can refer to the links above for more information about the HW used

If you do not have the 197/K197A available, but for whatever reason you want to test this software, you may want to look at this simulator: https://github.com/alx2009/displayBoardTester

You will need the Arduino IDE, with dxCore (https://github.com/SpenceKonde/DxCore) and the u8g2 library (https://github.com/olikraus/u8g2/wiki)

Connections:
------------
Currently this sketch assumes that the microcontroller is connected as follows:
- AVDD and VDD connected to a 3.3V power source (which is also supplied to the display)
- VDDIO2 connected to +5V (same voltage as the 197/197A main board +5V digital power rail)
- PA0-PA1 used as Serial (for programming via bootloader and/or debug output)
- PA2 used to detect when the Bluetooth Module is powered on (if used)
- PA3, PA4, PA6 used to interface the OLED (3 wire SPI mode)
- PA5 is connected to the BT module BT_STATE (if used)
- PA7 is connected to a LED (optional, default LED_BUILTIN for dxCore) 
- PC0-PC3 used to interface the 197/197A main board (4-wire SPI)
- PD1-PD4 connected to the pushbutton cluster on the front panel
- PD5, PD7, PF0, PF1 used to interface to the 197/197A main board (push-button input)

The SW has some support for a HC-05 bluetooth module. If used, in addition to PA0-PA1 (Serial RX/TX) pin PA2 should be connected to the BT module 5V power via a SW divider (the pin max voltage is 3.3V), pin PA5 should be connected to the BT_STATE so that the SW can detect and display the connection status. In addition BT_STATE can also be connected via a capacitor to the reset pin to autoreset the micro. The module should be configure to interface with an Arduino with baud rate = 115200 according to the many instructions available (hint: google hc-05 bluetooth module arduino).

The definition of the pins in pinout.h can be changed to support the OLED in 4 wire SPI mode, but in such a case it will not be possible to detect when the Bluetooth module is powered on and off via PIN PA2.

Functionality:
-------------
The current SW is implementing the same functions available in the 197/197A (with minor differences due to the different display technology used), plus the following additional functions:
- Bluetooth interface
- Statistics
- Graph
- Additional temperature measurement (with K type thermocouple)

Holding the "REL" button for 0.5 s will show a Options menu to enable the additional functions and variouis options, as well as data logging to bluetooth serial.

clicking the "STO" button holds the value currently displayed (when the option to repurpose STO and RCL is enabled in the options menu). Hold mode can be entered while in graph mode, but it has no effect (currently, this may change in future revisions). Changing display mode cancel the holding. Holding only affects what is displayed, internally statistics are continuosly updated and logging to bluetooth is not affected. A second click exit hold. 

Some commands can be entered via Serial connection (connect via Serial/bluetooth Serial and send "?" for a list)

Bluetooth support:
-------------
The SW tries to detemine if the BT module is powered on. If it is, BT is displayed. The BT module pin state is also monitored continuosly. When the pin is low, "<->" is displayed next to "BT" to indicate an active bluetooth connection. 

Logging to bluetooth can be activated via the options menu. A time stamp can be selected in the options menu. Note that this time stamp is based on the millis() function, which is only as precise as the Arduino clock.

Temperature measurement:
-------------
The additional temperature measurement mode - when enabled in the Options menu - supports connecting a K type thermocouple to measure temperature. To enter this mode the K197 must be in the mV DC range, then the "dB" button is clicked. Clicking the "dB" button once more will enter dB mode as normal.

The temperature of the cold joint is also shown (this is measwured with the AVR internal temperature sensor). The accuracy is limited by the accuracy of the AVR temperature sensor, around 3C according to the data sheet.

Options menu:
-------------
At the bottom of the "Options" menu a "Show log" option shows a window with the latest debug output (useful for troubleshooting issues that only happen when Serial is turned off, e.g. BT module detection problems)

Two menu items enable storing and retrieving the configuration to the EEPROM. At startup and after reset the retrieve happens automatically.

Statistics display mode
-------------
An additional "statistics" display mode is available when the option to repurpose STO and RCL is enabled in the options menu. In this mode in addition to the instantaneous value the average, minimum and maximum value is displayed. Holding the STO button alternates between "normal" and "statistics" mode. Not all annunciators are available in statistics mode. The statistics themselves are not affected from the display mode switch, but they are reset whenever the measurement conditions change  (including for example measurement unit, REL state, AC button, etc.).

Graph display mode
-------------
An additional "graph" display mode is available when the option to repurpose STO and RCL is enabled in the options menu. In this mode a graph of the measurement is shown. Double clicking the STO button alternates between "normal" and "graph" mode. Not all annunciators are available in graph mode. The graph itself is not affected from the display mode switch, but it is reset whenever the measurement conditions change (including for example measurement unit, REL state, AC button, etc.).

Sample rate and other options can be set in the options menu.

The x (time) scale changes automatically depending on how many samples have been collected. At most 180 samples can be stored, corresponding to about 60s at the fastest sample rate. Note that the timje scale is only approximate, as we assume that the voltmeter is measuring at exactly 3 Hz. When a more exact analysis is required, it is recommended to log the data via bluetooth.

Keyboard: 
---------
compared to the original K197, the use of the the pushbuttons on the front panel changes as follows:
- Holding the REL button will enter the Options menu
- double click of the REL button resets the statistics (but otherwise does not affect the reference value)
- Normal STO and RCL functions can be disabled from the options menu. When disabled, the buttons are repurposed as follows:
  - clicking the STO button once hold the currently displayed measurement when in normal or statistics mode. A second click returns to continuos updates.
  - Holding the STO button alternates between "normal" and "statistics" display mode (when repurposing of STO and RCL is enabled)
  - Double clicking the STO button alternates between "normal" and "graph" display mode (when repurposing of STO and RCL is enabled)

When the Options menu is shown, the buttons are used to navigate the menu as follows:
REL = up (hold to exit the menu)
dB  = down
STO = left
RCL = right/select/ok

Porting:
-------
I am not planning to port this SW to other micros. If you want to use an older AVR you should consider Technogeeky implementation instead of mine: https://github.com/technogeeky/keithley-197. 

For other architectures, you are welcomed to clone this repository and do the porting. 

Contributions:
-------------
Merge requests with bug fixes and new relevant features will be considered, I cannot promise more at this stage.

Please note that this sketch is formatted with Clang (default LLVM style) and documented with Doxygen. If possible conform to this, which is a convention used by many Arduino libraries (e.g. see for example Adafruit excellent guide https://learn.adafruit.com/contribute-to-arduino-with-git-and-github/overview)

Useful References:
------------------
In addition to the EEVBlog threads and library links, the following direct links may save you some time:
Microchip page for the AVR64DB28 microcontroller: https://www.microchip.com/en-us/product/AVR64DB28#document-table
  Recommended readings:
     - Data sheet (obviously)
     - Application Note "Migration from the megaAVR® to AVR® Dx Microcontroller Families" (useful if you have previous experience with older AVR!)
  
DxCore pinout diagram: https://github.com/SpenceKonde/DxCore/blob/master/megaavr/extras/DB28.md

dxCore Improved Digital I/O Functionality https://github.com/SpenceKonde/DxCore/blob/master/megaavr/extras/Ref_Digital.md and 
Direct Port Manipulation https://github.com/SpenceKonde/DxCore/blob/master/megaavr/extras/DirectPortManipulation.md

dxCore guide to SPI (SPI pin mappings): https://github.com/SpenceKonde/DxCore/blob/master/megaavr/libraries/SPI/README.md

dxCore Optiboot (bootloader) reference: https://github.com/SpenceKonde/DxCore/blob/master/megaavr/extras/Ref_Optiboot.md

Programming via UPDI (needed at least the first time, in order to load the bootloader): https://github.com/SpenceKonde/AVR-Guidance/blob/master/UPDI/jtag2updi.md

Link to the OLED display (no affiliation or special endorsement. Only that I bought it there and it works): https://www.aliexpress.com/item/33013330192.html?spm=a2g0o.order_list.0.0.21ef1802HXu3o 
