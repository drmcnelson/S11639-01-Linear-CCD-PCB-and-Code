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


#ifndef STRINGLIB_H
#define STRINGLIB_H

unsigned int wordLength( char *s );

char *nextWord( char *s );

unsigned int countWords( char *s );
  
char *startsWith( char *s, const char *key );
  
char *parseUint( char *s, unsigned int *u );
  
char *parseUint32( char *s, uint32_t *u );
  
char *parseFlt( char *s, float *p );
  
unsigned int parseUints( char *pc, unsigned int *p, unsigned int nmax );
  
unsigned int parseUint32s( char *pc, uint32_t *p, unsigned int nmax );
  
unsigned int parseFlts( char *pc, float *p, unsigned int nmax );
  
// parse each word as usecs, else as float seconds, convert to usecs
unsigned int parseUsecs( char *pc, uint32_t *p, unsigned int nmax );

#endif
