#pragma once

#include <stdbool.h>


/* Deinits the peripheral, then starts the progam found at the given address.
   The program must be linked to be working at this address.
   If ledSignalling is true, LEDs are adjusted to LED2 to be green before
   starting the other progam.
   External peripherals should be disabled before calling this function.
   For the STM32L452, using the address 0x1FFF0000 calls the internal
   bootloader.
*/
void McuStartOtherProgram(void * startAddress, bool ledSignalling);

/*For the PC simulation, the constants are not defined.
  Might be true for other platforms in the future too.
*/
#ifdef PC_SIM
#define RCC_HCLK_DIV1 1
#define RCC_HCLK_DIV2 2
#define RCC_HCLK_DIV4 4
#define RCC_HCLK_DIV8 8
#define RCC_HCLK_DIV16 16
#endif

/*
This assumens the MCU is already running on the Hsi.

Valid values for apbDivider:
RCC_HCLK_DIV1
RCC_HCLK_DIV2
RCC_HCLK_DIV4
RCC_HCLK_DIV8
RCC_HCLK_DIV16

returns:
  0: ok
  1: frequency unsupported
  2: setting latency failed
  3: failed to start PLL
  4: failed to set new divider and latency
*/
uint8_t McuClockToHsiPll(uint32_t frequency, uint32_t apbDivider);

uint8_t McuClockToHsePll(uint32_t frequency, uint32_t apbDivider);

//Stops the external clock and the PLL, clock frequency will be 16MHz
uint8_t McuClockToHsi(uint32_t apbDivider);

//GPIOs, which may not be an output, can be locked to a function
void McuLockCriticalPins(void);

uint64_t McuTimestampUs(void);

void McuDelayUs(uint32_t us);

uint32_t McuApbFrequencyGet(void);

uint32_t McuCpuFrequencyGet(void);
