/*
  Author:   Mitchell C. Nelson
  Date:     January 3, 2023
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

#include "Arduino.h"

//#include <digitalWriteFast.h>

#include <SPI.h>

#include <limits.h>

#include <ADC.h>
#include <ADC_util.h>

#include <EEPROM.h>

#include <LittleFS.h>

#include "stringlib.h"

#include "S1163901_SPI.h"

LittleFS_Program filesys;
bool filesys_ready = false;

extern float tempmonGetTemp(void);

// ADC setup
ADC *adc = new ADC();

#define MAXADC 8
uint8_t adcpins[MAXADC] = { A0, A1, A2, A3, A4, A5, A6, A7 };

// CPU Cycles per Usec
#define CYCLES_PER_USEC (F_CPU / 1000000)

// Measure elapsed time in cpu clock cycles
uint32_t elapsed_cycles_holder;
inline uint32_t elapsed_cycles() {
  return (ARM_DWT_CYCCNT - elapsed_cycles_holder);
}

inline void elapsed_cycles_start() {
  elapsed_cycles_holder = ARM_DWT_CYCCNT;
}

// Uncomment for Elapsed time in usecs
//elapsedMicros elapsed_usecs;

// ------------------------------------------------------------------

#define thisMANUFACTURER_NAME {'D','R','M','C','N','E','L','S','O','N' }
#define thisMANUFACTURER_NAME_LEN 10

#define thisPRODUCT_NAME {'S','p','e','c','S','1','6','6','3','9','0','1','V','0','1'}
#define thisPRODUCT_NAME_LEN 15
#define thisPRODUCT_SERIAL_NUMBER { 'S','N','0','0','0','0','0','0','0','0','0','1' }
#define thisPRODUCT_SERIAL_NUMBER_LEN 12

extern "C"
{
  struct usb_string_descriptor_struct_manufacturer
  {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[thisMANUFACTURER_NAME_LEN];
  };

  usb_string_descriptor_struct_manufacturer usb_string_manufacturer_name = {
    2 + thisMANUFACTURER_NAME_LEN * 2,
    3,
    thisMANUFACTURER_NAME
  };

  // -------------------------------------------------
  
  struct usb_string_descriptor_struct_product
  {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[thisPRODUCT_NAME_LEN];
  };

  usb_string_descriptor_struct_product usb_string_product_name = {
    2 + thisPRODUCT_NAME_LEN * 2,
    3,
    thisPRODUCT_NAME
  };

  // -------------------------------------------------
  
  struct usb_string_descriptor_struct_serial_number
  {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[thisPRODUCT_SERIAL_NUMBER_LEN];
  };

  usb_string_descriptor_struct_serial_number usb_string_serial_number =
    {
      2 + thisPRODUCT_SERIAL_NUMBER_LEN * 2, 
      3,
      thisPRODUCT_SERIAL_NUMBER
    };
}

// ------------------------------------------------------------------

const char versionstr[] = "T4LCD vers 0.3";

const char authorstr[] =  "Patents Pending and (c) 2024 by Mitchell C. Nelson, Ph.D. ";

void blink() {
  Serial.println( "Error: SPI interface uses the blink ping");
}

/* --------------------------------------------------------
   Extension of the interrupt API, re-enable pin interrupt after dropping pending
 */
#define IMR_INDEX   5
#define ISR_INDEX   6

void resumePinInterrupts(uint8_t pin)
{
	if (pin >= CORE_NUM_DIGITAL) return;
	volatile uint32_t *gpio = portOutputRegister(pin);
	uint32_t mask = digitalPinToBitMask(pin);
        gpio[ISR_INDEX] = mask;  // clear pending
	gpio[IMR_INDEX] |= mask;  // re-enable
}

void disablePinInterrupts(uint8_t pin)
{
	if (pin >= CORE_NUM_DIGITAL) return;
	volatile uint32_t *gpio = portOutputRegister(pin);
	uint32_t mask = digitalPinToBitMask(pin);
	gpio[IMR_INDEX] &= ~mask;
}

/* ===================================================================================================
   External USB command buffer
*/
#define RCVLEN 256
char rcvbuffer[RCVLEN];
uint16_t nrcvbuf = 0;

#define SNDLEN 256
char sndbuffer[SNDLEN];

/* =====================================================================================
   Built-in ADC readout
*/
inline int fastAnalogRead( uint8_t pin ) {
  adc->adc0->startReadFast(pin);
  while ( adc->adc0->isConverting() );
  return adc->adc0->readSingle();
}

/* --------------------------------------------------------
   ADC single reads
   (note that the alpha0 board used the first two channels)
 */
#define NADC_CHANNELS 4
#define ADC_RESOLUTION (3.3/4096.)
uint16_t adc_data[NADC_CHANNELS] = { 0 };

// Set this to something more than 0 to turn on adc reporting.
unsigned int adc_averages = 0;

/* Send ADC readings
 */
void sendADCs( unsigned int averages ) {

  unsigned int i;
  float scalefactor = ADC_RESOLUTION/averages;
  float val;

  int n = averages;

  for( i = 0 ; i < NADC_CHANNELS; i++ ) {
    adc_data[i] = fastAnalogRead( adcpins[i] );
  }
  n--;
  
  while( n-- ) {
    for( i = 0 ; i < NADC_CHANNELS; i++ ) {
      adc_data[i] += fastAnalogRead( adcpins[i] );
    }
  }
  
  Serial.print( "ADCAVGS " );
  Serial.println( averages );
  
  Serial.print( "ADCDATA" );
  for( i = 0 ; i < NADC_CHANNELS; i++ ) {
    val = adc_data[i] * scalefactor;
    Serial.print( " " );
    Serial.print( val, 5 );
  }
  
  Serial.println( "" );
}

/* --------------------------------------------------------
   Send chip temperature
 */

// Set this to something more than 0 to turn on chip temperature reporting
unsigned int chipTemp_averages = 0;

void sendChipTemperature( unsigned int averages ){
  Serial.print( "CHIPTEMPERATURE " );
  Serial.println( tempmonGetTemp() );
}

/* ==========================================================================
   Filesys functions
 */

bool readfromfile( char *name ) {
  File file;
  
  if (!filesys_ready) {
    Serial.println( "Error: filesys not ready" );
    return false;
  }

  if ( !(file = filesys.open( name, FILE_READ )) ) {
    Serial.println( "Error: not able to open for read" );
    return false;
  }

  file.read( (char *) &bufferp[DATASTART], NPIXELS*2 );
  file.close();
  return true;
}

bool writetofile( char *name ) {
  File file;
  
  if (!filesys_ready) {
    Serial.println( "Error: filesys not ready" );
    return false;
  }

  if ( !(file = filesys.open( name, FILE_WRITE_BEGIN )) ) {
    Serial.println( "Error: not able to open for write" );
    return false;
  }

  file.write( (char *) &bufferp[DATASTART], NPIXELS*2 );

  Serial.print( "saved to ");
  Serial.print( file.name() );
  Serial.print( " ");
  Serial.print( NPIXELS*2 );
  Serial.print( " ");
  Serial.println( file.size() );

  file.close();
  
  return true;
}

void listfiles() {
  File dir, entry;
  if (filesys_ready) {
    Serial.print("Space Used = ");
    Serial.println(filesys.usedSize());
    Serial.print("Filesystem Size = ");
    Serial.println(filesys.totalSize());
    dir = filesys.open( "/" );
    while( (entry = dir.openNextFile()) ) {
      Serial.print( entry.name() );
      Serial.print( " " );
      Serial.print( entry.size(), DEC );
      Serial.println();
      entry.close();
    }
    dir.close();
  }
  else {
    Serial.println( "Error: filesys not ready" );
  }
}
  
/* ===================================================================
   Help text
 */
void help() {

  sensor_help();
  
  Serial.println("#Coefficients for pixel number to wwavelength");
  Serial.println("#  store coefficients <a0> <a1> <a2> <a3> (need 4 numbers)");
  Serial.println("#  coefficients       - report");
  Serial.println("#");
  Serial.println("#  store units <string> (upto 6 characters, c.f. nm, um, mm)");
  Serial.println("#  units              - report");
  Serial.println("#");
  Serial.println("#Response function coefficients");
  Serial.println("#  store response <a0> <a1> <a2> <a3>");
  Serial.println("#  response       - report");
  Serial.println("#");
  Serial.println("#Save/recall dark and response spectra");
  Serial.println("#  save dark|resp      - save current buffer as dark or response");
  Serial.println("#  recall dark|resp    - and send it to the host");
  Serial.println("#");
  Serial.println("#  upload int|float    - followed by values one per line from the host");
  Serial.println("#");
  Serial.println("#  save filename       - save from current buffer to internal file");
  Serial.println("#  recall filename     - read from internal file to current buffer and send" );
  Serial.println("#");
  Serial.println("#Identifier string (63 bytes)");
  Serial.println("#  store identifier <identifier>");
  Serial.println("#  identifier         - list identifier string");
  Serial.println("#");
  Serial.println("#Microcontroller functions");
  Serial.println("#  temperature        - report microcontroller temperature");
  Serial.println("#  reboot             - reboots the entire board");
  Serial.println("#");
  Serial.println("#Read and average analog inputs");
  Serial.println("#  adcs <navgs>        - read analog inputs and report");
  Serial.println("#  set adcs <navgs>    - read ADCs at frame completion");
  Serial.println("#  set adcs off");
  Serial.println("#");
}


/* ===================================================================
   The setup routine runs once when you press reset:
*/

void setup() {

  // initialize the digital pin as an output.
#ifdef HASLED
  pinMode(led, OUTPUT);
#endif

  // ------------------------------
  // Setup the onboard ADCs
  adc->adc0->setReference(ADC_REFERENCE::REF_3V3); 
  adc->adc0->setAveraging(1);                 // set number of averages
  adc->adc0->setResolution(12);               // set bits of resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); 
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED); 
  adc->adc0->wait_for_cal();  
  adc->adc0->singleMode();              // need this for the fast read

  // ------------------------------
  Serial.begin(9600);
  delay(100);

  // Patent pending and copyright notice displayed at startup
  Serial.println( versionstr );
  Serial.println( authorstr );
  //Serial.println( salesupportstr );

#ifdef DIAGNOSTICS_CPU 
  Serial.print("F_CPU: "); Serial.print(F_CPU/1e6);  Serial.println(" MHz."); 
  //Serial.print("F_BUS: "); Serial.print(F_BUS/1e6);  Serial.println(" MHz."); 
  Serial.print("ADC_F_BUS: "); Serial.print(ADC_F_BUS/1e6); Serial.println(" MHz.");
#endif

  // ------------------------------
  // Linear CCD sensor

  sensor_setup();
  
  // ------------------------------
  // File system in program RAM

#ifdef S1163901_SPI_H
  if (!filesys.begin( 1024*512 ) ) {
    Serial.println( "not able to setup filesys" );
  }
  else {
    filesys_ready = true;
    Serial.println("filesystem ready");
    listfiles();
  }
#endif
  
#ifdef HASLED
  blink();
  delay(1000);
  blink();
#endif

}

// the loop routine runs over and over again forever:
void loop() {

  uint16_t nlen = 0;
  char *pc, *pc1;
  char c;
  
  unsigned int utemp = 0;

  /* ---------------------------------------------------
     Read serial input until endof line or ctl character
   */
  while ( Serial.available() ) {

    if (is_busy) {
      noInterrupts();
      stop_sensor();
      interrupts();
    }

    c = Serial.read();

    if ( c ) {

      // break at ctl-character or semi-colon
      if ( iscntrl( c ) || c == ';' ) {
        nlen = nrcvbuf;
        rcvbuffer[nrcvbuf] = 0;
        nrcvbuf = 0;
        break;
      }

      // skip leading spaces
      else if ( nrcvbuf || !isspace(c) ) {
        rcvbuffer[nrcvbuf++] = c;        
      }

      if ( nrcvbuf >= RCVLEN ) {
        Serial.println( (char *)"Error: buffer overflow" );
        nrcvbuf = 0;
      }
    }
    
#ifdef DIAGNOSTICS_RCV
    Serial.print( '#' );
    Serial.println( rcvbuffer );
#endif
  }

  /* ====================================================================
   * Command processing
   */

   if ( nlen > 0 ) {
    
     //blink();

     for ( int n = 0; (n < RCVLEN) && rcvbuffer[n] ; n++ ) {
       rcvbuffer[n] = tolower( rcvbuffer[n] );
     }
     pc = rcvbuffer;
     pc1 = pc;

     Serial.println( pc );

     /*-----------------------------------------------------------
       Put this at the top, best chance of getting to it if very busy
     */
     if ( startsWith( rcvbuffer, "reboot") ) {
       _reboot_Teensyduino_();
     }

     /* -----------------------------------------------------------
	Go to the sensor cli first
     */
     
     else if ( sensor_cli(pc, &pc1) || (pc1>pc) ) {
       Serial.println( "Ack: Sensor command recognized" );
     }
     
     /* --------------------------------------------------------------
      */
     else if ( (pc = startsWith( rcvbuffer, "help" )) ) {
      help();
     }
     
    /* -----------------------------------------------------------
       Support for dark and response spectra
     */
    else if ( (pc = startsWith( rcvbuffer, "save dark" )) ) {
       writetofile( (char *) "dark.txt" );
     }

     else if ( (pc = startsWith( rcvbuffer, "recall dark" )) ) {
       if ( readfromfile( (char *) "dark.txt" ) ) {
	 Serial.println( "DARK" );
	 sendData();
       }
     }
    
     else if ( (pc = startsWith( rcvbuffer, "save resp" )) ) {
       writetofile( (char *) "resp.txt" );
     }
    
     else if ( (pc = startsWith( rcvbuffer, "recall resp" )) ) {
       if ( readfromfile( (char *) "resp.txt" ) ) {
	 Serial.println( "RESPONSE" );
	 sendData();
       }
     }

     else if ( (pc = startsWith( rcvbuffer, "save" )) ) {
       while( pc && isspace(*pc) ) pc++;
       if ( pc && *pc ) {
	 writetofile( (char *) pc );
       }
     }
    
     else if ( (pc = startsWith( rcvbuffer, "recall" )) ) {
       while( pc && isspace(*pc) ) pc++;
       if ( readfromfile( (char *) pc ) ) {
	 Serial.print( "RECALLED " );
	 Serial.println( pc );
	 sendData();
       }
     }
    
     else if ( (pc = startsWith( rcvbuffer, "quickformat" )) ) {
       filesys.quickFormat();
       listfiles();
     }
    
     else if ( (pc = startsWith( rcvbuffer, "list files" )) ) {
       listfiles();
     }
    
     else if ( (pc = startsWith( rcvbuffer, "remove" )) ) {
       while( pc && isspace(*pc) ) pc++;
       filesys.remove(pc);
       listfiles();
     }
     
    /* -----------------------------------------------------------
       ADC reporting
    */
    else if ( (pc = startsWith( rcvbuffer, "set adcs off")) ) {
      Serial.println( "setting adc reporting off" );
      adc_averages = 0;
    }
    
    else if ( (pc = startsWith( rcvbuffer, "set adcs")) && (pc = parseUint( pc, &adc_averages )) ) {
      Serial.print( "setting adc reporting, averaging " );
      Serial.println( adc_averages );
    }
    
    else if ( (pc = startsWith( rcvbuffer, "adcs")) || (pc = startsWith( rcvbuffer, "read analog inputs")) ) {
      utemp = 1;
      parseUint( pc, &utemp );
      if (utemp) {
	sendADCs( utemp );
      }
    }

    /* ------------------------------------------------------------
       Temperature reporting
     */
    else if ( (pc = startsWith( rcvbuffer, "set temperature off")) ) {
      Serial.println( "setting chip temperature reporting off" );
      chipTemp_averages = 0;
    }
    
    else if ( (pc = startsWith( rcvbuffer, "set temperature on")) ) {
      Serial.println( "setting chip temperature reporting on" );
      chipTemp_averages = 1;
    }
    
    else if ( !chipTemp_averages && (pc = startsWith( rcvbuffer, "temperature" )) ) {
      Serial.print( "CHIPTEMPERATURE " );
      Serial.println( tempmonGetTemp() );
    }
    
    else {
      Serial.print( "Error: unknown command //" );
      Serial.print( (char *) rcvbuffer );
      Serial.println( "//" );
      
    }

   // Indicate that we processed this message
   nlen = 0;

   Serial.println( "DONE" );
  }
  
}
