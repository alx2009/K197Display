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
- PA2-PA6 used to interface the OLED
- PA7 is connected to a LED (optional, default LED_BUILTIN for dxCore) 
- PC0-PC3 used to interface the 197/197A main board (4-wire SPI)
- PD1-PD4 connected to the pushbutton cluster on the front panel
- PD5, PD7, PF0, PF1 used to interface to the 197/197A main board (push-button input)

The SW has initial support for a HC-05 bluetooth module. If used, in addition to PA0-PA1 (Serial RX/TX) pin PA5 should be connected to the BT_STATE so that the SW can detect and display the connection status. In addition BT_STATE can also be connected via a capacitor to the reset pin to autoreset the micro. The module should be configure to interface with an Arduino with baud rate = 115200 according to the many instructions available (hint: google hc-05 bluetooth module arduino).

Functionality:
-------------
The current SW is implementing the same functions available in the 197/197A, with minor differences due to the different display technology used (plus any unintended differences - the Issues section is available for reporting them, as well as new feature requests). Additional functions:
- At startup the module tries to detemine if Serial is connected. If it is, BT is displayed (assuming a Bluetooth module is connected)
- Pin PA5 is monitored continuosly. When low, "<->" is displayed next to "BT" to indicate an active bluetooth connection
- Some commands can be entered via Serial connection (press ? for a list)
- Holding the "REL" button for 0.8 s or more and then releasing it will show the debug output on the left half of the OLED screen. Hold again to go back to the normal display.

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
