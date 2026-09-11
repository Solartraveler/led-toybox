/* UsbDebug
(c) 2026 by Malte Marwedel

SPDX-License-Identifier: Apache-2.0

*/

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "usbDebug.h"

#include "ledspiellib/boxusb.h"
#include "ledspiellib/leds.h"
#include "ledspiellib/mcu.h"
#include "ledspiellib/power.h"
#include "ledspiellib/rs232debug.h"
#include "ledspiellib/stackSampler.h"
#include "ledspiellib/timer32Bit.h"
#include "ledspiellib/watchdog.h"

#include "main.h"
#include "usbBulk.h"
#include "utility.h"

typedef struct {
	bool usbEnabled;
	uint32_t cycleSecond;
	uint32_t cycle10ms;
	uint32_t ticksSleep1s; //for performance measurement only
	uint32_t ticksSleep10s; //for performance measurement only
	uint8_t cpuLoad1s; //for performance measurement only
	uint8_t cpuLoadCnt; //for performance measurement only
	uint8_t cpuLoad10s; //for performance measurement only
	uint32_t mainLoopCycles; //for performance measurement only
	uint32_t mainLoopCycles1s; //for performance measurement only
} ledspielState_t;

ledspielState_t g_ledspielState;


void MainMenu(void) {
	printf("\r\nSelect operation:\r\n");
	printf("1..9: Send bulk packets\r\n");
	printf("h: This help screen\r\n");
	printf("i: CPU idle\r\n");
	printf("p: Toggle print USB performance\r\n");
	printf("r: Reset\r\n");
	printf("u: Toggle USB device\r\n");
}

void UsbToggle(void) {
	g_ledspielState.usbEnabled = !g_ledspielState.usbEnabled;
	if (g_ledspielState.usbEnabled) {
		UsbBulkStart();
	} else {
		UsbBulkStop();
	}
}

static void CpuIdleCalc(void) {
	g_ledspielState.cpuLoad1s = 100.0 - (float)g_ledspielState.ticksSleep1s * 100.0 / (float)F_CPU;
	g_ledspielState.ticksSleep1s = 0;
	g_ledspielState.cpuLoadCnt++;
	g_ledspielState.mainLoopCycles1s = g_ledspielState.mainLoopCycles;
 	g_ledspielState.mainLoopCycles = 0;
	if (g_ledspielState.cpuLoadCnt == 10) {
		g_ledspielState.cpuLoadCnt = 0;
		g_ledspielState.cpuLoad10s = 100.0 - (float)g_ledspielState.ticksSleep10s * 10.0 / (float)F_CPU;
		g_ledspielState.ticksSleep10s = 0;
	}
}

static void CpuIdlePrint(void) {
	printf("CPU load 1s: %u%c, 10s: %u%c\r\n", g_ledspielState.cpuLoad1s, '%', g_ledspielState.cpuLoad10s, '%');
	printf("Main loop cycles 1s: %u\r\n", (unsigned int)g_ledspielState.mainLoopCycles1s);
}

static void UsbDebugSend(uint32_t packets) {
	uint8_t data[10 * 64];
	size_t l = packets * 64;
	for (uint32_t i = 0; i < l; i++) {
		data[i] = i;
	}
	UsbBulkQueue(data, l);
}

static void ProcessDebug(void) {
	char input = Rs232GetChar();
	if (input) {
		printf("%c", input);
	}
	switch (input) {
		case 'h': MainMenu(); break;
		case 'i': CpuIdlePrint(); break;
		case 'r': NVIC_SystemReset(); break;
		case 'u': UsbToggle(); break;
		case '1' ... '9':
			UsbDebugSend(input - '0');
		  break;
		default: break;
		if (g_ledspielState.usbEnabled) {
			UsbBulkCycle(input);
		}
		break;
	}
}

void AppInit(void) {
	LedsInit();
	Led1Yellow();
	//WatchdogStart(20000); //Might already be started by the loader (we don't know)
	/* 48MHz: USB does not work reliable with 32MHz, so this is the minimum.
	   There are power of two scalers for the SD card, which supports 25MHz,
	   so 24MHz can be used. Also mp3 needs around 40MHz, so 48MHz is propably the best
	   to be used.
	*/
	uint8_t clockError = McuClockToHsePll(F_CPU, RCC_HCLK_DIV1);
	Rs232Init();
	printf("\r\nUsbDebug %s\r\n", APPVERSION);
	printf("(c) 2026 Malte Marwedel\r\nLicense: Apache 2.0\r\n");
	if (clockError) {
		printf("Error, setting up PLL - %u\r\n", clockError);
	}
	StackSampleInit();
	UsbBulkInit();
	g_ledspielState.usbEnabled = true;
	printf("Ready. Press h for available commands\r\n");
	StackSampleCheck();
	Rs232Flush();
	Rs232GetChar(); //clear possible junk data
}

//called every second
static void AppCycle1s(void) {
	WatchdogServe();
	CpuIdleCalc();
}

//called every 10ms
static void AppCycle10ms(void) {
	ProcessDebug();
}

void AppCycle(void) {
	bool work = false;
	if (g_ledspielState.usbEnabled) {
		work = UsbBulkCycle(0);
	}
	uint32_t tick = HAL_GetTick();
	if (g_ledspielState.cycleSecond < tick) {
		g_ledspielState.cycleSecond += 1000;
		AppCycle1s();
	}
	if (g_ledspielState.cycle10ms < tick) {
		g_ledspielState.cycle10ms += 10;
		AppCycle10ms();
	}
	if (!work) {
		//This reduces the CPU load and therefore saves power
		Timer32BitInit(0);
		Timer32BitStart();
		__WFI();
		/*Failure in calculation: Servicing the ISR routine is counted as idle,
		  but there is not much done in the ISRs, as long as USB is disabled.
		*/
		uint32_t idle = Timer32BitGet();
#if (F_CPU > 84000000)
		idle *= 2;
#endif
		Timer32BitDeinit();
		g_ledspielState.ticksSleep1s += idle;
		g_ledspielState.ticksSleep10s += idle;
	}
	g_ledspielState.mainLoopCycles++;
}
