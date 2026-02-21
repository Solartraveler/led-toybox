/* LedSpiel
(c) 2025 by Malte Marwedel

Incomplete. Currently just allows reading the SD card over as MSC on the USB
port. And this is very slow. Writing does not work.


SPDX-License-Identifier: GPL-3.0-or-later
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

#include "ledspiel.h"

#include "ledspiellib/boxusb.h"
#include "ledspiellib/flash.h"
#include "ledspiellib/leds.h"
#include "ledspiellib/mcu.h"
#include "ledspiellib/rs232debug.h"
#include "ledspiellib/stackSampler.h"
#include "ledspiellib/watchdog.h"

#include "audioOut.h"
#include "filesystem.h"
#include "keyInput.h"
#include "main.h"
#include "mp3Playback.h"
#include "sdmmcAccess.h"
#include "usbMsc.h"
#include "utility.h"
#include "json.h"

typedef struct {
	bool usbEnabled;
	bool ledState;
	bool playing;
	uint32_t ledTime;
	float volume;

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
	printf("l: List files\r\n");
	printf("a: Audio test\r\n");
	printf("+: Playback volume increment\r\n");
	printf("-: Playback volume decrement\r\n");
}

void AppInit(void) {
	LedsInit();
	Led1Yellow();
	/* 48MHz: USB does not work reliable with 32MHz, so this is the minimum.
	   There are power of two scalers for the SD card, which supports 25MHz,
	   so 24MHz can be used. Also mp3 needs around 40MHz, so 48MHz is propably the best
	   to be used.
	*/
	uint8_t clockError = McuClockToHsePll(F_CPU, RCC_HCLK_DIV1);
	Rs232Init();
	printf("\r\nLedSpiel %s\r\n", APPVERSION);
	if (clockError) {
		printf("Error, setting up PLL - %u\r\n", clockError);
	}
	StackSampleInit();
	/*It looks like, if higher CPU frequencies are used, the divider needs to be
	  at least 4. Selecting a higher APB divider does not help.
	  A divider of 2 for the SPI works with 96MHz, but with 144MHz,
	  reading works and writing fails with the tested SD card.
	  At  168MHz (APB2Div = 4, SPI div = 2 -> 21MHz) reading fails too.
	  But 168MHz (APB2Div = 2, SPI div = 4 -> 21MHz) works.
	*/
#if (F_CPU > 50000000)
	FlashEnable(4);
#else
	FlashEnable(2);
#endif
	SdmmcCrcMode(SDMMC_CRC_WRITE);
	FilesystemMount();
	g_ledspielState.usbEnabled = false;
	Led1Green();
	g_ledspielState.volume = 1.0;
	printf("Ready. Press h for available commands\r\n");
	StackSampleCheck();
}

static void PrepareOtherProgam(void) {
	Led2Off();
	//first all peripheral clocks should be disabled
	UsbStop(); //stops USB clock
	Rs232Flush();
	Rs232Stop(); //stops UART1 clock
}

static void JumpDfu(void) {
	uintptr_t dfuStart = ROM_BOOTLOADER_START_ADDRESS;
	Led1Green();
	printf("\r\nDirectly jump to the DFU bootloader\r\n");
	volatile uintptr_t * pProgramStart = (uintptr_t *)(dfuStart + 4);
	printf("Program start will be at 0x%x\r\n", (unsigned int)(*pProgramStart));
	PrepareOtherProgam();
	McuStartOtherProgram((void *)dfuStart, true); //usually does not return
}

#define BENCH_BLOCK 512
#define BENCH_SIZE (256 * BENCH_BLOCK)

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

void BenchmarkSdcard(void) {
	SdmmcCrcMode(SDMMC_CRC_NONE);
	printf("\r\nFirst startup after idle:\r\n");
	BenchmarkRead(); //cards tend to be in idle, so the first transfer is slower, this wakes up the card

	printf("\r\nWithout CRC:\r\n");
	BenchmarkRead();

	SdmmcCrcMode(SDMMC_CRC_WRITE);
	printf("\r\nWith writing CRC:\r\n");
	BenchmarkRead();

	SdmmcCrcMode(SDMMC_CRC_READWRITE);
	printf("\r\nWith CRC:\r\n");
	BenchmarkRead();
}

void UsbToggle(void) {
	if (g_ledspielState.usbEnabled == false) {
		PlaybackStop();
		StorageInit();
	} else {
		StorageStop();
	}
	g_ledspielState.usbEnabled = !g_ledspielState.usbEnabled;
}

#define AUDIO_SAMPLERATE 44100
#define AUDIO_SINE_RATE 500

#define AUDIO_SINE_SIZE (AUDIO_SAMPLERATE / AUDIO_SINE_RATE)

//exact multiple of a sine curve, so it can play in a loop without disortion
#define AUDIO_TEST_SIZE (AUDIO_SINE_SIZE)

#define AUDIO_SINE_RATE2 550
#define AUDIO_SINE_SIZE2 (AUDIO_SAMPLERATE / AUDIO_SINE_RATE2)

void AudioTest(void) {
	static uint16_t data[AUDIO_TEST_SIZE];
	float pi2 = (float)M_PI * (float)2.0;
	for (uint32_t i = 0; i < AUDIO_TEST_SIZE; i++) {
		data[i] = ((float)AUIDO_VALUE_MAX/2) + ((float)AUIDO_VALUE_MAX/2) * sinf((float)i * pi2 / (float)AUDIO_SINE_SIZE);
		//printf("%u:%u\r\n", (unsigned int)i, data[i]);
	}
	AudioInit(data, AUDIO_TEST_SIZE, AUDIO_SAMPLERATE);
	uint32_t end = HAL_GetTick();
	while ((HAL_GetTick() - end) < 2000) {
		//printf("Free: %u\r\n", AudioBufferFreeGet());
		//printf("Tim: %u, cnt: %u, 0x%x, DAC: %u\r\n", (unsigned int)TIM7->CNT, (unsigned int)DMA1_Stream2->NDTR, (unsigned int)DMA1_Stream2->M0AR, (unsigned int)(DAC->DHR12R1));
	}
	//and now with 600Hz, but with putting the data to the fifo on demand
	uint16_t data2[AUDIO_SINE_SIZE2];
	for (uint32_t i = 0; i < AUDIO_SINE_SIZE2; i++) {
		data2[i] = ((float)AUIDO_VALUE_MAX/2) + ((float)AUIDO_VALUE_MAX/2) * sinf((float)i * pi2 / (float)AUDIO_SINE_SIZE2);
		//printf("%u:%u\r\n", (unsigned int)i, data[i]);
	}
	uint32_t i = 0;
	end = HAL_GetTick();
	while ((HAL_GetTick() - end) < 2000) {
		while (AudioBufferFreeGet() == 0);
		AudioBufferPut(data2[i]);
		i++;
		if (i >= AUDIO_SINE_SIZE2) {
			i = 0;
		}
		//printf("Tim: %u, cnt: %u, 0x%x, DAC: %u\r\n", (unsigned int)TIM7->CNT, (unsigned int)DMA1_Stream2->NDTR, (unsigned int)DMA1_Stream2->M0AR, (unsigned int)(DAC->DHR12R1));
	}
	AudioStop();
}

void ListFiles(const char * directory, const char * prefix, uint32_t recursive) {
	DIR d;
	FILINFO fi;
	if (f_opendir(&d, directory) == FR_OK) {
		while (f_readdir(&d, &fi) == FR_OK) {
			if (fi.fname[0]) {
				printf("%s%c %8u %s\r\n", prefix, (fi.fattrib & AM_DIR) ? 'd' : ' ', (unsigned int)fi.fsize, fi.fname);
				if ((recursive) && (fi.fattrib & AM_DIR)) {
					char subdirectory[257];
					char subprefix[16];
					snprintf(subdirectory, sizeof(subdirectory), "%s/%s", directory, fi.fname);
					snprintf(subprefix, sizeof(subprefix), "%s  ", prefix);
					ListFiles(subdirectory, subprefix, recursive - 1);
				}
			} else {
				break;
			}
		}
		f_closedir(&d);
	}
}

void PlaySelectFilename(uint32_t dirnum, uint32_t filenum, char * filename, size_t len) {
	DIR d;
	FILINFO fi;
	char dirname[16];
	snprintf(dirname, sizeof(dirname), "/%02u", (unsigned int)dirnum);
	if (f_opendir(&d, dirname) == FR_OK) {
		printf("Opened %s\r\n", dirname);
		while (f_readdir(&d, &fi) == FR_OK) {
			if (fi.fname[0]) {
				printf("%s\r\n", fi.fname);
				if ((EndsWith(fi.fname, ".mp3")) && (fi.fattrib & AM_DIR) == 0) {
					if (filenum == 0) {
						snprintf(filename, len, "%s/%s", dirname, fi.fname);
						break;
					}
					filenum--;
				}
			} else {
				break;
			}
		}
		f_closedir(&d);
	} else {
		printf("Failed to open %s\r\n", dirname);
	}
}

void PlayContinue(void) {
	if (g_ledspielState.playing == false) {
		char filename[128];
		PlaySelectFilename(1, 0, filename, sizeof(filename));
		printf("Selected %s\r\n", filename);
		g_ledspielState.playing = true;
		PlaybackStart(filename, 1.0);
	}
	PlaybackProcess();
}

static void VolumeSet(void) {
	PlaybackVolume(g_ledspielState.volume);
	uint32_t vHi = g_ledspielState.volume;
	uint32_t vLo = (g_ledspielState.volume - (float)vHi) * 1000;
	printf("New volume %u.%03u\r\n", (unsigned int)vHi, (unsigned int)vLo);
}

void VolumeUp(void) {
	g_ledspielState.volume *= 1.125;
	if (g_ledspielState.volume > 4.0) {
		g_ledspielState.volume = 4.0;
	}
	if (g_ledspielState.volume < 0.001) {
		g_ledspielState.volume = 0.0625;
	}
	VolumeSet();
}

void VolumeDown(void) {
	g_ledspielState.volume /= 1.125;
	VolumeSet();
}

void AppCycle(void) {
	uint32_t time = HAL_GetTick();
	if ((time - g_ledspielState.ledTime) >= 250) {
		if (g_ledspielState.ledState) {
			Led2Off();
		} else {
			Led2Green();
		}
		g_ledspielState.ledState = !g_ledspielState.ledState;
		g_ledspielState.ledTime = time;
	}
	char input = Rs232GetChar();
	if (input) {
		printf("%c", input);
	}
	switch (input) {
		case 'h': MainMenu(); break;
		case 'r': NVIC_SystemReset(); break;
		case 'b': JumpDfu(); break;
		case 't': BenchmarkSdcard(); break;
		case 'u': UsbToggle(); break;
		case 'a': AudioTest(); break;
		case 'l': ListFiles("/", "", 2); break;
		case '+': VolumeUp(); break;
		case '-': VolumeDown(); break;
		case 'w':
		case 'p':
			if (g_ledspielState.usbEnabled) {
				StorageCycle(input);
			}
			break;
		default: break;
	}
	if (g_ledspielState.usbEnabled) {
		StorageCycle(0);
	} else {
		PlayContinue();
	}
	WatchdogServe();
	StackSampleCheck();
}

