/* LedSpiel
(c) 2025 by Malte Marwedel

SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ledspiel.h"

#include "ledspiellib/boxusb.h"
#include "ledspiellib/flash.h"
#include "ledspiellib/leds.h"
#include "ledspiellib/mcu.h"
#include "ledspiellib/rs232debug.h"

#include "main.h"

#include "usbMsc.h"
#include "utility.h"
#include "json.h"

typedef struct {
	bool usbEnabled;
	uint32_t ledCycle;

} ledspielState_t;

ledspielState_t g_ledspielState;


void MainMenu(void) {
	printf("\r\nSelect operation:\r\n");
	printf("h: This screen\r\n");
	printf("r: Reboot with reset controller\r\n");
	printf("b: Jump to ST DFU bootloader\r\n");
	printf("t: Measure storage performance\r\n");
	printf("u: Toggle USB device\r\n");
	printf("w: Toggle USB write protection\r\n");
	printf("p: Toggle print USB performance\r\n");
}

void AppInit(void) {
	LedsInit();
	Led1Yellow();
	/* 48MHz: USB does not work reliable with 32MHz, so this is the minimum.
	   There are power of two scalers for the SD card, which supports 25MHz,
	   so 24MHz can be used. Also mp3 needs around 40MHz, so 48 is propably the best
	   to be used.
	*/
	uint8_t clockError = McuClockToHsePll(96000000, RCC_HCLK_DIV1);
	Rs232Init();
	printf("\r\nLedSpiel %s\r\n", APPVERSION);
	if (clockError) {
		printf("Error, setting up PLL - %u\r\n", clockError);
	}
	/*It looks like, if higher CPU frequencies are used, the divider needs to be
	  at least 4. Selecting a higher APB divider does not help.
	  A divider of 2 for the SPI works with 96MHz, but with 144MHz,
	  reading works and writing fails with the tested SD card.
	  At  168MHz (APB2Div = 4, SPI div = 2 -> 21MHz) reading fails too.
	  But 168MHz (APB2Div = 2, SPI div = 4 -> 21MHz) works.
	*/
	FlashEnable(4);
	StorageInit();
	g_ledspielState.usbEnabled = true;
	Led1Green();
	printf("Ready. Press h for available commands\r\n");
}

void PrepareOtherProgam(void) {
	Led2Off();
	//first all peripheral clocks should be disabled
	UsbStop(); //stops USB clock
	Rs232Flush();
	Rs232Stop(); //stops UART1 clock
}

void JumpDfu(void) {
	uintptr_t dfuStart = ROM_BOOTLOADER_START_ADDRESS;
	Led1Green();
	printf("\r\nDirectly jump to the DFU bootloader\r\n");
	volatile uintptr_t * pProgramStart = (uintptr_t *)(dfuStart + 4);
	printf("Program start will be at 0x%x\r\n", (unsigned int)(*pProgramStart));
	PrepareOtherProgam();
	McuStartOtherProgram((void *)dfuStart, true); //usually does not return
}

bool MetaNameGet(uint8_t * jsonStart, size_t jsonLen, char * nameOut, size_t nameMax) {
	return JsonValueGet(jsonStart, jsonLen, "name", nameOut, nameMax);
}

#define BENCH_BLOCK 512
#define BENCH_SIZE (1024 * 512)

void BenchmarkRead() {
	printf("Reading %uKiB\r\n", BENCH_SIZE / 1024);
	uint8_t buffer[BENCH_BLOCK];
	uint64_t tStart = McuTimestampUs();
	for (uint32_t i = 0; i < BENCH_SIZE; i+= BENCH_BLOCK) {
		FlashRead(i, buffer, BENCH_BLOCK);
	}
	uint64_t tStop = McuTimestampUs();
	uint32_t deltaMs = (tStop - tStart) / 1000;
	uint32_t kbs = BENCH_SIZE * 1000 / deltaMs / 1024;
	printf("Took %ums -> %uKiB/s\r\n", (unsigned int)deltaMs, (unsigned int)kbs);
}

void AppCycle(void) {
	//led flash
	if (g_ledspielState.ledCycle < 25) {
		Led2Green();
	} else {
		Led2Off();
	}
	if (g_ledspielState.ledCycle >= 50) {
		g_ledspielState.ledCycle = 0;
	}
	g_ledspielState.ledCycle++;

	char input = Rs232GetChar();
	if (input) {
		printf("%c", input);
	}
	switch (input) {
		case 'h': MainMenu(); break;
		case 'r': NVIC_SystemReset(); break;
		case 'b': JumpDfu(); break;
		case 't': BenchmarkRead(); break;
		default: break;
	}
	StorageCycle(input);
	HAL_Delay(10); //call this loop ~100x per second
}
