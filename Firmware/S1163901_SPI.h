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

// ------------------------------------------------------------------
//#define DIAGNOSTICS_CPU
//#define DIAGNOSTICS_IDLER
//#define DIAGNOSTICS_SYNC
//#define DIAGNOSTICS_GATE
// Stream& dataPort = SerialUSB1;
// ------------------------------------------------------------------

#ifndef S1163901_SPI_H
#define S1163901_SPI_H

// Master Clock Frequency
extern const unsigned long fM;

/* =====================================================
   Pins, these need to be constant to take advantage of
   digitalWriteFast etc.
   They are hard coded in the the .c file, because
   of the register level macros
*/
extern const char sensorstr[];

// External trigger, sync 
extern const int interruptPin;  // Trigger input pin
extern const int busyPin;       // Gate output pin, goes high during shutter
extern const int syncPin;       // Goes low for trigger standoff (delay) period, use this for desktop N2 laser
extern const int sparePin;      // Spare pin for digital output

// Clock and logic to the ILX511 sensor
extern const int fMPin;         // Mclk,  FTM1 is 3, 4
extern const int fMPinMonitor;  // Mclk monitor
extern const int STPin;         // start pin
extern const int TRGPin;        // trig to start conversion
extern const int EOSPin;        // Spare pin to the sensor board

// SPI interface to the ADC
extern const int CNVSTPin;
extern const int SDIPin;
extern const int SDOPin;
extern const int CLKPin;

// Pin states
extern int intedgemode;
extern unsigned int busyPinState;
extern unsigned int syncPinState;
extern unsigned int sparePinState;

// Operational States
extern bool is_busy;
extern bool is_running;
extern bool is_attached;

// Sync pin functionaltiyt
extern bool do_sync_start;
extern bool do_sync_shutter;
extern bool do_sync_holdoff;
extern unsigned int holdoff_usecs;

// PWM on spare pin
extern bool spare_pwm;
extern uint32_t pwm_resolution;
extern uint32_t pwm_value;
extern float pwm_frequency;

void sparePWM_stop();
void sparePWM_start();

/* =====================================================
   Data configuration
*/
// ADC configuration, not much more needed for this ADC
#define NBITS 16
#define VFS  4.0 // This is in units of volts from the sensor
#define SENSOR_SCALE_OFFSET 1.0
#define SENSOR_SCALE_RANGE 2.0

// Sensor's data structure
#define NREADOUT 2048
#define DATASTART 0
#define DATASTOP 2048

#define NPIXELS (DATASTOP-DATASTART)
#define NBYTES (NPIXELS*2)

#define NDARK 0

// -----------------------------------------------
/* Data transfer format
 */
#define BINARY 0
#define ASCII 1
#define FLOAT 2
extern unsigned int dataformat;

extern unsigned int send_every;

/* Transfer handshake mode
 */
extern bool data_async;

/* Data integrity check
 */
extern bool crc_enable;
extern bool sum_enable;

/* --------------------------------------------------------
   Data buffers and transfer functions
 */

// Need to be a power of 2 for ring 
#define NBUFFERS 16
extern uint16_t buffers[NBUFFERS][NREADOUT];

extern uint16_t *bufferp;
extern unsigned int buffer_index;

/* --------------------------------------------------------
   ISR operation and frame states
*/

// Frame state
extern unsigned int frame_counter;
extern unsigned int exposure_elapsed;
extern unsigned int frame_elapsed;
extern unsigned int frameset_counter;

// Frame settings
extern unsigned int frames_req;
extern unsigned int exposure_req;
extern unsigned int frame_interval_req;
extern unsigned int framesets_req;

extern unsigned int framesets_interval_req;

// Ring bufering
#if 0
#define RINGBUFFERING
typedef struct {
  unsigned int frame_counter;      // frame number
  unsigned int exposure_elapsed;   // measured shutter usecs
  unsigned int frame_elapsed;       // usecs since this set start or trigger
  unsigned int frameset_counter;   // frame number
  double *data;
} Frame_t;

extern Frame_t frameRing[];
extern uint16_t readout_index;
extern uint16_t send_index;

inline uint16_t *getReadoutFrame();
inline uint16_t *getSendBuffer();
#endif

/* --------------------------------------------------------
   Latching
*/
extern bool do_latch;
extern unsigned int latch_value;
extern unsigned int latch_pixel;

/* --------------------------------------------------------
   Pin functions
*/
void pulsePin( unsigned int pin, unsigned int usecs );

void pulsePinHigh( unsigned int pin, unsigned int usecs );

void pulsePinLow( unsigned int pin, unsigned int usecs );

/* --------------------------------------------------------
   Sending data and messages
 */
void sendData( );

void sendDataReady();

void sendTestData();

uint16_t *initBufferSelect();
uint16_t *decrementBufferSelect();
uint16_t *incrementBufferSelect();
uint16_t *selectBuffer( unsigned int isel );
uint16_t *currentBufferSelect();

/* --------------------------------------------------------
   Data collection
*/
inline void start_S1163901_isr();
inline void stop_S1163901_isr();


bool start_gated_frames( unsigned int nframes);

bool start_single_frame(unsigned int usecs);

bool start_clocked_frames(unsigned int exposure_usecs, unsigned int frame_interval_usecs, unsigned int nframes);

bool start_clocked_framesets(unsigned int exposure_usecs,
			     unsigned int frame_interval_usecs, unsigned int nframes,
			     unsigned int frameset_interval_usecs, unsigned int nsets);

bool start_triggered_frames( unsigned int usecs, unsigned int nframes);

bool start_triggered_clocked_frames( unsigned int exposure_usecs,
				     unsigned int frame_interval_usecs,
				     unsigned int nframes, unsigned int nsets);

bool quickClean(unsigned int usecs, unsigned int ncycles);

bool wait_busy( unsigned int timeoutmillis, bool interruptiple );

/* --------------------------------------------------
 */
void start_MasterClock();
void stop_MasterClock();

void stop_sensor();

/* --------------------------------------------------
 */
void sensor_dump();

void sensor_help();

void sensor_setup();

bool sensor_cli(char *pc, char **next);

/* --------------------------------------------------
 */
bool testkey(char *pc, const char *key, char **next);

bool testuint(char*pc,char**next,unsigned int *u);
int testuints(char*pc,char**next,unsigned int *u, int nmax);

bool testfloat(char*pc,char**next,float *u);
int testfloats(char*pc,char**next,float *u, int nmax);

#endif
