/*
  Author:   Mitchell C. Nelson
  Date:     March 8, 2024
  Contact:  drmcnelson@gmail.com
  This work is a derivative of the following works:
  
    RAWHIDfirmware.ino   Mitchell C. Nelson,  June 18, 2021

    TCD1304_201223.ino   Mitchell C. Nelson,  Aug 10 through Dec 23, 2020

  Permissions:

     Permission is hereby granted for non-commercial use.

     For commerical uses, please contact the author at the above email address.

  Provisos:

  1) No representation is offered or assumed of suitability for any purposee whatsoever.

  2) Use entirely at your own risk.

  3) Please cite appropriately in any work using this code.

 */

#include "Arduino.h"

//#include <digitalWriteFast.h>
//#include <elapsedMillis.h>  // uncomment this if not Teensy

/* Note self, use this for portability to Arduinos
   
   attachInterrupt( digitalPinToInterrupt( pin_number ), pin_ISR, CHANGE);

*/

#include <SPI.h>

#include <limits.h>

#include <string.h>

#include "S1163901_SPI.h"

#include "eepromlib.h"

//#define TESTING

/* =====================================================================
   Fast ISR support
 */

#define FASTISR

#ifdef FASTISR

#include "imxrt.h"
#include "pins_arduino.h"
#define DR_INDEX    0
#define GDIR_INDEX  1
#define PSR_INDEX   2
#define ICR1_INDEX  3
#define ICR2_INDEX  4
#define IMR_INDEX   5
#define ISR_INDEX   6
#define EDGE_INDEX  7

volatile uint32_t *directgpio;
volatile uint32_t directmask = 0;

inline void directAttach( uint8_t pin, void (*function)(void), int mode ) {
  directgpio = portOutputRegister(pin);
  directmask = digitalPinToBitMask(pin);
  attachInterrupt(pin, function, mode);
  attachInterruptVector(IRQ_GPIO6789, function);
}

inline uint32_t directClear( ) {
  uint32_t status = directgpio[ISR_INDEX] & directgpio[IMR_INDEX];
  if (status) {
    directgpio[ISR_INDEX] = status;
  }
  return status & directmask;
}

void directnoop() {
}

inline void directDetach(uint8_t pin) {
  detachInterrupt(pin);
  attachInterrupt(pin,directnoop,RISING);
  detachInterrupt(pin);
}

#endif

// ==================================================================
// CPU Cycles per Usec
#define CYCLES_PER_USEC (F_CPU / 1000000)

#define NANOSECS_TO_CYCLES(n) (((n)*CYCLES_PER_USEC)/1000)

// ==================================================================
// SPI maximum clock per the ARM7 documents
#define SPI_SPEED 30000000
SPISettings spi_settings( SPI_SPEED, MSBFIRST, SPI_MODE0);   // 30 MHz, reads 1.5usecs/word including CNVST

#define SPI_TO_USECS(nbits) ((nbits*1000)/(SPI_SPEED/1000))
#define USECS_TO_SPI(usecs) ((usecs*(SPI_SPEED/1000))/1000)

// ==================================================================
// ADC requires 730ns sampling, so this is about as good as it gets
#ifdef FASTISR
#define MASTER_CLOCK 600000
#else
#define MASTER_CLOCK 500000
#endif
const unsigned long fM  = MASTER_CLOCK;   // Safe setting, max is 666667 (666.667kHz).

#define CLOCKS_TO_USECS(nclks) ((nclks*1000)/(MASTER_CLOCK/1000))
#define USECS_TO_CLOCKS(usecs) ((usecs*(MASTER_CLOCK/1000))/1000)


// the actual value works to 4.7064 msecs, set it to 5msecs
// SPI 2kW*16/20MHz = 1.1msec
//
#define MY_USB_SPEED 480000000
#define USB_TO_USECS(nbits) ((nbits*1000)/(MY_USB_SPEED/1000))
#define USECS_TO_USB(usecs) ((usecs*(MY_USB_SPEED/1000))/1000)

#define TRANSFERUSECS_CALC (CLOCKS_TO_USECS((NREADOUT + 100))+(USB_TO_USECS((NREADOUT*16+1024))))
#define TRANSFERUSECS_MIN 4000
#define TRANSFERUSECS ((TRANSFERUSECS_CALC>TRANSFERUSECS_MIN)?TRANSFERUSECS_CALC:TRANSFERUSECS_MIN)

#define EXPOSUREMIN (CLOCKS_TO_USECS(50))
#define FRAMEMIN (TRANFSERUSECS + CLOCKS_TO_USECS(110))

// ==================================================================
const char sensorstr[] =  "S11639-01-SPI";
const char versionstr[] = "T4LCD vers 0.3";
const char authorstr[] =  "Copyright (c) 2024 by Mitchell C. Nelson, Ph.D. ";

// External trigger, sync 
const int syncPin = 0;        // On shutter, start or start followed by delay - for desktop N2 laser
const int busyPin = 1;        // Gate output pin, goes high during shutter
const int interruptPin = 2;   // Trigger input pin
const int sparePin =  3;      // Spare pin for digital output

// Clock and logic to the ILX511 sensor
const int fMPin   =  4;         // Mclk,  FTM1 is 3, 4
const int fMPinMonitor   =  5;  // Mclk monitor
const int STPin   =  6;         // start pin
const int TRGPin  = 7;          // trig to start conversion
const int EOSPin = 8;           // Spare pin to the sensor board

// SPI interface to the ADC
const int CNVSTPin = 9;
const int SDIPin = 11;
const int SDOPin = 12;
const int CLKPin = 13;

// Pin states
int intedgemode = RISING;        // Default trigger mode
unsigned int busyPinState = LOW;
unsigned int syncPinState = HIGH;
unsigned int sparePinState = LOW;

// running toggled states
bool sync_toggled = false;
bool busy_toggled = false;

#define NUMBEROFPINS 24

// -----------------------------------------------
// Register level pin access

#define CLOCKREAD (CORE_PIN5_PINREG & CORE_PIN5_BITMASK)

/*
#define EOSREAD (CORE_PIN8_PINREG & CORE_PIN8_BITMASK)

#define SETCNVST (CORE_PIN9_PORTSET = CORE_PIN9_BITMASK)
#define CLEARCNVST (CORE_PIN9_PORTCLEAR = CORE_PIN9_BITMASK)

#define SETBUSYPIN (CORE_PIN1_PORTSET = CORE_PIN1_BITMASK)
#define CLEARBUSYPIN (CORE_PIN1_PORTCLEAR = CORE_PIN1_BITMASK)
#define TOGGLEBUSYPIN (CORE_PIN1_PORTTOGGLE = CORE_PIN1_BITMASK)

#define SETSYNCPIN (CORE_PIN2_PORTSET = CORE_PIN2_BITMASK)
#define CLEARSYNCPIN (CORE_PIN2_PORTCLEAR = CORE_PIN2_BITMASK)
#define TOGGLESYNCPIN (CORE_PIN2_PORTTOGGLE = CORE_PIN2_BITMASK)
*/
// -----------------------------------------------

inline const char *boolstr(bool val) {
  return val?"true":"false";
}

const char *isrstatenames[] = {"ready","counting","lower_st","readout","checkeos","unknown"};
inline const char *isrstatestring(unsigned int n) {
  n = (n > 4)? 5:n;
  return isrstatenames[n];
}

const char *formatnames[] = {"binary","ascii","float","unknown"};
inline const char *dataformatstring(unsigned int n) {
  n = (n > 3)? 4:n;
  return formatnames[n];
}

// -----------------------------------------------

// Pulse pin agnostic to initial state
inline void pulsePin( unsigned int pin, unsigned int usecs ) {
  digitalToggleFast(pin);
  delayMicroseconds(usecs);
  digitalToggleFast(pin);
}

// Pulse pin to specific state
inline void pulsePinHigh( unsigned int pin, unsigned int usecs ) {
  digitalWriteFast(pin,HIGH);
  delayMicroseconds(usecs);
  digitalWriteFast(pin,LOW);
}

inline void pulsePinLow( unsigned int pin, unsigned int usecs ) {
  digitalWriteFast(pin,LOW);
  delayMicroseconds(usecs);
  digitalWriteFast(pin,HIGH);
}
// -----------------------------------------------
// pwm on spare pin
bool spare_pwm = false;
uint32_t pwm_resolution = 8;
uint32_t pwm_value = 4;
float pwm_frequency = 0.;

void sparePWM_stop() {
  pinMode(sparePin,OUTPUT);
  digitalWriteFast(sparePin, sparePinState);
  spare_pwm = false;
}

void sparePWM_start() {

  float f;
  
  f = F_CPU/(1<<pwm_resolution);
  f /= 4;

  pwm_frequency = f;

  Serial.print( "PWM resolution " );
  Serial.println( pwm_resolution );
  analogWriteResolution(pwm_resolution);  // only for the next call
	
  Serial.print( "PWM Frequency " );
  Serial.println( f );
  analogWriteFrequency(sparePin, f);


  Serial.print( "PWM value " );
  Serial.println( pwm_value );
  analogWrite(sparePin,pwm_value);
	
  spare_pwm = true;
}

/* ===================================================================================================
   Raw data and send buffers
*/
// needs to be a power of 2, always
#define NBUFFERS 16
uint16_t buffers[NBUFFERS][NREADOUT] = { { 0 } };

// manual buffer selection
uint16_t *bufferp = &buffers[0][0];
unsigned int buffer_index = 0;

// Format and handshake controls
unsigned int dataformat = BINARY;
bool data_async = true;
bool crc_enable = false;
bool sum_enable = false;

// Latching
bool do_latch = false;
unsigned int latch_value = 0;
unsigned int latch_pixel = 0;

// Buffer management - this one is private
uint16_t *setBuffer_(unsigned int isel) {
  buffer_index = isel % NBUFFERS;
  bufferp = &buffers[buffer_index][0];
  return bufferp;
}

uint16_t *initBufferSelect() {
  return setBuffer_(0);
}

uint16_t *decrementBufferSelect() {
  unsigned int isel;
  isel = buffer_index ? (buffer_index-1) : NBUFFERS-1;
  return setBuffer_( isel );
}

uint16_t *incrementBufferSelect() {
  return setBuffer_( (buffer_index+1) );
}

uint16_t *selectBuffer( unsigned int isel ) {
  if (isel < NBUFFERS ) {
    return setBuffer_(isel);
  }
  return NULL;
}

uint16_t *currentBuffer( ) {
  return bufferp;
}

/* ------------------------------------------------
   Not implemented yet, ring buffering
*/
#ifdef RINGBUFFERING

Frame_t frameRing[NBUFFERS] = { {0} };
uint16_t readout_index = 0 ;
uint16_t send_index = 0 ;

inline uint16_t *getReadoutFrame() {
  Frame_t *p = &frameRing[readout_index][0];
  readout_index = (readout_index+1)&(NBUFFER-1);
  return p;
}

inline uint16_t *getSendBuffer() {
  uint16_t *p = &frameRing[send_index][0];
  send_index = (send_index+1)&(NBUFFER-1);
  return p;
}

#endif

/* ==========================================================================
   latch functions
*/

void clearLatch() {
  do_latch = false;
  latch_value = 0;
  Serial.println( "latch cleared" );      
}

bool latchupdate( ) {
  uint16_t *p16 = &bufferp[DATASTART];
  uint16_t uval = 0;
  uint16_t umax = 0;
  int nmax = 0;
  int n;
  
  if (latch_pixel>0) {
    uval = p16[latch_pixel];
    nmax = latch_pixel;
  }

  else {
    umax = 0;
    for ( n = 0; n < NPIXELS; n++ ) {
      uval = *p16++;
      if ( uval > umax ) {
	umax = uval;
	nmax = n;
      }
    }
  }
    
  if ( umax > latch_value) {
    latch_value = umax;
    Serial.print( "latched " );
    Serial.print( nmax );
    Serial.print( " " );
    Serial.println( latch_value );
    return true;
  }
  
  return false;
}

/* ==========================================================================
 */
unsigned char crctable[256] = {
  0x00,0x31,0x62,0x53,0xc4,0xf5,0xa6,0x97,0xb9,0x88,0xdb,0xea,0x7d,0x4c,0x1f,0x2e,
  0x43,0x72,0x21,0x10,0x87,0xb6,0xe5,0xd4,0xfa,0xcb,0x98,0xa9,0x3e,0x0f,0x5c,0x6d,
  0x86,0xb7,0xe4,0xd5,0x42,0x73,0x20,0x11,0x3f,0x0e,0x5d,0x6c,0xfb,0xca,0x99,0xa8,
  0xc5,0xf4,0xa7,0x96,0x01,0x30,0x63,0x52,0x7c,0x4d,0x1e,0x2f,0xb8,0x89,0xda,0xeb,
  0x3d,0x0c,0x5f,0x6e,0xf9,0xc8,0x9b,0xaa,0x84,0xb5,0xe6,0xd7,0x40,0x71,0x22,0x13,
  0x7e,0x4f,0x1c,0x2d,0xba,0x8b,0xd8,0xe9,0xc7,0xf6,0xa5,0x94,0x03,0x32,0x61,0x50,
  0xbb,0x8a,0xd9,0xe8,0x7f,0x4e,0x1d,0x2c,0x02,0x33,0x60,0x51,0xc6,0xf7,0xa4,0x95,
  0xf8,0xc9,0x9a,0xab,0x3c,0x0d,0x5e,0x6f,0x41,0x70,0x23,0x12,0x85,0xb4,0xe7,0xd6,
  0x7a,0x4b,0x18,0x29,0xbe,0x8f,0xdc,0xed,0xc3,0xf2,0xa1,0x90,0x07,0x36,0x65,0x54,
  0x39,0x08,0x5b,0x6a,0xfd,0xcc,0x9f,0xae,0x80,0xb1,0xe2,0xd3,0x44,0x75,0x26,0x17,
  0xfc,0xcd,0x9e,0xaf,0x38,0x09,0x5a,0x6b,0x45,0x74,0x27,0x16,0x81,0xb0,0xe3,0xd2,
  0xbf,0x8e,0xdd,0xec,0x7b,0x4a,0x19,0x28,0x06,0x37,0x64,0x55,0xc2,0xf3,0xa0,0x91,
  0x47,0x76,0x25,0x14,0x83,0xb2,0xe1,0xd0,0xfe,0xcf,0x9c,0xad,0x3a,0x0b,0x58,0x69,
  0x04,0x35,0x66,0x57,0xc0,0xf1,0xa2,0x93,0xbd,0x8c,0xdf,0xee,0x79,0x48,0x1b,0x2a,
  0xc1,0xf0,0xa3,0x92,0x05,0x34,0x67,0x56,0x78,0x49,0x1a,0x2b,0xbc,0x8d,0xde,0xef,
  0x82,0xb3,0xe0,0xd1,0x46,0x77,0x24,0x15,0x3b,0x0a,0x59,0x68,0xff,0xce,0x9d,0xac
};

inline uint8_t calculateCRC( unsigned char *pc, size_t nbytes)
{
  uint8_t crc = 0xff;
  size_t i;
  for (i = 0; i < nbytes; i++) {
    crc ^= *pc++;
    crc = crctable[crc];
  }
  return crc;
}

inline uint32_t calculateSum(uint16_t *p16, size_t nwords) {
  uint32_t u = 0;
  size_t n = 0;
  for (n = 0; n < nwords; n++ ) {
    u += *p16++;
  }
  return u;
}

/* ==========================================================================
   Sending data and messages
 */


inline void sendDataCRC() {
  unsigned char crc;
  crc = calculateCRC((unsigned char *)&bufferp[DATASTART],NBYTES);
  Serial.print( "CRC 0x" );
  Serial.println( crc, HEX );
}

inline void sendDataSum() {
  uint32_t usum = 0;
  usum = calculateSum(&bufferp[DATASTART],NPIXELS);
  Serial.print( "SUM " );
  Serial.println( usum );
}

void sendBuffer_Formatted( ) {
  uint16_t *p16 = &bufferp[DATASTART];
  Serial.print( "DATA " );
  Serial.println( NPIXELS );
  for ( int n = 0; n < NPIXELS; n++ ) {
    Serial.println( p16[n] );
  }
  Serial.println( "END DATA" );
}

void sendBuffer_Binary( ) {
  uint16_t *p16 = &bufferp[DATASTART];
  Serial.print( "BINARY16 " );
  Serial.println( NPIXELS );
  Serial.write( (byte *) p16, NBYTES );
  Serial.println( "END DATA" );
}

void sendData( ) {

  if (crc_enable) sendDataCRC();
  if (sum_enable) sendDataSum();
  
  if ( dataformat == BINARY ) {
    sendBuffer_Binary( );
  }
  else if ( dataformat == ASCII ) {
    sendBuffer_Formatted( );
  }
}

void sendDataReady() {
  Serial.println( "READY DATA" );
}

// -------------------------------------------------------------------------
void sendTestData() {
  uint16_t *p16 = &bufferp[DATASTART];
  Serial.println( "TESTDATA" );
  // load test parttern
  for ( int n = 0; n < NPIXELS; n++ ) {
    p16[n] = n;
  }
  // send it
  sendData();
}

/* ==========================================================================
   Timers and time keeping
*/

uint64_t cycles64( )
{
  static uint32_t oldCycles = ARM_DWT_CYCCNT;
  static uint32_t highDWORD = 0;

  uint32_t newCycles = ARM_DWT_CYCCNT;

  if (newCycles < oldCycles) {
    ++highDWORD;
  }
  oldCycles = newCycles;
  
  return (((uint64_t)highDWORD << 32) | newCycles);
}

// Measures Exposure 
uint32_t exposure_elapsed_cycles_holder;
inline uint32_t exposure_elapsed_time_usecs() {
  return (ARM_DWT_CYCCNT - exposure_elapsed_cycles_holder)/CYCLES_PER_USEC;
}

inline void exposure_elapsed_time_start() {
  exposure_elapsed_cycles_holder = ARM_DWT_CYCCNT;
}

// Measures time since start
uint32_t frame_elapsed_cycles_holder;
inline uint32_t frame_elapsed_time_usecs() {
  return (ARM_DWT_CYCCNT - frame_elapsed_cycles_holder)/CYCLES_PER_USEC;
}

inline void frame_elapsed_time_start() {
  frame_elapsed_cycles_holder = ARM_DWT_CYCCNT;
}

// Measures cnvst to transfer time
uint32_t raw_elapsed_cycles_holder;
inline uint32_t raw_elapsed_cycles() {
  return (ARM_DWT_CYCCNT - raw_elapsed_cycles_holder);
}

inline void raw_elapsed_cycles_start() {
  raw_elapsed_cycles_holder = ARM_DWT_CYCCNT;
}



/* ==========================================================================
   Sensor ISR
*/

// Frame status
unsigned int frame_counter = 0;
unsigned int exposure_elapsed = 0;
unsigned int frame_elapsed = 0;
unsigned int frameset_counter = 0;

unsigned int frames_req = 1;
unsigned int exposure_req = 100;
unsigned int frame_interval_req = 200;

unsigned int frameset_interval_req = 100000;
unsigned int framesets_req = 1;

bool is_frame_clocked = false;
bool is_frame_triggered = false;
bool is_frameset_clocked = false;
bool is_frameset_triggered = false;

bool is_gated = false;

bool frames_complete = false;
bool framesets_complete = false;

unsigned int send_every = 1;

// Flag for the busy bit, inhibits the idler
bool is_busy = false;
bool is_running = false;

// Flag for st_isr attached
bool is_attached = false;

// Sync output
bool do_sync_start = false;
bool do_sync_shutter = false;
bool do_sync_holdoff = false;
unsigned int holdoff_usecs = 0;


// ----------------------------------------------------
  
IntervalTimer FrameTimer;
IntervalTimer FrameSetTimer;

unsigned int trg_countdown = 0;

unsigned int isr_state = 0;
unsigned int isr_next_state = 0;

unsigned int clocks_st_high = 0;

#define READY 0
#define COUNTING 1
#define LOWER_ST 2
#define READOUT 3
#define CHECKEOS 4

unsigned int readout_counter = 0;
unsigned int check_eos_counter = 0;

/* =====================================================================
   This is the S11639-01 state machine
   
   Everything that follows after it. is setting it up and starting it
   running for various data collection scenarios
*/


//#define ISRPIN fMPinMonitor
#define ISRPIN TRGPin
#define ISREDGE RISING
void S1163901_TRG_isr();

inline void start_S1163901_isr() {
  is_attached = true;
#ifdef FASTISR
  directAttach(digitalPinToInterrupt(ISRPIN), S1163901_TRG_isr, ISREDGE);
#else
  attachInterrupt(digitalPinToInterrupt(ISRPIN), S1163901_TRG_isr, ISREDGE);
#endif
}

inline void stop_S1163901_isr() {
#ifdef FASTISR
  directDetach(digitalPinToInterrupt(ISRPIN));
#else
  detachInterrupt(digitalPinToInterrupt(ISRPIN));
#endif
  is_attached = false;
}

void S1163901_TRG_isr()
{
  int i;

  // For the fast attach option
#ifdef FASTISR
  if( !directClear( ) ) {
    return;
  }
#endif
  
  //digitalWriteFast(sparePin,HIGH);
  //digitalWriteFast(sparePin,LOW);

  switch (isr_state)
    {
    case COUNTING:

      trg_countdown--;
      if (!trg_countdown) {
	isr_state = isr_next_state;
      }

      break;

    case LOWER_ST:

      // 3.2 nsecs
      digitalWriteFast( STPin, LOW );
      
      trg_countdown = 87;
      isr_next_state = READOUT;

      isr_state = COUNTING;
      readout_counter = 0;

      exposure_elapsed = exposure_elapsed_time_usecs();
      exposure_elapsed += 48*1000/(MASTER_CLOCK/1000);
      //Serial.printf("lowered st, exposure %d\n", exposure_elapsed);
      
      if (sync_toggled) {
	// 3.3 nsecs
	digitalToggleFast(syncPin);
	sync_toggled = false;
      }

      break;

    case READOUT:

      /* Note: if any changes are made in this part of the state
	 machine, check it with the scope, trigger on eos, and
	 make sure it stays in sync all the way to eos.
      */

      // Wait for the video output to stabilize
      //raw_elapsed_cycles_start();
      
#ifdef TESTING
      digitalWriteFast(sparePin,HIGH);
#endif

      // Start the ADC acquire, data is ready 730nsec later.
      digitalWriteFast( CNVSTPin, HIGH );
      //Serial.println("CNVST HIGH");
      
      /* We can lower cnvst early and start the SPI set up.
	 This works out to data starting at 750nsec
      */
      raw_elapsed_cycles_start();
      while( raw_elapsed_cycles() < NANOSECS_TO_CYCLES(580) );

      digitalWriteFast( CNVSTPin, LOW );
      //Serial.println("CNVST LOW");

#ifdef TESTING
      digitalWriteFast(sparePin,LOW);
#endif
      
      /* This is the 16 bit transfer, it takes 150 nsec to setup,
	 533 nsec for the transfer, and another 100 nsec to return.
	 Acquire cannot start again until after the 533 nsec.  So
	 not much savings availabe more than the above.
      */
      bufferp[readout_counter++] = SPI.transfer16(0xFFFF);
      
      if (readout_counter >= NREADOUT) {
	/* Setup for the next state. eos comes on the 2nd clk after
	   we're done readout. Let's give it one extra beyond that.
	*/
	check_eos_counter = 2;
	isr_state = CHECKEOS;
      }
      
#ifdef TESTING
      digitalWriteFast(sparePin,HIGH);
      digitalWriteFast(sparePin,LOW);
#endif
      
      break;

    case CHECKEOS:

      if(!(i=digitalReadFast(EOSPin))) {
	if (check_eos_counter) {
	  check_eos_counter--;
	  break;
	}
	Serial.print("Error: EOS not found after readout complete, ");
	Serial.println(readout_counter);
      }
	
      // If everything is complete, we can disable the CLK interrupt
      if (framesets_complete && frames_complete) {
	stop_S1163901_isr();
      }
      
#ifdef TESTING
      Serial.print( "EOS state: ");
      Serial.print(i);
      Serial.print( " check eos counter: ");
      Serial.println(check_eos_counter);
#endif
      
      if (frames_complete) {
	
	if (is_frame_clocked) {
	  FrameTimer.end();
	  is_frame_clocked = false;
	}
	else if (is_frame_triggered) {
	  detachInterrupt(digitalPinToInterrupt(interruptPin));
	  is_frame_triggered = false;
	}
	
	if (framesets_complete) {	  	    
	  if (is_frameset_clocked) {
	    FrameSetTimer.end();
	    is_frameset_clocked = false;
	  }
	  else if (is_frameset_triggered) {
	    detachInterrupt(digitalPinToInterrupt(interruptPin));
	    is_frameset_triggered = false;
	  }
	  
	}
      }

      // ----------------------------------------------------
      // And now we send the data
      if ( !send_every  || !(frame_counter % send_every) ) {
	
	// Twos complement for this ADC
	for ( i = 0; i < NREADOUT; i++ ) {
	  bufferp[i] ^= (uint16_t) 0x8000;
	}
	
	// --------------------
	// send the header and data
	// --------------------
	Serial.print( "ELAPSED " );
	Serial.println( frame_elapsed );

	Serial.print( "COUNTER " );
	Serial.println( frame_counter );
      
	Serial.print( "SHUTTER " );
	Serial.println( exposure_elapsed );
	
	if (data_async) {
	  sendData( );
	}
	else {
	  sendDataReady();
	}
	
	if  (frames_complete) {
	  Serial.println("FRAMESET END");
	  if ( framesets_complete) {
	    Serial.println("COMPLETE");
	  }
	}

      }

      if (frames_complete && framesets_complete) {
	if (busy_toggled) {
	  digitalToggleFast(busyPin);
	  busy_toggled = false;
	}
	else {
	  Serial.println("Error: framesets_complete, but not busy.");
	}
	is_busy = false;
      }

      // we are done with this frame
      isr_state = READY;

      break;
    }
}

// ----------------------------------------------------
void start_frame_isr()
{
  if (!is_gated) {
    isr_next_state = LOWER_ST;
    trg_countdown = clocks_st_high;
    //Serial.printf( "start_frame_isr, trg_countdown: %d\n",trg_countdown);
  }

  // Time since start of the set
  frame_elapsed = frame_elapsed_time_usecs();
    
  // Start the exposure timer
  exposure_elapsed_time_start();

  // assert the sync pin on shutter
  if (do_sync_shutter || (do_sync_start && !frame_counter) ) {
    digitalToggleFast(syncPin);
    sync_toggled = true;
  }

  // -------------------------------------
  // Increment here, and set flags, save time in the TRG interrupt handler
  frame_counter++;
  if (frame_counter >= frames_req) {
    frames_complete = true;
    frameset_counter++;
    if (frameset_counter >= framesets_req) {
      framesets_complete = true;
    }    
  }
  
  // -------------------------------------
  // We want the ST to go high as clock goes low
  while ( !CLOCKREAD ) {}  // wait while low
  while ( CLOCKREAD ) {}   // wait while high

  // Check this on the scope:  Does this go high before the trg pin?
  digitalWriteFast( STPin, HIGH );
  
  // Does this happen within the 30 nsecs?
  if (!is_gated) {
    isr_state = COUNTING;
  }
}

// ----------------------------------------------
bool gated_frame_state = false;

void gated_frame_isr()
{
  if (gated_frame_state) {
    isr_state = LOWER_ST;
    gated_frame_state = false;
  }
  else {
    start_frame_isr();
    gated_frame_state = true;
  }
}

// ----------------------------------------------
void start_triggered_frames_()
{
  frame_elapsed_time_start();

  frame_counter = 0;
  frames_complete = false;

  // connect single isr to the interrupt
  is_frame_triggered = true;
  attachInterrupt(digitalPinToInterrupt(interruptPin), start_frame_isr, intedgemode);  
  
}

// ----------------------------------------------
void start_clocked_frameset_isr()
{
  // start the elapsed time counter
  frame_elapsed_time_start();

  frame_counter = 0;
  frames_complete = false;

  if ( frames_req > 1 ) {
    is_frame_clocked = true;
    FrameTimer.begin(start_frame_isr,frame_interval_req);
  }
  else {
    start_frame_isr();
  }
}

// ----------------------------------------------
void start_triggered_clocked_frameset_()
{
  frame_elapsed_time_start();

  frame_counter = 0;
  frames_complete = false;

  // connect single isr to the interrupt
  is_frameset_triggered = true;
  attachInterrupt(digitalPinToInterrupt(interruptPin), start_clocked_frameset_isr, intedgemode);  
}

// ===================================================================
bool setup_frame_(unsigned int exposure_usecs)
{
  unsigned int clocks_exposure = USECS_TO_CLOCKS(exposure_usecs);

  if (clocks_exposure < 50) {
    Serial.println("Error: exposure too short");
    return false;
  }

  // we use this in the start_frame_isr()
  clocks_st_high = clocks_exposure - 48;

  // for the report sent to the host
  exposure_req = exposure_usecs;

  // default these, this is setup, they are always zero
  frame_counter = 0;
  frameset_counter = 0;
  
  // default these, the calling routines will have to set them after calling this
  frames_req = 1;
  framesets_req = 1;

  // and defaul these too, again, the calling routine has to set them, after return
  is_frame_clocked = false;
  is_frame_triggered = false;
  is_frameset_clocked = false;
  is_frameset_triggered = false;
  is_gated = false;
  
  return true;
}

bool setup_frameset_(unsigned int exposure_usecs,
		     unsigned int frame_interval_usecs, unsigned int nframes)
{
  if ( (exposure_usecs == 0) &&
       (frame_interval_usecs > (TRANSFERUSECS+EXPOSUREMIN)) ) {
    exposure_usecs = frame_interval_usecs-TRANSFERUSECS;
  }

  else if (frame_interval_usecs < exposure_usecs + TRANSFERUSECS) {
    Serial.println( "Error: need longer frame interval" );
    return false;
  }

  if (!setup_frame_(exposure_usecs)) {
    return false;
  }
  
  frames_req = nframes ? nframes : 1;
  frame_interval_req = frame_interval_usecs;

  return true;
}

// ===================================================================

bool start_gated_frames( unsigned int nframes)
{
  if (busy_toggled || is_busy) {
    Serial.println("Error: start gated frames while busy.");
    return false;
  }
  
  frame_counter = 0;
  frameset_counter = 0;

  frames_req = nframes?nframes:1;
  framesets_req = 1;

  frames_complete = false;
  framesets_complete = false;

  frame_elapsed_time_start();
    
  // Internal and external busy indicator
  digitalToggleFast(busyPin);
  busy_toggled = true;
  is_busy = true;
  
  // connect the sensor isr
  start_S1163901_isr();
  //is_attached = true;
  //attachInterrupt(digitalPinToInterrupt(TRGPin), S1163901_TRG_isr, RISING);  
    
  // connect single isr to the interrupt
  gated_frame_state = false;
  is_gated = true;
  attachInterrupt(digitalPinToInterrupt(interruptPin), gated_frame_isr, CHANGE);

  return true;
}


bool start_single_frame(unsigned int usecs)
{
  if (busy_toggled || is_busy) {
    Serial.println("Error: start single frame while busy.");
    return false;
  }
  
  if (!setup_frame_(usecs)) {
    return false;
  }

  // Internal and external busy indicator
  digitalToggleFast(busyPin);
  busy_toggled = true;
  is_busy = true;

  // connect the sensor isr
  start_S1163901_isr();
  //is_attached = true;
  //attachInterrupt(digitalPinToInterrupt(TRGPin), S1163901_TRG_isr, RISING);

  // collect one frame
  start_frame_isr();

  return true;
}

// -----------------------------------------------------------------------
bool start_clocked_frames(unsigned int exposure_usecs,
			  unsigned int frame_interval_usecs, unsigned int nframes)
{
  if (busy_toggled || is_busy) {
    Serial.println("Error: start single clocked frames busy.");
    return false;
  }

  if (nframes <= 1) {
    return start_single_frame(exposure_usecs);
  }
  
  if (!setup_frameset_(exposure_usecs,frame_interval_usecs,nframes)) {
    return false;
  }

  Serial.println( "CLOCKED START" );
  Serial.print( "EXPOSURE " );Serial.println( exposure_req );
  Serial.print( "CLOCK " ); Serial.println( frame_interval_req );
  Serial.print( "FRAMES " ); Serial.println( frames_req );
  Serial.print( "SETS " ); Serial.println( framesets_req );
    
  // Internal and external busy indicator
  digitalToggleFast(busyPin);
  busy_toggled = true;
  is_busy = true;
  
  // connect the sensor isr
  start_S1163901_isr();
  
  // the start clock framset isr takes care of the timer and "is clocked flag"
  start_clocked_frameset_isr();
    
  return true;  
}

bool start_clocked_framesets(unsigned int exposure_usecs,
			     unsigned int frame_interval_usecs, unsigned int nframes,
			     unsigned int frameset_interval_usecs, unsigned int nsets)
{
  if (busy_toggled || is_busy) {
    Serial.println("Error: start clocked framesets while busy.");
    return false;
  }
  
  if (nframes <= 1 && nsets <= 1) {
    return start_single_frame(exposure_usecs);
  }

  if (nsets <= 1) {
    return start_clocked_frames(exposure_usecs,frame_interval_usecs,nframes);
  }
  
  if (!setup_frameset_(exposure_usecs,frame_interval_usecs,nframes)) {
    return false;
  }

  if ( !frameset_interval_usecs ) {
    frameset_interval_usecs = frame_interval_usecs*nframes+TRANSFERUSECS;
  }
  else if (frameset_interval_usecs < frame_interval_usecs*nframes+TRANSFERUSECS) {
    Serial.println( "Error: need longer frameset interval" );
    return false;
  }

  frameset_interval_req = frameset_interval_usecs;
  framesets_req = nsets;
  
  Serial.println( "CLOCKED START" );
  Serial.print( "EXPOSURE " );Serial.println( exposure_req );
  Serial.print( "CLOCK " ); Serial.println( frame_interval_req );
  Serial.print( "FRAMES " ); Serial.println( frames_req );
  Serial.print( "SETS " ); Serial.println( framesets_req );
    
  // Internal and external busy indicator
  digitalToggleFast(busyPin);
  busy_toggled = true;
  is_busy = true;
  
  // connect the sensor isr
  start_S1163901_isr();

  is_frameset_clocked = true;
  FrameTimer.begin(start_clocked_frameset_isr,frameset_interval_req);

  return true;
}


// -----------------------------------------------------------------------
// Triggered single frames
bool start_triggered_frames( unsigned int usecs, unsigned int nframes)
{
  if (busy_toggled || is_busy) {
    Serial.println("Error: start triggered  frames while busy.");
    return false;
  }
  
  frame_elapsed_time_start();
  
  if (!setup_frame_(usecs)) {
    return false;
  }
  
  frames_req = nframes ? nframes : 1;

  Serial.println( "TRIGGERED SINGLES START" );  
  Serial.print( "EXPOSURE " );Serial.println( exposure_req );
  Serial.print( "FRAMES " ); Serial.println( frames_req );
  Serial.print( "SETS " ); Serial.println( framesets_req );
  
  // Internal and external busy indicator
  digitalToggleFast(busyPin);
  busy_toggled = true;
  is_busy = true;
  
  // connect the sensor isr
  start_S1163901_isr();

  // connect the single frame isr
  is_frame_triggered = true;
  attachInterrupt(digitalPinToInterrupt(interruptPin), start_frame_isr, intedgemode);

  return true;
}

bool start_triggered_clocked_frames( unsigned int exposure_usecs, unsigned int frame_interval_usecs,
				     unsigned int nframes, unsigned int nsets)
{
  if (busy_toggled || is_busy) {
    Serial.println("Error: start trigger clocked frame while busy.");
    return false;
  }
  
  if (!setup_frameset_(exposure_usecs,frame_interval_usecs,nframes)) {
    return false;
  }

  framesets_req = nsets ? nsets : 1;

  Serial.println( "TRIGGERED SETS START" );
  Serial.print( "EXPOSURE " );Serial.println( exposure_req );
  Serial.print( "CLOCK " ); Serial.println( frame_interval_req );
  Serial.print( "FRAMES " ); Serial.println( frames_req );
  Serial.print( "SETS " ); Serial.println( framesets_req );
  
  
  // Internal and external busy indicator
  digitalToggleFast(busyPin);
  busy_toggled = true;
  is_busy = true;
  
  // connect the sensor isr
  start_S1163901_isr();

  // connect the clock start
  is_frameset_triggered = true;
  attachInterrupt(digitalPinToInterrupt(interruptPin), start_clocked_frameset_isr, intedgemode);  

  return true;
}

bool wait_busy( unsigned int timeoutmillis, bool interruptible )
{
  unsigned int millicounter = 0;

  // do not allow forever loops
  if (!timeoutmillis) interruptible = true;

  while(is_busy &&
	(!interruptible || !Serial.available()) &&
	(!timeoutmillis || millicounter < timeoutmillis))
    {
      delay(100);    
    }
  return !is_busy;
}

/* ==================================================================
   Sensor start and stop
*/
bool masterclock_running = false;

void start_MasterClock( )
{
  pinMode(fMPin,     OUTPUT);   
  digitalWriteFast(fMPin, LOW);

  pinMode(fMPinMonitor,     INPUT);
  
  analogWriteResolution(4);          // pwm range 4 bits, i.e. 2^4
  analogWriteFrequency(fMPin, fM);
  analogWrite(fMPin,8);              // dutycycle 50% for 2^4

  masterclock_running = true;
}

void stop_MasterClock( )
{
  pinMode(fMPin,     OUTPUT);   
  digitalWriteFast(fMPin, LOW);

  masterclock_running = false;
}

void stop_sensor()
{
  if (is_attached) {
    stop_S1163901_isr();
  }
  isr_state = READY;

  digitalWriteFast(STPin, LOW);

  if (is_frame_triggered || is_gated) {
    detachInterrupt(digitalPinToInterrupt(interruptPin));
    is_frame_triggered = false;
    is_gated = false;
  }
	
  if (is_frame_clocked) {
    FrameTimer.end();
    is_frame_clocked = false;
  }

  if (is_frameset_clocked) {
    FrameSetTimer.end();
    is_frameset_clocked = false;
  }

  if (is_frameset_triggered) {
    detachInterrupt(digitalPinToInterrupt(interruptPin));
    is_frameset_triggered = false;
  }

  if (sync_toggled) {
    digitalToggleFast(syncPin);
    sync_toggled = false;
  }
  
  if (busy_toggled) {
    digitalToggleFast(busyPin);
    busy_toggled = false;
    is_busy = false;
  }
}
/* ==================================================================
   Sensor cleaning
*/

void S1163901_Pulse_( unsigned int usecs ) {

  if (isr_state != READY || is_busy) {
    Serial.println( "Error: shutter pulse called while busy" );
    return;
  }

  if (usecs < CLOCKS_TO_USECS(6) ) {
    Serial.println( (char *)"Error: need longer shutter time." );
  }
  
  pulsePinHigh( STPin, usecs );

}

bool quickClean(unsigned int usecs, unsigned int ncycles)
{
  if (isr_state != READY || is_busy) {
    Serial.println( (char *)"Error: Quick clean while busy." );
    return false;
  }

  if (usecs < CLOCKS_TO_USECS(100) ) {
    Serial.println( (char *)"Error: need longer shutter time." );
  }

  while( ncycles && !is_busy ) {

    pulsePinHigh( STPin, usecs );

    delayMicroseconds( usecs );

    ncycles--;
  }

  return true;
}

/* ==========================================================================
   Delay function call, compatible with isr,  delay up to 10msec else use timer.
*/
void (*delayed_function_ptr)() = 0;
IntervalTimer delay_Timer;

void delayed_isr() {
  if (delayed_function_ptr) {
    (*delayed_function_ptr)();
    delay_Timer.end();
  }
}

// ----------------------------------------------------
//   This is the call to execute a function after a delay

inline void delay_call( void (*funct)(), uint32_t delay_usecs ) {
  if ( delay_usecs < 10000 ) {
    delayMicroseconds(delay_usecs);
    (*funct)();
  }
  else {
    delayed_function_ptr = funct;
    delay_Timer.begin(delayed_isr,delay_usecs);
  }
}

/* ==========================================================================
   Hold off, similar to the abovee
*/

void (*holdoff_function_ptr)() = 0;
IntervalTimer holdoff_delay_Timer;

void holdoff_delay_isr_() {
  holdoff_delay_Timer.end();
  if (holdoff_function_ptr) {
    (*holdoff_function_ptr)();
  }
}

inline void holdoff_isr( ) {
  if (do_sync_holdoff) {
    digitalToggleFast(syncPin);
    delayMicroseconds(1);
    digitalToggleFast(syncPin);
  }
  if ( holdoff_usecs < 10000 ) {
   delayMicroseconds(holdoff_usecs);
   (*holdoff_function_ptr)();
  }
  else {
    holdoff_delay_Timer.begin(holdoff_delay_isr_,holdoff_usecs);
  }
}

void set_holdoff_function( void (*funct)() ) {
  holdoff_function_ptr = funct;
}

void cancel_holdoff( ) {
  holdoff_delay_Timer.end();
}


/* ===================================================================
   The setup routine should be run from setup() in the master sketch, after you setup Serial.
*/

void sensor_setup() {

  Serial.println( "Sensor Setup" );

  // External Synch pins
  pinMode(interruptPin, INPUT);

  pinMode(busyPin, OUTPUT);
  digitalWriteFast(busyPin, busyPinState);

  pinMode(syncPin, OUTPUT);
  digitalWriteFast(syncPin, syncPinState);

  pinMode(sparePin, OUTPUT);
  digitalWriteFast(sparePin, sparePinState);

  // ==============================================
  // S11639-01 pins
  pinMode(STPin,     OUTPUT);
  digitalWriteFast(STPin, LOW);
  
  pinMode(TRGPin,     INPUT);   
  
  pinMode(EOSPin,     INPUT);   
  
  // ============================================
  // SPI library takes care of the other pins
  pinMode(CNVSTPin,     OUTPUT);   
  digitalWriteFast(CNVSTPin, LOW);
  
  SPI.begin();
  SPI.beginTransaction(spi_settings);

  // ============================================
  // Master Clock
  start_MasterClock();
  
#ifdef RINGBUFFERING
  for (n=0; n<NBUFFERS; n++) {
    frameRing[n] = &buffers[n][0];
  }
#endif  
}


/* ==========================================================================
   Diagnostic dump
 */

void sensor_dump( ){
  
  Serial.println( "#Sensor dump" );

  // ------------------------------------------------------
  Serial.printf( "#states toggled busy: %s toggled %s sync: %s toggled %s spare: %s\n",
		 boolstr(busyPinState),boolstr(busy_toggled),
		 boolstr(syncPinState),boolstr(sync_toggled),
		 boolstr(sparePinState));
  
   Serial.printf( "#isr attached: %s state: %s next: %s, countdown %d\n",
		 boolstr(is_attached),
		 isrstatestring(isr_state),
		 isrstatestring(isr_next_state),
		 trg_countdown
		 );
  
  // ------------------------------------------------------
  Serial.printf( "#sync on_start: %s on_shutter: %s holdoff: %s %d usecs\n",
		 boolstr(do_sync_start),
		 boolstr(do_sync_shutter),
		 boolstr(do_sync_holdoff),
		 holdoff_usecs);

  // ------------------------------------------------------
  Serial.printf( "#frame counter: %d/%d exposure: %d/%d elapsed: %d/%d ",
		 frame_counter, frames_req,
		 exposure_elapsed, exposure_req,
		 frame_elapsed/frame_interval_req);

  Serial.printf( "clk'd: %s trig'd: %s gated: %s ",
		 boolstr(is_frame_clocked),
		 boolstr(is_frame_triggered),
		 boolstr(is_gated));

  Serial.printf( "complete %s\n", boolstr(frames_complete));

  // ------------------------------------------------------
  Serial.printf( "#framesets counter: %d/%d interval: %d ",
		 frameset_counter, framesets_req,
		 frameset_interval_req );

  Serial.printf( "clk'd: %s trig'd: %s ",
		 boolstr(is_frameset_clocked),
		 boolstr(is_frameset_triggered));

  Serial.printf( "complete %s\n", boolstr(framesets_complete));
		 
  // ------------------------------------------------------
  Serial.printf( "#transfers aync: %s crc: %s sum: %s %s sel: %d\n",
		 boolstr(data_async),
		 boolstr(crc_enable),
		 boolstr(sum_enable),
		 dataformatstring(dataformat),
		 buffer_index);
		 
  // ------------------------------------------------------
  
  /*
  Serial.print( "#idler Usecs " ); Serial.print( idlerUsecs );
  Serial.print( " counter " ); Serial.print( idlerCounter );
  Serial.print( " counts " ); Serial.print( idlerCounts );
  Serial.print( " running " ); Serial.print( idlerRunning );
  Serial.print( " auto " ); Serial.print( idlerAuto );
  Serial.println( "" );
  */

  // ------------------------------------------------------
  Serial.printf( "#do_latch %s value: %d pixel: %d\n",
		 boolstr(do_latch), latch_value, latch_pixel );

  if (!spare_pwm) {
    Serial.print( " spare " );
    Serial.print( sparePinState );
  }
  Serial.println( "" );
  
  if (spare_pwm) {
    Serial.print( "#pwm" );
    Serial.print( " resolution " ); Serial.print( pwm_resolution );
    Serial.print( " frequency " ); Serial.print( pwm_frequency );
    Serial.print( " value " ); Serial.print( pwm_value );
    Serial.println( "" );
  }

}


/* =================================================================================================
   Command line processing.  From loop(), for each line read from the
   serial input, call sensor_cli( line )
*/

void sensor_help()
{
  Serial.println("#  version           - report software version");
  Serial.println("#  configuration     - report device configuration and data structure");
  Serial.println("#  pins              - report digital i/o functions and pin numbers");
  Serial.println("#");
  Serial.println("#  stop              - stop the data collection");
  Serial.println("#  stop clock        - stop the master clock");
  Serial.println("#  start clock       - start the master clock");
  Serial.println("#");
  Serial.println("#  set ascii          - set data format to ascii");
  Serial.println("#  set binary         - set data format to binary");
  Serial.println("#  format             - report data form");
  Serial.println("#");
  Serial.println("#  set async          - data is sent asynchronously");
  Serial.println("#  set synchronous    - READY DATA is sent, host responds send data");
  Serial.println("#");
  Serial.println("#  enable|disable crc - precede each data transfer with crc");
  Serial.println("#  enable|disable sum - precede each data transfer with sum");
  Serial.println("#");
  Serial.println("#  send crc | crc     - send 8 bit crc calculated on the binary data");
  Serial.println("#  send sum | sum     - send 32 bit sum calculated on the binary data");
  Serial.println("#  send [data]        - send test data");
  Serial.println("#  send test          - (re)send last data");
  Serial.println("#");
  Serial.println("#  select buffer [n]  - select/report current buffer by number");
  Serial.println("#  send               - send contents of the current buffer");
  Serial.println("#");
  Serial.println("#  clock|read <exposure>                            - read one frame"); // legacy
  Serial.println("#  clock|read <n> <frame-interval>                  - read n frames");  // legacy
  Serial.println("#  clock|read <n> <exposure> <interval>             - read n frames");  // legacy
  Serial.println("#  clock|read <m> <outer> <n> <interval>            - read m sets of n clocked frames");
  Serial.println("#  clock|read <m> <outer> <n> <exposure> <interval> - read m sets of n clocked frames");
  Serial.println("#");
  Serial.println("#  trigger <exposure>                    - trigger one frame");               // new
  Serial.println("#  trigger <n> <exposure>                - trigger n times, single frames");  // legacy
  Serial.println("#  trigger <m> <n> <exposure>            - trigger m sets of n clocked frames");  // legacy
  Serial.println("#  trigger <m> <n> <exposure> <interval> - trigger m sets of n clocked frames");      // new
  Serial.println("#");
  Serial.println("#  gate <n>                - gate n frames, default to 1");
  Serial.println("#");	
  Serial.println("#  set trigger rising      - set trigger on rising edge");
  Serial.println("#  set trigger falling     - set trigger on falling edge");
  Serial.println("#  set trigger change      - set trigger on both rising and falling edges");
  Serial.println("#");
  Serial.println("#  set trigger pullup      - set trigger input to pullup");
  Serial.println("#  clear trigger pullup    - set trigger input to no pullup");
  Serial.println("#");
  Serial.println("#  latch");
  Serial.println("#  latch <pixel>");
  Serial.println("#  clear latch");
  Serial.println("#");	
  Serial.println("#  set sync shutter                - assert sync pin during exposure");
  Serial.println("#  set sync start                  - assert sync pin at first frame in series"); 
  Serial.println("#  set sync holdoff [usecs]        - assert sync and wait for start");
  Serial.println("#  set sync off                    - clear sync assert setting");
  Serial.println("#  clear sync");
  Serial.println("#");
  Serial.println("#  set spare|sync|busy hi|lo       - set pin and new non asserted state");
  Serial.println("#  pulse spare|sync|busy [usecs]   - pulse the pin for usecs duration");
  Serial.println("#  toggle spare|sync|busy          - toggle pin, set new non-asserted state");
  Serial.println("#");
#ifdef EEPROMLIB_H
  Serial.println("#  identifier        - report identifier string");
  Serial.println("#  coefficients      - report stored coefficients");
  Serial.println("#  units             - report stored units string (nm, um, etc)");
  Serial.println("#");
  Serial.println("#  store identifier <string>   - writes to a reserved location in eeprom");
  Serial.println("#  store coefficients <a0> <a1> <a2> <a3>");
  Serial.println("#  store units");
  Serial.println("#");
#endif
  /*
  Serial.println("#Idler");
  Serial.println("#  set idler off      - set idler off/auto/time spec");
  Serial.println("#  set idler auto");
  Serial.println("#  set idler <usecs>");
  Serial.println("#");
  Serial.println("#  pwm <bits> <binary_value>");
  Serial.println("#  pwm off");
  Serial.println("#");
  */  
  Serial.printf("#Pins: Trig(inp) %d busy %d sync %d spare %d\n",
		interruptPin, busyPin, syncPin, sparePin );
  
  
}

/* -------------------------------------------------------------
   Some lightweight string processing
*/

bool testkey(char *pc, const char *key, char **next) {
  int n = strlen(key);
  if (*pc && !strncmp(pc,key,n)) {
    pc += n;
    while (*pc && !isspace(*pc)) pc++;
    while (*pc && isspace(*pc)) pc++;
    if (next) *next = pc;
    return true;
  }
  if (next) *next = pc;
  return false;
}

bool testuint(char*pc,char**next,unsigned int *u) {
  unsigned long ul;
  ul = strtoul(pc,next,0);
  if (*next > pc && ul <= UINT_MAX) {
    if (u) *u = ul;
    return true;
  }
  return false;
}

int testuints(char*pc,char**next,unsigned int *u, int nmax) {
  int n = 0;
  while ( (n<nmax) && testuint(pc,&pc,u) ) {
    u++;
    n++;
  }
  return n;
}

bool testfloat(char*pc,char**next,float *pf) {
  float f;
  f = strtof(pc,next);
  if (*next > pc) {
    if (f) *pf = f;
    return true;
  }
  return false;
}

int testfloats(char*pc,char**next,float *pf, int nmax) {
  int n = 0;
  while ( (n<nmax) && testfloat(pc,&pc,pf) ) {
    pf++;
    n++;
  }
  return n;
}

int testlength(char*pc) {
  int n = 0;
  while(*pc&&isprint(*pc)&&*pc!=0xff) {
    pc++;
    n++;
  }
  return n;
}

/* -------------------------------------------------------------
   Some specialized parsing and setting of pin states and functions
*/

bool setsyncmode( char *pc) {
  bool retv = false;
  if (testkey(pc,"shutter",&pc)) {
    do_sync_shutter = true;
    do_sync_start = false;
    do_sync_holdoff = false;
    retv = true;
  }
  else if (testkey(pc,"start",&pc)) {
    do_sync_shutter = false;
    do_sync_start = true;
    do_sync_holdoff = false;
    retv = true;
  }
  else if (testkey(pc,"holdoff",&pc)) {
    do_sync_shutter = false;
    do_sync_start = false;
    do_sync_holdoff = testuint(pc,&pc,&holdoff_usecs);
    retv = do_sync_holdoff;
  }
  else if (testkey(pc,"off",&pc)) {
    do_sync_shutter = false;
    do_sync_start = false;
    do_sync_holdoff = false;
    retv = true;
  }
  return retv;
}

// -----------------------------------------------
bool setpinstate_( int pin, unsigned int state )
{
  if (pin<NUMBEROFPINS)
    {
      //Serial.printf("setpinstate_ %d, %d\n",pin,state);
      if (state) {
	//Serial.printf("setpinstate_ %d, %d HIGH\n",pin,state);
	digitalWriteFast(pin,HIGH);
      }
      else {
	//Serial.printf("setpinstate_ %d, %d LOW\n",pin,state);
	digitalWriteFast(pin,LOW);
      }

      switch(pin)
	{
	case syncPin:
	  syncPinState = state;
	  break;
	case busyPin:
	  busyPinState = state;
	  break;
	case sparePin:
	  sparePinState= state;
	  break;
	default:
	  break;
	}

      return true;
    }
  //Serial.printf("setpinstate_ no valid pin %d, %d\n",pin,state);
  return false;  
}
      
bool setpinstate( int pin, char *pc) {
  //Serial.printf("setpinstate %d, %s\n",pin,pc);
  if (testkey(pc,"hi",&pc)) {
    //Serial.printf("setpinstate %d, hi\n",pin,pc);
    return setpinstate_( pin, HIGH );
  }
  else if (testkey(pc,"lo",&pc)) {
    //Serial.printf("setpinstate %d, lo\n",pin,pc);
    return setpinstate_( pin, LOW );
  }
  return false;
}

// -----------------------------------------------
bool togglepinstate( int pin )
{
  if (pin<NUMBEROFPINS)
    {
      digitalToggleFast(pin);

      switch(pin)
	{
	case syncPin:
	  syncPinState = !syncPinState;
	  break;
	case busyPin:
	  busyPinState = !busyPinState;
	  break;
	case sparePin:
	  sparePinState = !sparePinState;
	  break;
	default:
	  break;
	}

      return true;
    }
  return false;  
}

// -----------------------------------------------
int selectpin( char *pc, char **next) {
  int pin = -1;
  //Serial.printf("selectpin: %s\n",pc);
  if (testkey(pc,"sync",&pc)) {
    if (next) *next = pc;
    //Serial.printf( "testkey found sync, next %s\n", *next );
    pin = syncPin;
  }
  if (testkey(pc,"busy",&pc)) {
    if (next) *next = pc;
    //Serial.printf( "testkey found busy, next %s\n", *next );
    pin = busyPin;
  }
  if (testkey(pc,"spare",&pc)) {
    if (next) *next = pc;
    //Serial.printf( "testkey found spare, next %s\n", *next );
    pin = sparePin;
  }
  return pin;
}

/* -------------------------------------------------------------
   And now, the actual sensor CLI
*/
bool sensor_cli( char *pc, char **next )
{
  bool retv = false;
  int n;
  unsigned int u;


  if (testkey(pc,"stop",&pc)) {
    stop_sensor();
    if (testkey(pc,"clock",&pc)) {
      stop_MasterClock();
    }    
    retv = true;
  }
  
  else if (testkey(pc,"start clock",&pc)) {
    stop_sensor();
    start_MasterClock();
    retv = true;
  }
  
  else if ( (testkey(pc, "version", &pc)) ) {
    Serial.println( versionstr );
    Serial.println( authorstr );
    retv = true;
  }
    
  else if (testkey(pc,"configuration",&pc)) {

    Serial.printf( "PIXELS %u DARK %u BITS %u VFS %f", NPIXELS, NDARK, NBITS, VFS );
    Serial.print( " SENSOR " );
    Serial.print( sensorstr );
    Serial.printf( " OFFSET %f RANGE %f", SENSOR_SCALE_OFFSET, SENSOR_SCALE_RANGE);
    Serial.println("");

      
#ifdef DIAGNOSTICS_CPU
    Serial.print("F_CPU: ");
    Serial.print(F_CPU/1e6);
    Serial.println(" MHz."); 

    Serial.print("ADC_F_BUS: ");
    Serial.print(ADC_F_BUS/1e6);
    Serial.println(" MHz.");
#endif
    
    retv = true;
      
  }

  else if (testkey(pc, "pins", &pc )) {

    Serial.println("#Pins");
    Serial.print("#  Trigger(input)" );
    Serial.print( interruptPin );
    Serial.print("  Busy " );
    Serial.print( busyPin );
    Serial.print("  Sync " );
    Serial.print( syncPin );
    Serial.print("  Spare " );
    Serial.print( sparePin );
    Serial.println( "" );
    retv = true;
    
  }
  
  else if (testkey(pc,"set",&pc)) {

    if (is_busy) goto busymsg;
    
    if (testkey(pc,"trigger",&pc)) {
      if (testkey(pc,"rising",0)) {
	intedgemode = RISING;
	retv = true;
      }
      else if (testkey(pc,"falling",&pc)) {
	intedgemode = FALLING;
	retv = true;
      }
      else if (testkey(pc,"change",&pc)) {
	intedgemode = CHANGE;
	retv = true;
      }
      else if (testkey(pc,"pullup",&pc)) {
	pinMode(interruptPin, INPUT_PULLUP);
	retv = true;
      }
      else if (testkey(pc,"nopullup",&pc)) {
	pinMode(interruptPin, INPUT);
	retv = true;
      }
    }
    else  if (testkey(pc,"synch",&pc)) {
      if (is_busy) goto busymsg;
      data_async = false;
      retv = true;
    }
    else if (testkey(pc,"async",&pc)) {
      data_async = true;
      retv = true;
    }
    else if (testkey(pc,"binary",&pc)) {
      dataformat = BINARY;
      retv = BINARY;
    }
    else if (testkey(pc,"ascii",&pc)) {
      dataformat = ASCII;
      retv = true;
    }
    else if (testkey(pc,"float",&pc)) {
      dataformat = FLOAT;
      retv = FLOAT;
    }
    else if (testkey(pc,"exposure",&pc)) {
      if (testuint(pc,&pc,&exposure_req)) {
	retv = true;
      }
    }
    else if (testkey(pc,"interval",&pc)) {
      if (testuint(pc,&pc,&frame_interval_req)) {
	retv = true;
      }
    }
    else if (testkey(pc,"frames",&pc)) {
      if (testuint(pc,&pc,&frames_req)) {
	retv = true;
      }
    }
    else if (testkey(pc,"sets",&pc)) {
      if (testuint(pc,&pc,&framesets_req)) {
	retv = true;
      }
    }

    // this handles all of the user pins
    else if ((n=selectpin(pc,&pc))>=0) {
      if (n == syncPin) {
	//Serial.printf( "selectpin returned %d (syncpin), %s\n", n, pc);
	retv = setpinstate(n,pc) || setsyncmode(pc);
      }
      else {
	//Serial.printf( "selectpin returned %d, %s\n",n,pc);
	retv = setpinstate(n,pc);
      }
    }

    else if (testkey(pc,"latch",&pc)) {
      do_latch = testuint(pc,&pc,&latch_pixel);
      retv = do_latch;
    }

  }

  else if (testkey(pc,"toggle",&pc)) {
    if ((n=selectpin(pc,&pc))>=0) {
      retv = togglepinstate(n);
    }
  }

  else if (testkey(pc,"pulse",&pc)) {
    unsigned int pulsewidth = 1;
    if ((n=selectpin(pc,&pc))>=0) {
      testuint(pc,&pc,&pulsewidth);
      pulsewidth = pulsewidth ? pulsewidth : 1;  
      pulsePin(n,pulsewidth);
      retv = true;
    }
  }
      
  else if (testkey(pc,"clear",&pc)) {
    if (testkey(pc,"sync",&pc)) {
      if (is_busy) goto busymsg;
      do_sync_shutter = false;
      do_sync_start = false;
      do_sync_holdoff = false;      
      retv = true;
    }
    else if (testkey(pc,"trigger pullup",&pc)) {
      pinMode(interruptPin, INPUT);
      retv = true;
    }
    else if (testkey(pc,"latch",&pc)) {
      do_latch = false;
      retv = true;
    }
  }

  else if (testkey(pc,"enable",&pc)) {
    if (testkey(pc,"crc",&pc)) {
      crc_enable = true;
      retv = true;
    }
    else if (testkey(pc,"sum",&pc)) {
      sum_enable = true;
      retv = true;
    }
  }

  else if (testkey(pc,"disable",&pc)) {
    if (testkey(pc,"crc",&pc)) {
      crc_enable = false;
      retv = true;
    }
    else if (testkey(pc,"sum",&pc)) {
      sum_enable = false;
      retv = true;
    }
  }
  
  else if (testkey(pc,"format",&pc)) {
    // print format
    switch( dataformat )
      {
      case BINARY:
	Serial.println("format binary");
	break;
      case ASCII:
	Serial.println("format ascii");
	break;
      case FLOAT:
	Serial.println("format float");
	break;
      }
    retv = true;
  }

  else if (testkey(pc,"clock",&pc) || testkey(pc,"read",&pc)) {
    unsigned int ulist[5];
    int npars = testuints(pc,&pc,ulist,5);
    switch (npars)
      {
      case 1:
	//quickClean(200, 3);
	retv = start_single_frame(ulist[0]);
	break;
      case 2:
	//quickClean(200, 3);
	retv = start_clocked_frames(0, ulist[1], ulist[0]);
	break;
      case 3:
	//quickClean(200, 3);
	retv = start_clocked_frames(ulist[1], ulist[2], ulist[0]);
	break;
      case 4:
	//quickClean(200, 3);
	retv = start_clocked_framesets(0, ulist[3], ulist[2], ulist[1], ulist[0]);
	break;
      case 5:
	//quickClean(200, 3);
	retv = start_clocked_framesets(ulist[4], ulist[3], ulist[2], ulist[1], ulist[0]);
	break;
      }
  }

  else if (testkey(pc,"trigger",&pc)) {
    unsigned int ulist[4];
    int npars = testuints(pc,&pc,ulist,4);
    switch (npars)
      {
      case 1:
	//quickClean(200, 3);
	retv = start_triggered_frames(ulist[0],1);
	break;
      case 2:
	//quickClean(200, 3);
	retv = start_triggered_frames(ulist[1], ulist[0]);
	break;
      case 3:
	//quickClean(200, 3);
	retv = start_triggered_clocked_frames(0,ulist[2], ulist[1], ulist[0]);
	break;
      case 4:
	//quickClean(200, 3);
	retv = start_triggered_clocked_frames(ulist[3], ulist[2], ulist[1], ulist[0]);
	break;
      }
  }

  else if (testkey(pc,"gate",&pc)) {
    unsigned int nframes = 1;
    if (!(*pc) || testuint(pc,&pc,&nframes)) {
	//quickClean(200, 3);
      retv = start_gated_frames(nframes);
    }
  }

  else if (testkey(pc,"wait",&pc)) {
    unsigned int timeoutmillis = 0;
    bool interruptible = false;

    testuint(pc,&pc,&timeoutmillis);
    interruptible = !timeoutmillis || testkey(pc,"interrupt",&pc);
    
    retv = wait_busy(timeoutmillis,interruptible);
  }

  else if (testkey(pc,"send",&pc)) {
    if (testkey(pc,"crc",&pc)) {
      sendDataCRC();     
    }
  
    else if (testkey(pc,"sum",&pc)) {
      sendDataSum();     
    }

    else if (testkey(pc,"test",&pc)) {
      sendTestData();
    }
    else {
      sendData();
    }
    retv = true;
  }

  else if (testkey(pc,"crc",&pc)) {
    sendDataCRC();     
    retv = true;
  }
  
  else if (testkey(pc,"sum",&pc)) {
    sendDataSum();     
    retv = true;
  }

  else if (testkey(pc,"increment buffer",&pc)) {
    incrementBufferSelect();
  }

  else if (testkey(pc,"decrement buffer",&pc)) {
    decrementBufferSelect();
  }
  
  else if (testkey(pc,"select buffer",&pc)) {
    if (*pc && testuint(pc,&pc,&u) ) {
      selectBuffer(u);
      retv = true;
    }
    else {
      Serial.print( "buffer " );
      Serial.println( buffer_index );
      retv = true;
    }
  }

  else if (testkey(pc,"dump",&pc)) {
    sensor_dump();
    retv = true;
  }

#ifdef EEPROMLIB_H
  else if (testkey(pc,"store",&pc)) {

    if (testkey(pc,"coefficients",&pc)) {
      float coeffs[EEPROM_NCOEFFS] = { 0};
      if (testfloats(pc,&pc,coeffs,EEPROM_NCOEFFS)) {
	storeCoefficients(coeffs);
	retv = true;
      }
    }
    
    else if (testkey(pc,"units",&pc)) {
      if (*pc) {
	storeUnits(pc);
	retv = true;
      }
    }
    else if (testkey(pc,"identifier",&pc)) {
      if (*pc) {
	storeIdentifier(pc);
	retv = true;
      }
    }
    
  }

  else if (testkey(pc,"erase",&pc)) {

    if (testkey(pc,"coefficients",&pc)) {
      eraseCoefficients();
      retv = true;
    }
    
    else if (testkey(pc,"units",&pc)) {
      eraseUnits();
      retv = true;
    }
    else if (testkey(pc,"identifier",&pc)) {
      eraseIdentifier();
      retv = true;
    }
    
  }

  else if (testkey(pc,"coefficients",&pc)) {
    printCoefficients();
    retv = true;
  }
    
  else if (testkey(pc,"units",&pc)) {
    printUnits();
    retv = true;
  }

  else if (testkey(pc,"identifier",&pc)) {
    printIdentifier( );
    retv = true;
  }
#endif

  Serial.printf("Sensor_cli returning %d\n", retv );

  if (next) *next = pc;
  return retv;
  
 busymsg:
  Serial.println("Error: command not accepted while sensor is busy");

  if (next) *next = pc;
  return false;
}
