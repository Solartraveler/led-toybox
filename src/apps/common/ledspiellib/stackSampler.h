#pragma once

/*Call once for init. Will use a timer and its interrupt to periodically
  check the minimum and maximum stack pointer within the ISR. This assumes
  the stack pointers are the same as in the main program. This might give
  useless results if an application with multiple threads is used.*/
void StackSampleInit(void);

/*Prints a message if the values have increased since the last call*/
void StackSampleCheck(void);