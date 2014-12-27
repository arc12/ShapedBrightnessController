/*
* Shaped Brightness Controller.cpp
*
* Created: 16/11/2014 10:29:44 PM
*  Author: Adam
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

#include <avr/io.h>
#include "ShapedBrightnessController.h"

//constructor
ShapedBrightnessController::ShapedBrightnessController(uint8_t numLeds){
	if(numLeds<SBC_MAX_LEDS){
		_numLeds = numLeds;
	}else{
		_numLeds=SBC_MAX_LEDS;
	}
	for(int i=0; i<_numLeds;i++){
		_counter[i]=0;
		_shape[i]=0;
		_rate[i]=0;
		_scale[i]=0;
		_triggerIP[i]=0;
		_cycle[i]=0;
		_changeEnable[i]=false;
	}
}

void ShapedBrightnessController::initialise(){
		_pwm = PCA9685();
		//initialise for driving NMOS using the all-call I2C address.
		_pwm.initialise(PCA9685_DRIVE_NMOS);
}
  

//realised as a series of loops for ease of reading, even tho less efficient
void ShapedBrightnessController::tick(){
	
	//check if there are any one-shot patterns, and if so, should they be triggered
	//if not one-shot then the _changeEnable[] is set to indicate whether the gate is currently open
	//these have the same effect on the way the LED updates brightness, but for one-shot the effect latches
	//whereas for gating it does not. Note the default gate does not over-ride brightness, it controls tick() updating
	// (but SBC_TG_GATEOUT does cause output over-riding in addition to blocking brightness updating, so when the output
	// gate opens again, the same brightness should pertain)
	for(int i=0; i<_numLeds;i++){
		if(_shape[i]&SBC_TG_1SHOT){
			_changeEnable[i]=_changeEnable[i] || ((_triggerIP[i]>=512) && (_prevTriggerIP[i]<512));
			_prevTriggerIP[i] = _triggerIP[i];
			}else{
			_changeEnable[i]=_triggerIP[i]>=512;
		}
	}

	//update the "internal counter" - equiv to time
	int r;
	int c;
	boolean cycled = false; //set to true when counter goes "over the top"
	for(int i=0; i<_numLeds;i++){
		if(_changeEnable[i]){
			r=_rate[i];
			c=_counter[i]+r;
			//when the internal counter reaches its max, a cycle ends, which has various effects!
			//_cycle is used for MISSn
			if(c>=2048){
				c-=2048;
				cycled = true;
				if((_shape[i]&(SBC_WSMOD_MISS2+SBC_WSMOD_MISS1))>0){
					uint8_t cycleLength = ((_shape[i]&(SBC_WSMOD_MISS2))?2:0)+((_shape[i]&(SBC_WSMOD_MISS1))?1:0);
					if(_cycle[i]==cycleLength){
						_cycle[i]=0;
						if(_shape[i]&SBC_TG_1SHOT){
							_changeEnable[i]=false;//end of one shot after a "hit" + 2 "miss" cycles
						}
					}else{
						_cycle[i]++;
					}
				}else{
					_cycle[i]=0;
					if(_shape[i]&SBC_TG_1SHOT){
						_changeEnable[i]=false;//end of one shot in non-hit-and-miss situation
					}
				}		
			}
			_counter[i]=c;
		}
	}
	
	// Loop over all LEDs and compute the output value, taking account of lots of factors!
	long base=0;
	int output;
	for(int i=0; i<_numLeds;i++){
		boolean updateOut = true;//gets set to false for RND when no change is due, to suppress output value updating
					
		// output override to 0; LED will be OFF in three situations
		//  a) if TG_1SHOT is in force and the "shot" has ended
		//  b) if TG_GATEOUT is in force (but not if 1-shot set... it overrides gate flags)
		//  c) if MISS2 or MISS3 is set, and the cycle counter indicates we are in a "miss" if cycle!=0
		if( ( (_shape[i]&SBC_TG_1SHOT) && !_changeEnable[i]) ||
			( ( (_shape[i]&(SBC_TG_1SHOT+SBC_TG_GATEOUT)) == SBC_TG_GATEOUT) && !_changeEnable[i])||
			(_cycle[i]!=0) ){
				//( (_shape[i]&SBC_WSMOD_MISS1) && (_cycle[i]==1) ) || 	( (_shape[i]&SBC_WSMOD_MISS2) && (_cycle[i]<=2) )
				output = 0;
			}else{
			//calculate the "base" output value according to the specified wave-form,
			// later adjusted by INVERT, MISSn
			
			//first get "base" output value (pre-scaling, pre-inversion)
			switch (_shape[i]&0x07){
				case SBC_WAVESHAPE_OFF:
					base = 0;
					break;
				case SBC_WAVESHAPE_SAW:
					base=_counter[i]>>3;
					break;
				case SBC_WAVESHAPE_TRI:
					if(_counter[i]<1024){
						base=_counter[i]>>2;
						}else{
						base=(2047-_counter[i])>>2;
					}
					break;
				case SBC_WAVESHAPE_SQU:
					if(_counter[i]<1024){
						base=255;
						}else{
						base=0;
					}
					break;
				case SBC_WAVESHAPE_PULSE:
					if(_counter[i]<410){
						base=255;
						}else{
						base=0;
					}
					break;
				case SBC_WAVESHAPE_SPIKE:
					if(_counter[i]<1024){
						base=pow(_counter[i]>>6,2);
						}else{
						base=pow(31 - (_counter[i]>>6),2);
					}
					if(base>255) base=255;
					break;
				case SBC_WAVESHAPE_RND:
					if(cycled){
						base = random(255);
					}else{
						updateOut = false;
					}
					break;
				case SBC_WAVESHAPE_ON:
					base= 255;
					break;
				default:
					base=0;
			}
			
			//now do scaling and inversion
			output = (base*(long)_scale[i])>>8;
			if(_shape[i] & SBC_WSMOD_INVERT){
				output = 255-output;
			}
			
			//put a floor boost in to keep LEDs glimmering even at low end, but only when no output=0 override
			if(output==0) output = 1;
			
		}//end of output=0 over-ride if block
		
		//send to the PWM
		if(updateOut){
			_pwm.SetBrightness(output,i);
		}
		
		//if(i==2){
			//Serial.print(i);
			//Serial.print(" ");
			//Serial.println(output);
		//}
	}
}


void ShapedBrightnessController::setRate(uint8_t led, int val){
	led=led%_numLeds;
	_rate[led] = val>>2;
}

void ShapedBrightnessController::setScale(uint8_t led, int val){
	led=led%_numLeds;
	_scale[led]=val>>2;
}

void ShapedBrightnessController::setTriggerIP(uint8_t led, int val){
	led=led%_numLeds;
	_triggerIP[led]=val;
}

void ShapedBrightnessController::setPattern(uint8_t led, uint8_t shape, int phase){
	led=led%_numLeds;
	_shape[led]=shape;
	phase=phase%2048;
	_phase[led]=phase;
	_counter[led]=phase;
	_cycle[led]=0;
	_changeEnable[led]=false;
}


void ShapedBrightnessController::getPatternProgBytes(uint8_t led, uint8_t patternProg[4]){
	patternProg[0]=_shape[led];
	patternProg[1]=0;
	patternProg[2]=highByte(_phase[led]);
	patternProg[3]=lowByte(_phase[led]);
}

void ShapedBrightnessController::setPatternFromProgBytes(uint8_t led, uint8_t patternProg[4]){
	setPattern(led, patternProg[0], (((int)patternProg[2])<<8) + patternProg[3]);
}