/**************************************************************************/
/*!
  @file     K197device.h

  Arduino K197Display sketch

  Copyright (C) 2022 by ALX2009

  License: MIT (see LICENSE)

  This file is part of the Arduino K197Display sketch, please see
  https://github.com/alx2009/K197Display for more information

  This file defines the k197device class

  This class is responsible to store and decode the raw information received
  through the base class SPIdevice, and make it available to the rest of the
  sketch

*/
/**************************************************************************/
#ifndef K197_DEVICE_H
#define K197_DEVICE_H
#include "SPIdevice.h"
#include <Arduino.h>
#include <ctype.h> // isDigit()

// define bitmaps for the various annunciators

// annunciators0
#define K197_AUTO_bm 0x01  ///< bitmap for the AUTO annunciator
#define K197_REL_bm 0x02   ///< bitmap for the REL annunciator
#define K197_STO_bm 0x04   ///< bitmap for the STO annunciator
#define K197_dB_bm 0x08    ///< bitmap for the dB annunciator
#define K197_AC_bm 0x10    ///< bitmap for the AC annunciator
#define K197_RCL_bm 0x20   ///< bitmap for the RCL annunciator
#define K197_BAT_bm 0x40   ///< bitmap for the MAT annunciator
#define K197_MINUS_bm 0x80 ///< bitmap for the "-" annunciator

// annunciators7
#define K197_mV_bm 0x01    ///< bitmap for the "m" annunciator used for mV
#define K197_M_bm 0x02     ///< bitmap for the "M" annunciator
#define K197_micro_bm 0x04 ///< bitmap for the "µ" annunciator
#define K197_V_bm 0x08     ///< bitmap for the "V" annunciator
#define K197_k_bm 0x10     ///< bitmap for the "k" annunciator
#define K197_mA_bm 0x20    ///< bitmap for the "m" annunciator used for mA

// annunciators8
#define K197_Cal_bm 0x01   ///< bitmap for the CAL annunciator
#define K197_Omega_bm 0x02 ///< bitmap for the "Ω" annunciator
#define K197_A_bm 0x04     ///< bitmap for the "A" annunciator
#define K197_RMT_bm 0x20   ///< bitmap for the RMT annunciator

#define K197_RAW_MSG_SIZE 8 ///< raw message size = 6 digits + [sign] + null

/**************************************************************************/
/*!
   @brief auxiliary class to specify the axis labels of a graph

   @details This class is used to pass enough information to represent
   the data stored in the cache as a graph
*/
/**************************************************************************/
struct k197graph_label_type {
     int8_t mult;
     int8_t pow10;
     k197graph_label_type() : mult(0), pow10(0) {};
     k197graph_label_type(int8_t init_mult, int8_t init_pow10) : mult(init_mult), pow10(init_pow10) {};
     static float getpow10(int i);
     void setLog10Ceiling(float x);
     void setScaleMultiplierUp(float x);
     void setScaleMultiplierDown(float x);
     float getValue() const {return mult==0 ? 0.0 : float(mult)*getpow10(pow10);}; ///< return equivalent float value
     void setValue(int8_t new_mult, int8_t new_pow10) {mult=new_mult; pow10=new_pow10;}; ///< set new multiple and new power of 10
     void setValue(const k197graph_label_type l) {mult=l.mult; pow10=l.pow10;}; ///< set value from another object
     void reset() {mult=0; pow10=0;}; // set new multiple and new power of 10
     bool isNormalized() const {return ::abs(mult)<10 ? true : false;}; ///< check if normalized (-10<mult<+10)
     bool isPositive() const {return mult > 0;}; ///< check if value >0
     bool isNegative() const {return mult < 0;}; ///< check if value <0
     k197graph_label_type abs() const { ///< returns a new object with the absolute value of the original object 
         return k197graph_label_type(mult>0 ? mult : mult, pow10);
     };

     // overloading operators used in label scaling
     k197graph_label_type operator-() { ///< unary Minus (-) operator 
        return k197graph_label_type(-(this->mult), (this->pow10) );
     };
     k197graph_label_type &operator--() { ///< Prefix decrement (--) operator 
        --pow10;
        return *this;
     };
     k197graph_label_type &operator++() { ///< Postfix increment (++) operator 
        ++pow10;
        return *this;
     };
};

//Overloading common functions and operators useful in autoscaling
inline bool operator==(const k197graph_label_type& lhs, const k197graph_label_type& rhs) { ///< overloaded comparison (==) operator 
         if (lhs.isNormalized() && rhs.isNormalized()) return (lhs.mult == rhs.mult) && (lhs.pow10 == rhs.pow10);
         else return lhs.getValue() == rhs.getValue();
};
inline bool operator!=(const k197graph_label_type& lhs, const k197graph_label_type& rhs) { ///< overloaded comparison (!=) operator 
         if (lhs.isNormalized() && rhs.isNormalized()) return (lhs.mult != rhs.mult) || (lhs.pow10 != rhs.pow10);
         else return lhs.getValue() != rhs.getValue();
};
inline bool operator==(const float& lhs, const k197graph_label_type& rhs) { ///< overloaded comparison (==) operator 
         return lhs == rhs.getValue();
};
inline bool operator!=(const float& lhs, const k197graph_label_type& rhs) { ///< overloaded comparison (!=) operator 
         return lhs == rhs.getValue();
};
inline bool operator>(const k197graph_label_type& lhs, const k197graph_label_type& rhs) { ///< overloaded comparison (>) operator 
         return lhs.getValue() > rhs.getValue();
};

/**************************************************************************/
/*!
    @brief  Define the scaling options for the y axis
*/
/**************************************************************************/
enum k197graph_yscale_opt {
  k197graph_yscale_zoom =     0x00, ///< zoom the graph as much as possible
  k197graph_yscale_zero =     0x01, ///< always include zero in the graph
  k197graph_yscale_prefsym =  0x02, ///< If graph cross zero, the scale is symmetric
  k197graph_yscale_0sym =     0x03, ///< combine the previous two
  k197graph_yscale_forcesym = 0x04, ///< Always use a symmetric scale
};

/**************************************************************************/
/*!
   @brief  auxiliary class to store the graph

   @details This class is used to pass enough information to represent
   the data stored in the cache as a graph
*/
/**************************************************************************/
struct k197graph_type {
     static const byte x_size=180 ;
     static const byte y_size=63 ;
     byte point[x_size];
     byte current_idx=0x00;
     byte npoints=0x00;
     k197graph_label_type y1;
     k197graph_label_type y0;
     byte y_zero=0x00; ///< the point value for 0, if included in the graph

     void setScale(float grmin, float grmax, k197graph_yscale_opt yopt);
};

/**************************************************************************/
/*!
   @brief  class to store and manage the K197 information

   @details This class is responsible to store and decode the raw data 
   received through the base class SPIdevice, and make it available to the 
   rest of the sketch as structured information. It is not conceptually 
   complicated, but due to the large number of data and inline methods 
   it may seem that way.

   Usage:

   Call setup() before doing anything else. 
   The method hasNewData() in the base class should be called frequently
   (tipically in the main Arduino loop). As soon as hasNewData() returns true
   then getNewReading() must be called to decode and store the new batch of data
   from the K197/197A. 
   After that the new information is available via the other member functions
*/
/**************************************************************************/
class K197device : public SPIdevice {
private:
  struct devflags_struct {
    /*!
       @brief  A union is used to simplify initialization of the flags
       @return Not really a return type, this attribute will save some RAM
    */
    union {
      unsigned char value = 0x00; ///< allows acccess to all the flags in the
                                  ///< union as one unsigned char
      struct {
        bool tkMode : 1;         ///< show T instead of V (K type thermocouple)
        bool msg_is_num : 1;     ///< true if message is numeric
        bool msg_is_ovrange : 1; ///< true if overange detected
        bool hold : 1;           ///< true if the display is holding the value
      };
    } __attribute__((packed)); ///<
  }; ///< Structure designed to pack a number of flags into one byte
  devflags_struct flags; ///< holds a number of key flags

  char raw_msg[K197_RAW_MSG_SIZE]; ///< stores decoded sign + 6 char, no DP (0
                                   ///< term. char array)
  byte raw_dp = 0x00; ///< Stores the Decimal Point (bit 0 = not used, bit 1...7
                      ///< = DP bit for digit/char 0-6 of raw_msg)

  float msg_value = 0.0; ///< numeric value of message

  byte annunciators0 = 0x00; ///< Stores MINUS BAT RCL AC dB STO REL AUTO
  byte annunciators7 = 0x00; ///< Stores mA, k, V, u (micro), M, m (mV)
  byte annunciators8 = 0x00; ///< Stores RMT, A, Omega (Ohm), C

  /*!
      @brief  check if b has the decimal point set
      @param b byte with a 7 segment + Decimal Point (DP) code as received from
     the K197/197A
      @return true if DP was set, false otherwise
  */
  inline static bool hasDecimalPoint(byte b) { return (b & 0b00000100) != 0; };
  /*!
      @brief  check if a char is a digit or a space
      @param c the char to check
      @return true if the char represents a digit or a space
  */
  inline static bool isDigitOrSpace(char c) {
    if (isDigit(c))
      return true;
    if (c == CH_SPACE)
      return true;
    return false;
  };

  float tcold = 0.0; ///< temperature used for cold junction compensation

  void setOverrange();
  void tkConvertV2C();

public:
  /*!
      @brief  constructor for the class. Do not forget that setup() must be
     called before using the other member functions.
  */
  K197device() { raw_msg[0] = 0; };
  bool getNewReading();
  byte getNewReading(byte *data);

  /*!
      @brief  get display hold mode
      @return true if display hold mode is active, false otherwise
  */
  bool getDisplayHold() { return flags.hold; };

  /*!
      @brief  set display hold mode
      @param newValue true to enter display hold mode, false to exit
  */
  void setDisplayHold(bool newValue) { flags.hold = newValue; };

  /*!
      @brief  Return the raw message

      @return last raw message received from the K197/197A
  */
  const char *getRawMessage() { return raw_msg; };

  /*!
      @brief  check if there is a decimal point on the nth character in raw
     message

      This is useful because in a 7 segment display the digit position is fixed
     (obviously). If we printed the numerical on the display, the digits would
     move as the range change, since the decimal point is considered a character
     on its own by the display driver library. And this is annoying, especially
     with auto range.

      The display code can use getRawMessage() to display the digits/chars
     without the decimal point and then use isDecPointOn() to add a decimal
     point at the right place in between

      @param  char_n the char to check (allowed range: 0 to 7, however 0 always
     return false)
      @return true if the last raw_message received from the K197/197A had a
     decimal point at the nth character
  */
  bool isDecPointOn(byte char_n) { return bitRead(raw_dp, char_n); };

  const __FlashStringHelper *
  getUnit(bool include_dB = false); // Note: includes UTF-8 characters
  const __FlashStringHelper *
  getMainUnit(bool include_dB = false); // Note: includes UTF-8 characters
  int8_t getUnitPow10();

  /*!
      @brief  check if overange is detected
      @return true if overange is detected
  */

  bool isOvrange() { return flags.msg_is_ovrange; };

  /*!
      @brief  check if overange is detected
      @return true if overange is NOT detected
  */
  bool notOvrange() { return !flags.msg_is_ovrange; };

  /*!
      @brief  check if message is a number
      @return true if message is a number (false means something else, like
     "Err", "0L", etc.)
  */
  bool isNumeric() { return flags.msg_is_num; };

  /*!
      @brief  returns the measurement value if available
      @return returns the measurement value if available (isNumeric() returns
     true) or 0.0 otherwise
  */
  float getValue() { return msg_value; };

  void debugPrint();

private:  
    static const byte max_graph_size = 180; ///< maximum number of measurements that can be cached

  /*!
      @brief  structure used to store previus vales, average, max, min, etc.
   */
  struct {
  public:
    float avg_factor = 1.0 / 3.0; ///< Factor used in average calculation
    bool tkMode = false;   ///< caches tkMode from previous measurement
    float msg_value = 0.0; ///< caches msg_value from previous measurement
    byte annunciators0 =
        0x00; ///< caches annunciators0 from previous measurement
    byte annunciators7 =
        0x00; ///< caches annunciators7 from previous measurement
    byte annunciators8 =
        0x00; ///< caches annunciators8 from previous measurement

    float average = 0.0; ///< keep track of the average
    float min = 0.0;     ///< keep track of the minimum
    float max = 0.0;     ///< keep track of the maximum

    float graph[max_graph_size]; ///< stores gr_size records
                                 ///< when gr_size = max_graph_size
                                 ///< becomes a circular buffer
    byte gr_index=max_graph_size-1;///< index to the most recent record
    byte gr_size=0; ///< amount of data in graph (0-max_graph_size)
    byte nskip = 0;  ///< Skip counter for rolling average
    byte nsamples = 3; ///< Number of samples to use for rolling average

    uint16_t nskip_graph=0;      ///< Skip counter for graph
    uint16_t nsamples_graph = 0; ///< Number of samples to use for graph
    bool autosample_graph=false; ///< if true set nsamples_graph automatically 

    /*!
      @brief get the array index from logical index
      @details the logical index is 0 for the oldest record and increases as we get towards newer records
      The array index is the index in the circular buffer graph[] corresponding to the logical index
      @param logic_index the logical index (range: 0 - gr_size-1)
      @return arry index (range: 0 - gr_size-1)
    */
    inline uint16_t grGetArrayIdx(uint16_t logic_index) {return (logic_index+gr_index+1) % gr_size;};
    
    /*!
      @brief add one sample to graph
      @details Only one out of every nsamples is stored
      @param nsamples number of samples
    */
    void add2graph(float x) {
        if (nskip_graph == 0) { 
            gr_index++;
            if (gr_index >= max_graph_size) gr_index=0; 
            graph[gr_index]=x;
            if (gr_size < max_graph_size) gr_size++; 
        }
        if (++nskip_graph>=nsamples_graph) nskip_graph = 0;
    };
    /*!
      @brief reset graph
    */
    void resetGraph() { gr_index = max_graph_size-1; gr_size = 0x00; nskip = 0x00; };

  } cache;

  void updateCache();
  void resampleGraph(uint16_t nsampled_new);

public:
  void fillGraphDisplayData(k197graph_type *graphdata, k197graph_yscale_opt yopt);
  void troubleshootAutoscale(float testmin, float testmax);
  void resetStatistics();

  /*!
      @brief  set the number of samples for rolling average calculation

      @details Note that because this is a rolling average, the effect of a
     change is not immediate, it requires on the order of nsamples before it
     takes effect

      @param nsamples number of samples
  */
  void setNsamples(byte nsamples) { 
      cache.nsamples = nsamples; 
      cache.avg_factor = 1.0 / float(nsamples);
  };
  /*!
      @brief get the number of samples for rolling average calculation
      @return number of samples
  */
  float getNsamples() { return cache.nsamples; };

  /*!
      @brief  set the sampling period for the graph in seconds

      @details the actual sapling time will approximate the requested time, depending on the actual sampling rate of the K197.
      when set to zero, all samples received from the K197 are graphed (the data rate is about 3Hz)

      @param nseconds sampling time in seconds
  */
  void setGraphPeriod(byte nseconds) { 
      uint16_t nsamples_new = nseconds * 3;
      resampleGraph(nsamples_new); 
  };
  /*!
      @brief get the sampling period for the graph in seconds
      @details the actual sapling time will approximate the returned time, depending on the actual sampling rate of the K197.
      When zero is returned, all samples received from the K197 are graphed (the data rate is about 3Hz)
      @return number the sampling period in seconds
  */
  uint16_t getGraphPeriod() { return cache.nsamples_graph / 3; };

  /*!
      @brief  set the autosample flag

      @details when this flag is set, when the graph is full the sampling period is increased automatically
      data already sampled are decimated, so that they match the new sampling time 

      @param autosample the new value of the autosample flag
  */
  void setAutosample(bool autosample) { 
      cache.autosample_graph = autosample; 

  };
  /*!
      @brief get the autosample flag
      @details see setAutosample()
      @return the value of the autosample flag 
  */
  bool getAutosample() { return cache.nsamples_graph / 3; };

  /*!
      @brief  returns the average value (see also setNsamples())
      @return average value
  */
  float getAverage() { return cache.average; };

  /*!
      @brief  returns the minimum value
      @return minimum value
  */
  float getMin() { return cache.min; };

  /*!
      @brief  returns the maximum value
      @return maximum value
  */
  float getMax() { return cache.max; };

public:
  // annunciators0
  /*!
      @brief  test if AUTO is on
      @return returns true if on, false otherwise
  */
  inline bool isAuto() { return (annunciators0 & K197_AUTO_bm) != 0; };

  /*!
      @brief  test if REL is on
      @return returns true if on, false otherwise
  */
  inline bool isREL() { return (annunciators0 & K197_REL_bm) != 0; };
  /*!
      @brief  test if STO is on
      @return returns true if on, false otherwise
  */
  inline bool isSTO() { return (annunciators0 & K197_STO_bm) != 0; };
  /*!
      @brief  test if dB is on
      @return returns true if on, false otherwise
  */
  inline bool isdB() { return (annunciators0 & K197_dB_bm) != 0; };
  /*!
      @brief  test if AC is on
      @return returns true if on, false otherwise
  */
  inline bool isAC() { return (annunciators0 & K197_AC_bm) != 0; };
  /*!
      @brief  test if AC is on
      @return returns true if on, false otherwise
  */
  inline bool isDC() { return (annunciators0 & K197_AC_bm) == 0; };
  /*!
      @brief  test if RCL is on
      @return returns true if on, false otherwise
  */
  inline bool isRCL() { return (annunciators0 & K197_RCL_bm) != 0; };
  /*!
      @brief  test if BAT is on
      @return returns true if on, false otherwise
  */
  inline bool isBAT() { return (annunciators0 & K197_BAT_bm) != 0; };
  /*!
      @brief  test if "-" is on (negative number displayed)
      @return returns true if on, false otherwise
  */
  inline bool isMINUS() { return (annunciators0 & K197_MINUS_bm) != 0; };

  // annunciators7
  /*!
      @brief  test if "m" used for V is on
      @return returns true if on, false otherwise
  */
  inline bool ismV() { return (annunciators7 & K197_mV_bm) != 0; };
  /*!
      @brief  test if "M" is on
      @return returns true if on, false otherwise
  */
  inline bool isM() { return (annunciators7 & K197_M_bm) != 0; };
  /*!
      @brief  test if "µ" is on
      @return returns true if on, false otherwise
  */
  inline bool ismicro() { return (annunciators7 & K197_micro_bm) != 0; };
  /*!
      @brief  test if "V" is on
      @return returns true if on, false otherwise
  */
  inline bool isV() { return (annunciators7 & K197_V_bm) != 0; };
  /*!
      @brief  test if "k" is on
      @return returns true if on, false otherwise
  */
  inline bool isk() { return (annunciators7 & K197_k_bm) != 0; };
  /*!
      @brief  test if "m" used for A is on
      @return returns true if on, false otherwise
  */
  inline bool ismA() { return (annunciators7 & K197_mA_bm) != 0; };

  // annunciators8
  /*!
      @brief  test if CAL is on
      @return returns true if on, false otherwise
  */
  inline bool isCal() {
    if ((annunciators8 & K197_Cal_bm) != 0)
      return true;
    return false;
  }
  /*!
      @brief  test if CAL is off
      @return returns true if off, false otherwise
  */
  inline bool isNotCal() { return !isCal(); };
  /*!
      @brief  test if "Ω" is on
      @return returns true if on, false otherwise
  */
  inline bool isOmega() { return (annunciators8 & K197_Omega_bm) != 0; };
  /*!
      @brief  test if "A" is on
      @return returns true if on, false otherwise
  */
  inline bool isA() { return (annunciators8 & K197_A_bm) != 0; };
  /*!
      @brief  test if RMT is on
      @return returns true if on, false otherwise
  */
  inline bool isRMT() { return (annunciators8 & K197_RMT_bm) != 0; };

  // Extra modes/annnunciators not available on original K197

  /*!
      @brief  set Thermocuple mode
      @details when set, if the k197 is in DC mode and mV range, the mV reading
      is converted to a temperature value assuming a k thermocouple is connected
      @param mode true if enabled, false if disabled
  */
  void setTKMode(bool mode) { flags.tkMode = mode; }

  /*!
      @brief  get Thermocuple mode (see also setTKMode())
      @return returns true when K Thermocouple mode is enabled
  */
  bool getTKMode() { return flags.tkMode; }

  /*!
      @brief  check if the Thermocuple mode is active now
      @return returns true when K Thermocouple mode is enabled and active,
  */
  bool isTKModeActive() { return isV() && ismV() && flags.tkMode && isDC(); }

  /*!
      @brief  returns the temperature used for cold junction compensation
      @return temperature in celsius
  */
  float getTColdJunction() {
    return abs(tcold) < 999.99 ? tcold : 999.99; // Keep it in the display range
                                                 // just to be on the safe side
  }
};

extern K197device k197dev; ///< predefined device to use with this class

#endif // K197_DEVICE_H
