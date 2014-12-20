/*
 * Shaped Brightness Controller.h
 *
 * Created: 16/11/2014 10:29:44 PM
 *  Author: Adam Cooper
 */
/* 
 * ***Made available using the The MIT License (MIT)***
 * Copyright (c) 2014, Adam Cooper
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the �Software�), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED �AS IS�, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

/*! This is meant for use with Xmas Lights.
Purpose is to control LED brightness using PCA9685, where the brightness of each LED is
programmed to follow a particular pattern as "ticks" of time occur, additionally under
the influence of input values, that are expected to be dynamic (e.g. from user-controlled
pots, sound level sensor, LDR etc)

It keeps current values for all LEDs and manages the change of the output brightness. */

#ifndef SHAPED_BRIGHTNESS_CONTL_H
#define SHAPED_BRIGHTNESS_CONTL_H

#include <Arduino.h>
#include "PCA9685.cpp"

#ifndef SBC_MAX_LEDS
	#define SBC_MAX_LEDS 9 //Number of LEDS for output. Defaults to 9 but may be defined to a different value in code that includes this library
#endif


/** @name SBC_WAVESHAPE_*
Control the waveform shape */
//!@{
#define SBC_WAVESHAPE_OFF 0 //!< The LED is always off, for the whole period
#define SBC_WAVESHAPE_SAW 1 //!< Ramping up saw tooth
#define SBC_WAVESHAPE_TRI 2 //!< Triangle
#define SBC_WAVESHAPE_SQU 3 //!< Square 50/50 mark/space
#define SBC_WAVESHAPE_PULSE 4 //!< 20/80 mark space pulse wave
#define SBC_WAVESHAPE_HUMP 5 //!< Rounded top hump (not quite sine)
#define SBC_WAVESHAPE_SPIKE 6 //!< Accelerating rise and fall (spikier than a triangle)
#define SBC_WAVESHAPE_ON 7 //!< The LED is always on, for the whole period
//!@}

/** @name SBC_WSMOD_*
Various ways of modifying the waveform shape as set by SBC_WAVESHAPE_* - add to it to create bit-mask to pass to setPattern() */
//!@{
#define SBC_WSMOD_INVERT 8 //!< The output is flipped.
#define SBC_WSMOD_MISS1 16 //!< after one cycle of the wave shape (determined by the internal counter),  spend one cycle waiting
#define SBC_WSMOD_MISS2 32 //!< after one cycle of the wave shape (determined by the internal counter),  spend two cycles waiting
//!@}

/** @name SBC_TG_*
Control trigger/gate behaviour - add to SBC_WAVESHAPE_* to create bit-mask to pass to setPattern(). SBC_TG_1SHOT takes priority*/
//!@{
#define SBC_TG_GATECHANGE 0 //!< This is default behaviour; the trigger/gate input will determine whether the LED brightness will change. (if no gate input, it remains fixed at the last value)
#define SBC_TG_GATEOUT 64 //!< The Trigger/gate input over-rides the output. If below threshold then the LED will be off, otherwise as controlled by the waveshape/rate/etc. 
#define SBC_TG_1SHOT 128 //!< The output will stay off (or on, if SBC_INVERT) until the trigger intput >=512. Phase is irrelevant in this mode.
//!@}

/*! Doxygen documentation about class Shaped Brightness Controller
 @brief ... brief desc of Shaped Brightness Controller. */
class ShapedBrightnessController{
public:
  /** @name Constructors and initialisation */
  //!@{
  /*! Basic constructor. Just sets things up with all patterns set to SBC_WAVESHAPE_OFF and prescale=0.
  Zeros-out stored rate, scale and triggerIP values. NB: you must call the initialise() method to set up the PWM output device.
  @param numLeds Actual number of LEDs in use. Forced to be <=SBS_MAX_LEDS*/
  ShapedBrightnessController(uint8_t numLeds);
  
  /*! Initilise PWM. Must be called after constructor and before real use. */
  void initialise();
  
  //!@}

  /** @name Routine Methods */
  //!@{
  /*! This should be called for each "tick" of time, expected to be close to 16Hz, but different values will just
  mean the time period variables get re-scaled. This re-computes all of the output values and sends 
  brightnesses to the PCA9685 using an internal counter (see setRate()), shape settings, and control values. */
  void tick();
  
  /*! Set rate of change of the internal counter (range 0-2047). The value of the internal counter is
  used to compute a base output brightness, according to the defined output ("wave") shape. 
  This is one of three control values.
  In general use, these should be updated every 1 to 16 ticks using some ADC (etc) input.
  @param led the zero-based index of the output LED to set the rate for 
  @param val a value in the range 0-1023 to be used to update the internal counter */
  void setRate(uint8_t led, int val);

  /*! Set max brightness of the LED. 
  This is one of three control values. In general use, these should be updated every 1 to 16 ticks using some ADC (etc) input.
  Unless set, the scale defaults to 0 - i.e. off
  @param led the zero-based index of the output LED to set the rate for 
  @param val a value in the range 0-1023 to be used for the brightness (1023=full on) */
  void setScale(uint8_t led, int val);
  
    /*! Set trigger (for one shot) or gate (otherwise).  Value >=512 to trigger or open gate.
	If one shot applies, the value is reset to zero when the one-shot ends, to avoid erroneous re-triggering
  This is one of three control values. In general use, these should be updated every 1 to 16 ticks using some ADC (etc) input.
  @param led the zero-based index of the output LED to set the rate for 
  @param val a value in the range 0-1023 to be used to check for trigger/gate-open */
  void setTriggerIP(uint8_t led, int val);

  //!@}

  /** @name Output Pattern Setup */
  //!@{
  /*! Set the pattern for a LED. This is for a single output shape
  @param led the zero-based index of the output LED to set the rate for 
  @param shape controls the shape via a sum of pre-defined values SBC_WAVESHAPE_* + optionally SBC_WSMOD_* to make a bitmask
  @param prescale number of places to right bit-shift the rate set by setRate(). Normally use 0. Different values may be useful when same ADC (etc) input is used to control the rate of several LEDs
  @param phase phase offset for this LED. A value in the range 0-2047 that defines the starting value for the internal counter.*/
  void setPattern(uint8_t led, uint8_t shape, uint8_t prescale, int phase);
  //!@}

  /** @name Byte-serialisation of program */
  //!@{
  /*! Gets the sequence of bytes that encodes the pattern program for a LED
  @param led the zero-based index of the output LED to set the rate for 
  @param patternProg Group of 4 bytes to save to EEPROM etc.*/
  void getPatternProgBytes(uint8_t led, byte patternProg[4]);

  /*! Sets the program using a sequence of bytes that encodes the pattern program for a LED
  The bytes contain {shape,prescale,highByte(phase),lowByte(phase)}
  @param led the zero-based index of the output LED to set the rate for 
  @param patternProg Group of 4 bytes, as obtained from getPatternProgBytes().*/
  void setPatternFromProgBytes(uint8_t led, uint8_t patternProg[4]);
  //!@}

private:
  //private member variables
  uint8_t _numLeds;//actual number of LEDs. Forced to be <=SMC_MAX_LEDS
  int _counter[SBC_MAX_LEDS]; //the counter for each LED is a bit like the x axis scan on an oscilloscope
  int _phase[SBC_MAX_LEDS];
  uint8_t _shape[SBC_MAX_LEDS]; //stores the output patterns for each LED
  uint8_t _prescale[SBC_MAX_LEDS]; //stores the prescale values for each LED
  int _rate[SBC_MAX_LEDS];//last value set by setRate()
  int _scale[SBC_MAX_LEDS];//max PWM value for each LED NB:0-255 range. = last value set by setScale()>>2
  int _triggerIP[SBC_MAX_LEDS];//last value set by setTriggerIP(). Used for triggering one-shots AND for gating output.
  uint8_t _cycle[SBC_MAX_LEDS];//counts how many cycles of _counter[i] - used when SBC_WSMOD_MISSn in force
  boolean _changeEnable[SBC_MAX_LEDS];//true if one-shot has been triggered and is in progress or if gate open for non-1-shot
  
  PCA9685 _pwm;
  
  //private methods
};



#endif //include guard