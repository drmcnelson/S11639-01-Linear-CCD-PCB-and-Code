/* ===========================================================================================
   EEPROM support for saving identifier and coordinate mapping constants

   Copyright 2021 by Mitchell C Nelson, PhD
   
 */

#ifndef EEPROMLIB_H
#define EEPROMLIB_H

#define EEPROM_SIZE 1080

#define EEPROM_ID_ADDR 0
#define EEPROM_ID_LEN 64

#define EEPROM_COEFF_ADDR 64
#define EEPROM_NCOEFFS 4
#define EEPROM_COEFF_LEN ( EEPROM_NCOEFFS * sizeof( float) )

#define EEPROM_RESP_ADDR (EEPROM_COEFF_ADDR + EEPROM_COEFF_LEN)
#define EEPROM_NRESPS 4
#define EEPROM_RESP_LEN ( EEPROM_NRESPS * sizeof( float) )

#define EEPROM_UNITS_ADDR ( EEPROM_RESP_ADDR + EEPROM_RESP_LEN )
#define EEPROM_NUNITS 8
#define EEPROM_UNITS_LEN EEPROM_NUNITS


void eeread( unsigned int address, int nbytes, char *p );
void eewrite( unsigned int address, int nbytes, char *p );

void eereadUntil( unsigned int address, int nbytes, char *p );
void eewriteUntil( unsigned int address, int nbytes, char *p );
void eeErase( unsigned int address, int nbytes );

/* -------------------------------------------------------------
   Store/read Identifier
*/
void eraseIdentifier( );
void readIdentifier( char *p );
void storeIdentifier( char *p );

void printIdentifier( );

/* -------------------------------------------------------------
   Store/read Units
*/
void eraseUnits( );
void readUnits( char *p );
void storeUnits( char *p );

void printUnits( );

/* -------------------------------------------------------------
   Store/read Wavelength Coeffs
*/

void eraseCoefficients( );
void readCoefficients( float *vals );
void storeCoefficients( float *vals );

void printCoefficients( );

#endif
