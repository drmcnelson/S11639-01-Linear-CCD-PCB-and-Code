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

#include <Arduino.h>

#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#include "stringlib.h"

unsigned int wordLength( char *s ) {
  char *s0 = s;
  while( *s && !isspace( *s ) ){
    s++;
  }
  return (s-s0);  
}

char *nextWord( char *s ) {
  while ( s  && *s && !isspace(*s) ) {
    s++;
  }
  while ( s  && *s && isspace(*s) ) {
    s++;
  }
  if ( s && *s ) {
    return s;
  }
  return 0;   
}

unsigned int countWords( char *s ) {
  unsigned int n = 0;
  if ( s && *s && !isspace(*s) ) {
    n++;
  }
  while( (s=nextWord(s)) ) {
    n++;
  }
  return n;
}

char *startsWith( char *s, const char *key ) {

  int n = strlen(key);

  if ( s && *s && key && *key ) {

    // skip leading spaces
    while ( *s && isspace(*s) ) s++;
      
    if ( *s && !strncmp( s, key, n ) ) {
      return s + n;
    }
    
  }
  return 0;
}

char *parseUint( char *s, unsigned int *u ) {
  unsigned long int l;
  char *p = s;
  if ( p && *p ) {
    l = strtoul( s, &p, 0 );
    if ( (p > s) && (l <= UINT_MAX) ) {
      *u = (unsigned int) l;
      return p;
    }
  }
  return 0;
}

char *parseUint32( char *s, uint32_t *u ) {
  unsigned long int l;
  char *p = s;
  if ( p && *p ) {
    l = strtoul( s, &p, 0 );
    if ( (p > s) && (l <= UINT32_MAX) ) {
      *u = (uint32_t) l;
      return p;
    }
  }
  return 0;
}

char *parseFlt( char *s, float *p ) {
  char *endptr = s;
  *p = strtof( s, &endptr );
  if (endptr > s) {
    return endptr;
  }
  return 0;
}

unsigned int parseUints( char *pc, unsigned int *p, unsigned int nmax ) {
  unsigned int n = 0;
  while( n < nmax && (pc = parseUint( pc, p ) ) ) {
    n++;
    p++;
  }
  return n;
}

unsigned int parseUint32s( char *pc, uint32_t *p, unsigned int nmax ) {
  unsigned int n = 0;
  while( n < nmax && (pc = parseUint32( pc, p ) ) ) {
    n++;
    p++;
  }
  return n;
}

unsigned int parseFlts( char *pc, float *p, unsigned int nmax ) {
  unsigned int n = 0;
  while( n < nmax && (pc = parseFlt( pc, p ) ) ) {
    n++;
    p++;
  }
  return n;
}

// parse each word as usecs, else as float seconds, convert to usecs
unsigned int parseUsecs( char *pc, uint32_t *p, unsigned int nmax ) {
  char *pc1 = pc;
  uint32_t ptemp = 0;
  float ftemp = 0;
  unsigned int n = 0;
  while( n < nmax && pc && pc[0] ) {
    pc1 = parseUint32(pc,&ptemp);
    if (pc1 && pc1[0] && isspace(pc1[0])) {
      *p++ = ptemp;
      n++;
      pc = pc1;
    }
    else if (parseFlt(pc,&ftemp)) {
      *p++ = (uint32_t) (ptemp * 1.E6);
      n++;
      pc = pc1;
    }
    else {
      break;
    }
  }
  return n;
}

  
