#include "WProgram.h"
#include <SPI.h>
#include "TMC262Stepper.h"

//TMC262 register definitions
#define DRIVER_CONTROL_REGISTER 0x0ul
#define CHOPPER_CONFIG_REGISTER 0x80000ul
#define SMART_ENERNGY_REGISTER  0xA0000ul
#define STALL_GUARD2_LOAD_MEASURE_REGISTER 0xC0000ul
#define DRIVER_CONFIG_REGISTER 0xE0000ul

#define REGISTER_BIT_PATTERN 0xFFFFFul

//definitions for the driver control register
#define MICROSTEPPING_PATTERN 0xFul
#define STEP_INTERPOLATION 0x200ul
#define DOUBLE_EDGE_STEP 0x100ul

//definitions for the chopper config register
#define CHOPPER_MODE_STANDARD 0x0ul
#define CHOPPER_MODE_T_OFF_FAST_DECAY 0x4000ul

#define RANDOM_TOFF_TIME 0x200ul
#define BLANK_TIMING_SHIFT 15
#define HYSTERESIS_DECREMENT_PATTERN 0x1800ul
#define HYSTERESIS_DECREMENT_SHIFT 11
#define HYSTERESIS_LOW_VALUE_PATTERN 0x780ul
#define HYSTERESIS_LOW_SHIFT 7
#define HYSTERESIS_START_VALUE_PATTERN 0x70ul
#define HYSTERESIS_START_VALUE_SHIFT 4
#define T_OFF_TIMING_PATERN 0xFul

//definitions for cool step register
#define MINIMUM_CURRENT_FOURTH 0x8000ul
#define CURRENT_DOWN_STEP_SPEED_PATTERN 0x6000ul
#define SE_MAX_PATTERN 0xF00ul
#define SE_CURRENT_STEP_WIDTH_PATTERN 0x60ul
#define SE_MIN_PATTERN 0xful

//definitions for stall guard2 current register
#define STALL_GUARD_FILTER_ENABLE 0x10000ul
#define STALL_GUARD_TRESHHOLD_VALUE_PATTERN 0x7F00ul
#define CURRENT_SCALING_PATTERN 0x1Ful

//default values
#define INITIAL_MICROSTEPPING 0x3ul //32th microstepping


//debuging output
#define DEBUG

TMC262Stepper::TMC262Stepper(int number_of_steps, int cs_pin, int dir_pin, int step_pin, unsigned int max_current)
{
	//save the pins for later use
	this->cs_pin=cs_pin;
	this->dir_pin=dir_pin;
	this->step_pin = step_pin;
	//initialize register values
	driver_control_register_value=DRIVER_CONFIG_REGISTER | INITIAL_MICROSTEPPING;
	chopper_config_register=CHOPPER_CONFIG_REGISTER;
	//calculate the current scaling from the max current setting (in mA)
	float mASetting = max_current;
	//this is derrived from I=(cs+1)/32*Vfs/Rsense*1/sqrt(2)
	//with vfs=5/16, Rsense=0,15
	//giving the formula CS=(ImA*32/(1000*k)-1 where k=Vfs/Rsense*1/sqrt(2) - too lazy to deal with complete formulas
	this->current_scaling = (byte)((mASetting*0.0217223203180507)-0.5); //theoretically - 1.0 for better rounding it is 0.5
}

void TMC262Stepper::start() {

#ifdef DEBUG	
	Serial.println("TMC262 stepper library");
	Serial.print("CS pin: ");
	Serial.println(cs_pin);
	Serial.print("DIR pin: ");
	Serial.println(dir_pin);
	Serial.print("STEP pin: ");
	Serial.println(step_pin);
	Serial.print("current scaling: ");
	Serial.println(current_scaling,DEC);
#endif
	//set the pins as output & its initial value
	pinMode(step_pin, OUTPUT);     
	pinMode(dir_pin, OUTPUT);     
	pinMode(cs_pin, OUTPUT);     
	digitalWrite(step_pin, LOW);     
	digitalWrite(dir_pin, LOW);     
	digitalWrite(cs_pin, HIGH);   
	
	
	SPI.setBitOrder(MSBFIRST);
	SPI.setClockDivider(SPI_CLOCK_DIV8);
	SPI.setDataMode(SPI_MODE0);
	SPI.begin();
		
	send262(DRIVER_CONTROL_REGISTER|INITIAL_MICROSTEPPING); 
	send262(CHOPPER_CONFIG_REGISTER | (3ul<<BLANK_TIMING_SHIFT) | CHOPPER_MODE_T_OFF_FAST_DECAY | (3ul<<HYSTERESIS_LOW_SHIFT) | (5ul << HYSTERESIS_START_VALUE_SHIFT) | 7); // was 0x941D7
	send262(SMART_ENERNGY_REGISTER);
	send262(STALL_GUARD2_LOAD_MEASURE_REGISTER|current_scaling);
	send262(DRIVER_CONFIG_REGISTER);
	
}

/*
  Sets the speed in revs per minute

*/
void TMC262Stepper::setSpeed(long whatSpeed)
{
  this->step_delay = 60L * 1000L / this->number_of_steps / whatSpeed;
}

/*
  Moves the motor steps_to_move steps.  If the number is negative, 
   the motor moves in the reverse direction.
 */
void TMC262Stepper::step(int steps_to_move)
{  
  int steps_left = abs(steps_to_move);  // how many steps to take
  
  // determine direction based on whether steps_to_mode is + or -:
  if (steps_to_move > 0) {this->direction = 1;}
  if (steps_to_move < 0) {this->direction = 0;}
    
    
  // decrement the number of steps, moving one step each time:
  while(steps_left > 0) {
  // move only if the appropriate delay has passed:
  if (millis() - this->last_step_time >= this->step_delay) {
      // get the timeStamp of when you stepped:
      this->last_step_time = millis();
      // increment or decrement the step number,
      // depending on direction:
      if (this->direction == 1) {
		  digitalWrite(step_pin, HIGH);
      } 
      else { 
		  digitalWrite(dir_pin, HIGH);
		  digitalWrite(step_pin, HIGH);
      }
      // decrement the steps left:
      steps_left--;
	  //disable sthe step & dir pins
	  delay(20);
	  digitalWrite(step_pin, LOW);
	  digitalWrite(dir_pin, LOW);
    }
  }
}

/*
  version() returns the version of the library:
*/
int TMC262Stepper::version(void)
{
  return 1;
}

unsigned long TMC262Stepper::send262(unsigned long datagram) {
	unsigned long i_datagram;

	//ensure that only valid bist are set (0-19)
	//datagram &=REGISTER_BIT_PATTERN;

#ifdef DEBUG
	Serial.print("Sending ");
	Serial.println(datagram,HEX);
#endif
	
	//select the TMC driver
	digitalWrite(cs_pin,LOW);
	
	//write/read the values
	i_datagram = SPI.transfer((datagram >> 16) & 0xff);
	i_datagram <<= 8;
	i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
	i_datagram <<= 8;
	i_datagram |= SPI.transfer((datagram      ) & 0xff);
	i_datagram >>= 4;
	
	//deselect the TMC chip
	digitalWrite(cs_pin,HIGH); 
#ifdef DEBUG
	Serial.print("Received ");
	Serial.println(i_datagram,HEX);
#endif
	
	return i_datagram;
}

void TMC262Stepper::setMicrostepping(int setting) {
	long setting_pattern;
	//poor mans log
	if (setting>=256) {
		setting_pattern=0;
	} else if (setting>=128) {
		setting_pattern=1;
	} else if (setting>=64) {
		setting_pattern=2;
	} else if (setting>=32) {
		setting_pattern=3;
	} else if (setting>=16) {
		setting_pattern=4;
	} else if (setting>=8) {
		setting_pattern=5;
	} else if (setting>=4) {
		setting_pattern=6;
	} else if (setting>=2) {
		setting_pattern=7;
    //1 and 0 lead to full step
	} else if (setting<=1) {
		setting_pattern=8;
	}
	//delete the old value
	this->driver_control_register_value &=0xFFFF0;
	//set the new value
	this->driver_control_register_value |=setting_pattern;
	send262(driver_control_register_value);
}