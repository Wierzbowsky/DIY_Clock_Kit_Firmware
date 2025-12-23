// LED functions for 4-digit seven segment led (normal)

#include <stdint.h>
#include "config.h"

// index into ledtable[]
#define LED_BLANK  10
#define LED_DASH   11
#define LED_TEMP   12

static const uint8_t ledtable[] = {
  // dp,g,f,e,d,c,b,a
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111, // 9 with d segment
  0b00000000, // 10 - ' '
  0b01000000, // 11 - '-'

  #if CFG_TEMP_UNIT == 'F'
    0b01110001, // F
  #else
    0b00111001, // C
  #endif
};
