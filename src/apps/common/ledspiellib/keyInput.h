#pragma once

void KeyInputInit(void);

//Disables the pulldowns, so this saves some power.
void KeyInputDeinit(void);

/*The return value is the order on J1:
  Bit: J1:     main.h: Pin: Special:
  0:   Pin 2,  Boot1,  PB1, Boot1
  1:   Pin 4,  Key6,   PC10
  2:   Pin 6,  Key5,   PC9
  3:   Pin 8,  Key4,   PC8
  4:   Pin 10, Key3,   PC7
  5:   Pin 12, Key2,   PC6
  6:   Pin 14, Key1,   PD2, Boot0
  So the lowest 7 bits are relevant.
*/
uint32_t KeyInputGet(void);
