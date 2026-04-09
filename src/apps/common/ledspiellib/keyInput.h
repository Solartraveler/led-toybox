#pragma once

void KeyInputInit(void);

//Disables the pulldowns, so this saves some power.
void KeyInputDeinit(void);

/*The return value is the order on J1:
  LSB: Pin 2
  MSB: Pin 14
  So there are the lowest 7 bits relevant.
  LSB and MSB are also the Boot0 and Boot1 pins.
*/
uint32_t KeyInputGet(void);
