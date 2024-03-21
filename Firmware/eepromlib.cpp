/* ===========================================================================================
   EEPROM support for saving identifier and coordinate mapping constants

   Copyright 2021 by Mitchell C Nelson, PhD
   
 */
#include <Arduino.h>
#include <EEPROM.h>

#include "eepromlib.h"

void eeread( unsigned int address, int nbytes, char *p ) {
  while( nbytes-- > 0  && address < EEPROM_SIZE ) {
    *p++ = EEPROM.read(address++);
  }
}

void eewrite( unsigned int address, int nbytes, char *p ) {
  while( nbytes-- > 0 && address < EEPROM_SIZE ) {
    EEPROM.write(address++, *p++);
  }
}

bool eetestbuffer( char *p, int nlen ) {
  while( nlen ) {
    if (*p && *p != 0xff) {
      return true;
    }
    nlen--;
  }
  return false;
}

// -------------------------------------------------------------

void eereadUntil( unsigned int address, int nbytes, char *p ) {
  char b = 0xFF;
  while( nbytes-- > 0  && address < EEPROM_SIZE && b ) {
    b = EEPROM.read(address++);
    if (b==0xff) b = 0;
    *p++ = b;
  }
}

void eewriteUntil( unsigned int address, int nbytes, char *p ) {
  while( nbytes-- > 0 && address < EEPROM_SIZE-1 && *p ) {
    EEPROM.write(address++, *p++);
  }
  EEPROM.write(address++, 0);
}

void eeErase( unsigned int address, int nbytes ) {
  while( nbytes-- > 0 && address < EEPROM_SIZE-1 ) {
    EEPROM.write(address++, 0xFF );
  }
}


/* -------------------------------------------------------------
   Store/read Identifier
*/
void eraseIdentifier( ) {
  eeErase( EEPROM_ID_ADDR, EEPROM_ID_LEN );
}

void readIdentifier( char *p ) {
  eereadUntil( EEPROM_ID_ADDR, EEPROM_ID_LEN, p );
}

void storeIdentifier( char *p ) {
  eewriteUntil( EEPROM_ID_ADDR, EEPROM_ID_LEN, p );
}

void printIdentifier( ) {
  char buffer[EEPROM_ID_LEN] = { 0 };
  readIdentifier( buffer );
  if (buffer[0]) {
    Serial.print( "Identifier: " ); Serial.println( buffer );
  }
  else {
    Serial.println( "Warning:  identifier is not set" );
  }
}
/* -------------------------------------------------------------
   Store/read Units
*/
void eraseUnits( ) {
  eeErase( EEPROM_UNITS_ADDR, EEPROM_UNITS_LEN );
}

void readUnits( char *p ) {
  eereadUntil( EEPROM_UNITS_ADDR, EEPROM_UNITS_LEN, p );
}

void storeUnits( char *p ) {
  eewriteUntil( EEPROM_UNITS_ADDR, EEPROM_UNITS_LEN, p );
}

void printUnits( ) {
  char buffer[EEPROM_UNITS_LEN] = {0};

  readUnits( buffer );

  if (buffer[0]) {
    Serial.print( "units: " ); Serial.println( buffer );
  }
  else {
    Serial.println( "Warning:  units is not set" );
  }
}

/* -------------------------------------------------------------
   Store/read Wavelength Coeffs
*/

void eraseCoefficients( ) {
  eeErase( EEPROM_COEFF_ADDR, EEPROM_COEFF_LEN );
}

void readCoefficients( float *vals ) {
  eeread( EEPROM_COEFF_ADDR, EEPROM_COEFF_LEN, (char *) vals );
}

void storeCoefficients( float *vals ) {
  eewrite( EEPROM_COEFF_ADDR, EEPROM_COEFF_LEN, (char *) vals );
}

void printCoefficients( ) {

  float vals[EEPROM_NCOEFFS];

  readCoefficients( vals );
  if (eetestbuffer((char *)vals,EEPROM_NCOEFFS)) {
    Serial.print( "coefficients" );
    for( int n = 0; n < EEPROM_NCOEFFS; n++ ) {
      Serial.printf( " %.8g", vals[n] );
    }
    Serial.println("");
  }
  else {
    Serial.println( "Warning:  coefficients is not set" );
  }
}

