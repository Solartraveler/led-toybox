#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "flashPlatform.h"

//Not thread safe
void FlashEnable(uint32_t clockPrescaler);

//Not thread safe
void FlashDisable(void);

//Thread safe if peripheralMt.c is used
uint16_t FlashGetStatus(void);

//Thread safe if peripheralMt.c is used
bool FlashRead(uint64_t address, uint8_t * buffer, size_t len);

/* len must be a multiple of FLASHPAGESIZE.
Thread safe if peripheralMt.c is used. If two threads are writing at overlapping
memory, each page will only contain data from one thread. But it is not defined
from which.
*/
bool FlashWrite(uint64_t address, const uint8_t * buffer, size_t len);

/*Thread safe if peripheralMt.c is used
  Returns the size in bytes.
*/
uint64_t FlashSizeGet(void);

//Thread safe
uint32_t FlashBlocksizeGet(void);

//Thread safe if peripheralMt.c is used
bool FlashReady(void);
