#pragma once

#define F_CPU 48000000

#define USB_BUFFERSIZE_BYTES 2060

#define ROM_BOOTLOADER_START_ADDRESS 0x1FFF0000

#include "ledspiellib/mcu.h"

//To make libmad happy
void abortIncept(void);
