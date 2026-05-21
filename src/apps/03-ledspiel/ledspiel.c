/* LedSpiel
(c) 2025 - 2026 by Malte Marwedel

SPDX-License-Identifier: GPL-3.0-or-later

Status:
USB: Partially working: Currently just allows reading the SD card over as MSC on the USB
  port. And this is very slow. Writing does not work.
Playing mp3: Working
Driving LED matrix: Working
Reading brightness: Working
Reading infrared keys: Not working
Playing animations: Working
Reading keys: Working
Charging: Working
Auto power off: Working
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
#include "ledspiellib/chargerControl.h"
#include "ledspiellib/flash.h"
#include "ledspiellib/keyInput.h"
#include "ledspiellib/ledMatrix.h"
#include "ledspiellib/leds.h"
#include "ledspiellib/mcu.h"
#include "ledspiellib/power.h"
#include "ledspiellib/rs232debug.h"
#include "ledspiellib/simpleadc.h"
#include "ledspiellib/stackSampler.h"
#include "ledspiellib/timer32Bit.h"
#include "ledspiellib/watchdog.h"

#include "adc.h"
#include "animationPlayback.h"
#include "audioOut.h"
#include "batteryEmpty.h"
#include "charging.h"
#include "errorSdCard.h"
#include "filesystem.h"
#include "main.h"
#include "mp3Playback.h"
#include "noFiles.h"
#include "noSdCard.h"
#include "sdmmcAccess.h"
#include "usbMsc.h"
#include "usbConnected.h"
#include "utility.h"
#include "json.h"

/*If selected 0, the folder selection + FOLDER_OFFSET will be opened
  so the first folder has to be named 01, not 00.
*/
#define FOLDER_OFFSET 1

#define PLAYBACK_FILES_AUTO_MAX 5

typedef struct {
	bool usbEnabled;
	bool playing;
	bool animation;
	bool animationRepeat;
	bool brightnessManual;
	bool charging;
	bool empty;
	bool simEmpty;
	uint32_t chargedSeconds;
	uint16_t chargeTryReason; //just for printfs to not flood the debug interface
	uint16_t chargeMaxU; //in [mV]
	uint16_t chargeNoRetry; //in [s] if a charge has been stopped, do not restart
	float volume;
	uint16_t brightnessShould;
	uint16_t brightnessIs;
	uint32_t cycleSecond;
	uint32_t cycle10ms;
	uint32_t cyclePowerDown; //if this counter reaches 0, the device is switched off [s]
	uint8_t frameTest;
	uint8_t filesystem; //0 = mounted, card working
	uint32_t inputPrevious;
	uint32_t inputNotChanged; //Time in [10ms]
	uint32_t folderNum; //folder to be selected 0...15
	uint16_t animationFileNum;
	uint16_t playbackFileNum;
	uint16_t playbackFilesAuto; //number of files played since last user input, playback of next file is only done if value is < PLAYBACK_FILES_AUTO_MAX
	bool animationNext; //key or serial debug requests the next animation
	bool playbackNext; //key or serial debug requests the next mp3
	bool folderUpdated; //key or serial debug requests playing of an other folder
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
	printf("0..9, A...F: Select folder 1...16\r\n");
	printf("a: Audio test\r\n");
	printf("b: Jump to ST DFU bootloader\r\n");
	printf("c: Charger state\r\n");
	printf("e: Simulate battery empty\r\n");
	printf("f: LED frame test\r\n");
	printf("h: This help screen\r\n");
	printf("i: CPU idle\r\n");
	printf("l: List files\r\n");
	printf("m: Next mp3\r\n");
	printf("n: Next animation\r\n");
	printf("p: Toggle print USB performance\r\n");
	printf("r: Reboot with reset controller\r\n");
	printf("s: Stop playback\r\n");
	printf("t: Measure storage performance\r\n");
	printf("u: Toggle USB device\r\n");
	printf("w: Toggle USB write protection\r\n");
	printf("x: Decrease brightness\r\n");
	printf("y: Increase brightness\r\n");
	printf("+: Playback volume increment\r\n");
	printf("-: Playback volume decrement\r\n");
}

#define FLIPBYTESU16(X) ((((X) >> 8) & 0xFF) | (((X) << 8) & 0xFF00))
#define FRAME_TEST_NUM 7

void FrameTest(uint8_t id) {
	if (id == 0) {
		printf("Disabled\r\n");
		MatrixDisable();
	} else if (id == 1) { //check a lot of brighness variants in 2 bits per color mode
		printf("2 bit variant with many different colors\r\n");
		MatrixInit(3);
		uint8_t data[MATRIX_X * MATRIX_Y] = {0};
		data[MATRIX_X * 0 + 0] = 0x3; //red
		data[MATRIX_X * 0 + 1] = 0x3 << 2; //green
		data[MATRIX_X * 0 + 2] = 0x3 << 4; //blue

		data[MATRIX_X * 1 + 0] = 0x2;
		data[MATRIX_X * 1 + 1] = 0x2 << 2;
		data[MATRIX_X * 1 + 2] = 0x2 << 4;
		data[MATRIX_X * 1 + 3] = 0x15; //darkest red + green + blue = white
		data[MATRIX_X * 1 + 4] = 0x2A; //medium red + green + blue = white

		data[MATRIX_X * 2 + 0] = 0x1;
		data[MATRIX_X * 2 + 1] = 0x1 << 2;
		data[MATRIX_X * 2 + 2] = 0x1 << 4;
		data[MATRIX_X * 2 + 3] = 0x3F;  //bright red + green + blue = white
		data[MATRIX_X * 2 + 4] = 0x3C;

		data[MATRIX_X * 3 + 0] = 0x0;
		data[MATRIX_X * 3 + 1] = 0x5;
		data[MATRIX_X * 3 + 2] = 0x6;
		data[MATRIX_X * 3 + 3] = 0x7;
		data[MATRIX_X * 3 + 4] = 0x9;

		data[MATRIX_X * 4 + 0] = 0xB;
		data[MATRIX_X * 4 + 1] = 0xC;
		data[MATRIX_X * 4 + 2] = 0xD;
		data[MATRIX_X * 4 + 3] = 0xE;
		data[MATRIX_X * 4 + 4] = 0xF;
		MatrixFrame(1, data);
	} else if (id == 2) { //very simple pattern, look if there are 5 dots who do not move
		printf("2 bit variant, simple pattern\r\n");
		MatrixInit(3);
		uint8_t data[MATRIX_X * MATRIX_Y] = {0};
		data[MATRIX_X * 0 + 0] = 0x3;
		data[MATRIX_X * 0 + 3] = 0x1;
		data[MATRIX_X * 0 + 4] = 0x2;
		data[MATRIX_X * 1 + 1] = 0xC;
		data[MATRIX_X * 2 + 2] = 0x30;
		MatrixFrame(1, data);
	} else if (id == 3) { //15 bit mode, 5 bit per color, 2 bytes per pixel
		printf("5 bit variant\r\n");
		MatrixInit((1 << 5) - 1);
		//sqrt(0x1F) = ~0x5
		uint16_t data[MATRIX_X * MATRIX_Y] = {0};
		data[MATRIX_X * 0 + 0] = FLIPBYTESU16(0x1F); //red maximum
		data[MATRIX_X * 0 + 1] = FLIPBYTESU16(0x05); //medium
		data[MATRIX_X * 0 + 2] = FLIPBYTESU16(0x01); //lowest
		data[MATRIX_X * 1 + 0] = FLIPBYTESU16(0x1F << 5); //green, 2. line
		data[MATRIX_X * 1 + 1] = FLIPBYTESU16(0x05 << 5);
		data[MATRIX_X * 1 + 2] = FLIPBYTESU16(0x01 << 5);
		data[MATRIX_X * 2 + 0] = FLIPBYTESU16(0x1F << 10); //blue 3. line
		data[MATRIX_X * 2 + 1] = FLIPBYTESU16(0x05 << 10);
		data[MATRIX_X * 2 + 2] = FLIPBYTESU16(0x01 << 10);
		MatrixFrame(2, (uint8_t *)data);
	} else if (id == 4) { //24 bit mode, one byte per color, 3 bytes per pixel
		printf("8 bit variant\r\n");
#define BYTES_PER_PIXEL3 3
		MatrixInit(255);
		//sqrt(0xFF) = ~0x10
		uint8_t data[MATRIX_X * MATRIX_Y * BYTES_PER_PIXEL3] = {0};
		data[MATRIX_X * 0 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 0 + 2] = 0xFF; //red maximum
		data[MATRIX_X * 0 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 1 + 2] = 0x10; //medium
		data[MATRIX_X * 0 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 2 + 2] = 0x01; //lowest
		data[MATRIX_X * 1 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 0 + 1] = 0xFF; //green, 2. line
		data[MATRIX_X * 1 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 1 + 1] = 0x10;
		data[MATRIX_X * 1 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 2 + 1] = 0x01;
		data[MATRIX_X * 2 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 0 + 0] = 0xFF; //blue 3. line
		data[MATRIX_X * 2 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 1 + 0] = 0x10;
		data[MATRIX_X * 2 * BYTES_PER_PIXEL3 + BYTES_PER_PIXEL3 * 2 + 0] = 0x01;
		MatrixFrame(BYTES_PER_PIXEL3, data);
	} else if (id == 5) { //27 bit mode, two bytes per color, 6 bytes per pixel
		printf("9 bit variant\r\n");
#define U16_PER_PIXEL3 3
		MatrixInit(511);
		//sqrt(0x1FF) = ~0x16
		uint16_t data[MATRIX_X * MATRIX_Y * BYTES_PER_PIXEL3] = {0};
		data[MATRIX_X * 0 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 0 + 2] = FLIPBYTESU16(0x1FF); //red maximum
		data[MATRIX_X * 0 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 1 + 2] = FLIPBYTESU16(0x016); //medium
		data[MATRIX_X * 0 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 2 + 2] = FLIPBYTESU16(0x001); //lowest
		data[MATRIX_X * 1 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 0 + 1] = FLIPBYTESU16(0x1FF); //green, 2. line
		data[MATRIX_X * 1 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 1 + 1] = FLIPBYTESU16(0x016);
		data[MATRIX_X * 1 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 2 + 1] = FLIPBYTESU16(0x001);
		data[MATRIX_X * 2 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 0 + 0] = FLIPBYTESU16(0x1FF); //blue 3. line
		data[MATRIX_X * 2 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 1 + 0] = FLIPBYTESU16(0x016);
		data[MATRIX_X * 2 * U16_PER_PIXEL3 + U16_PER_PIXEL3 * 2 + 0] = FLIPBYTESU16(0x001);
		MatrixFrame(U16_PER_PIXEL3 * sizeof(uint16_t), (uint8_t*)data);
	} else if (id == 6) { //maximum brightness to check worst case power consumption
		printf("2 bit, all LEDs on\r\n");
		MatrixInit(2);
		uint8_t data[MATRIX_X * MATRIX_Y];
		memset(data, 0x3F, sizeof(data)); //white
		MatrixFrame(1, data);
	}
}

static void AppPlaybackStop(bool autoplayNext) {
	PlaybackStop();
	g_ledspielState.playing = false;
	if (!autoplayNext) {
		g_ledspielState.playbackFilesAuto = PLAYBACK_FILES_AUTO_MAX;
	}
}

static void AppAnimationStop(void) {
	AnimationStop();
	g_ledspielState.animation = false;
}

static void PrepareOtherProgam(void) {
	//first all peripheral clocks should be disabled
	AppAnimationStop();
	AppPlaybackStop(false);
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
	g_ledspielState.usbEnabled = !g_ledspielState.usbEnabled;
	if (g_ledspielState.usbEnabled) {
		AppPlaybackStop(false);
		AppAnimationStop();
		FilesystemUnmount();
		g_ledspielState.filesystem = 3;
		StorageInit();
	} else {
		StorageStop();
		g_ledspielState.filesystem = FilesystemMount();
		g_ledspielState.folderUpdated = true;
	}
}

#define AUDIO_SAMPLERATE 44100
#define AUDIO_SINE_RATE 500

#define AUDIO_SINE_SIZE (AUDIO_SAMPLERATE / AUDIO_SINE_RATE)

//exact multiple of a sine curve, so it can play in a loop without disortion
#define AUDIO_TEST_SIZE (AUDIO_SINE_SIZE)

#define AUDIO_SINE_RATE2 550
#define AUDIO_SINE_SIZE2 (AUDIO_SAMPLERATE / AUDIO_SINE_RATE2)

//Using sinf increases the code size by about 4.2KiB.
#define AUDIO_SAWTOOTH

void AudioTest(void) {
	AppPlaybackStop(false);
	RAM_SUPPORTS_DMA static uint16_t data[AUDIO_TEST_SIZE];
	for (uint32_t i = 0; i < AUDIO_TEST_SIZE; i++) {
#ifdef AUDIO_SAWTOOTH
		float phase = (float)(i) / (float)AUDIO_TEST_SIZE;
		data[i] = (float)AUIDO_VALUE_MAX * phase;
#else
		const float pi2 = (float)M_PI * (float)2.0;
		data[i] = ((float)AUIDO_VALUE_MAX/2) + ((float)AUIDO_VALUE_MAX/2) * sinf((float)i * pi2 / (float)AUDIO_SINE_SIZE);
#endif
		//printf("%u:%u\r\n", (unsigned int)i, data[i]);
	}
	AudioInit(data, AUDIO_TEST_SIZE, AUDIO_SAMPLERATE);
	uint32_t end = HAL_GetTick();
	while ((HAL_GetTick() - end) < 2000) {
		//printf("Free: %u\r\n", AudioBufferFreeGet());
		//printf("Tim: %u, cnt: %u, 0x%x, DAC: %u\r\n", (unsigned int)TIM7->CNT, (unsigned int)DMA1_Stream2->NDTR, (unsigned int)DMA1_Stream2->M0AR, (unsigned int)(DAC->DHR12R1));
	}
	//and now with 600Hz, but with putting the data to the fifo on demand
	RAM_SUPPORTS_DMA static uint16_t data2[AUDIO_SINE_SIZE2];
	for (uint32_t i = 0; i < AUDIO_SINE_SIZE2; i++) {
#ifdef AUDIO_SAWTOOTH
		float phase = (float)(i) / (float)AUDIO_SINE_SIZE2;
		data2[i] = (float)AUIDO_VALUE_MAX * phase;
#else
		const float pi2 = (float)M_PI * (float)2.0;
		data2[i] = ((float)AUIDO_VALUE_MAX/2) + ((float)AUIDO_VALUE_MAX/2) * sinf((float)i * pi2 / (float)AUDIO_SINE_SIZE2);
#endif
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

#define SORT_FILES_MAX 128
#define SORT_CHARS_MAX 3

int PlaySelectCompare(const void * pA, const void * pB) {
	return memcmp(pA, pB, SORT_CHARS_MAX);
}

static void memswp(void * pA, void * pB, size_t elementSize) {
	uint8_t * pua = (uint8_t *)pA;
	uint8_t * pub = (uint8_t *)pB;
	for (size_t i = 0; i < elementSize; i++) {
		uint8_t tmp = *pua;
		*pua = *pub;
		*pub = tmp;
		pua++;
		pub++;
	}
}

//same function as qsort, just slower (and less program memory consuming)
void bubblesort(void * data, size_t elements, size_t elementSize,  int (*compare)(const void * a, const void * b)) {
	bool sorted = false;
	while ((elements > 1) && (!sorted)) {
		sorted = true;
		for (size_t i = 0; i < (elements - 1); i++) {
			void * pA = (void *) ((uintptr_t)data + i * elementSize);
			void * pB = (void *) ((uintptr_t)data + (i + 1) * elementSize);
			if (compare(pA, pB) > 0) {
				memswp(pA, pB, elementSize);
				sorted = false;
			}
		}
		elements--;
	}
}

/*Selects the filenum (0 based) from the directory dirnum.
  Known limit: If filenames starting with an equal name of SORT_CHARS_MAX,
  only the first file will be selected.
  Returns true if a file with the prefix and filenum was found.
*/
bool PlaySelectFilename(uint32_t dirnum, uint32_t filenum, const char * fileEnding, char * filename, size_t len) {
	DIR d;
	FILINFO fi;
	char dirname[16];
	bool found = false;
	snprintf(dirname, sizeof(dirname), "/%02u", (unsigned int)dirnum);
	//1. get all filenames and put the start chars into a buffer
	char toSort[SORT_FILES_MAX * SORT_CHARS_MAX];
	uint32_t numFound = 0;
	if (f_opendir(&d, dirname) == FR_OK) {
		//printf("Opened %s\r\n", dirname);
		while (f_readdir(&d, &fi) == FR_OK) {
			if (fi.fname[0]) {
				//printf("%s\r\n", fi.fname);
				if ((EndsWith(fi.fname, fileEnding)) && (fi.fattrib & AM_DIR) == 0) {
					if (numFound < SORT_FILES_MAX) {
						memcpy(toSort + numFound * SORT_CHARS_MAX, fi.fname, SORT_CHARS_MAX);
						numFound++;
					}
				}
			} else {
				break;
			}
		}
		f_closedir(&d);
	} else {
		printf("Failed to open %s\r\n", dirname);
		return false;
	}
	if (filenum >= numFound) {
		return false;
	}
	//2. okay, sort the files
	//qsort would work, but it takes about 1.6KiB of program memory
	//qsort(toSort, numFound, SORT_CHARS_MAX, &PlaySelectCompare);
	bubblesort(toSort, numFound, SORT_CHARS_MAX, &PlaySelectCompare);
#if 0
	printf("Sorted\r\n");
	for (uint32_t i = 0; i < numFound; i++) {
		for (uint32_t j = 0; j < SORT_CHARS_MAX; j++) {
			printf("%c", toSort[i * SORT_CHARS_MAX + j]);
		}
		printf("\r\n");
	}
#endif
	char fileStart[SORT_CHARS_MAX + 1] = {0};
	memcpy(fileStart, &(toSort[filenum * SORT_CHARS_MAX]), SORT_CHARS_MAX);
	//3. Now get the right file by opening the directory again
	if (f_opendir(&d, dirname) == FR_OK) {
		//printf("Opened %s\r\n", dirname);
		while (f_readdir(&d, &fi) == FR_OK) {
			if (fi.fname[0]) {
				//printf("%s\r\n", fi.fname);
				if ((EndsWith(fi.fname, fileEnding)) && (BeginsWith(fi.fname, fileStart)) && (fi.fattrib & AM_DIR) == 0) {
					snprintf(filename, len, "%s/%s", dirname, fi.fname);
					found = true;
					break;
				}
			} else {
				break;
			}
		}
		f_closedir(&d);
	}
	return found;
}

uint32_t PlayCountFiles(uint32_t dirnum, const char * fileEnding) {
	DIR d;
	FILINFO fi;
	char dirname[16];
	uint32_t num = 0;
	snprintf(dirname, sizeof(dirname), "/%02u", (unsigned int)dirnum);
	if (f_opendir(&d, dirname) == FR_OK) {
		while (f_readdir(&d, &fi) == FR_OK) {
			if (fi.fname[0]) {
				if ((EndsWith(fi.fname, fileEnding)) && (fi.fattrib & AM_DIR) == 0) {
					num++;
				}
			} else {
				break;
			}
		}
		f_closedir(&d);
	} else {
		printf("Failed to open %s\r\n", dirname);
	}
	return num;
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

void BrightnessUp(void) {
	if (g_ledspielState.brightnessIs < 255) {
		g_ledspielState.brightnessIs++;
	}
	MatrixBrightness(g_ledspielState.brightnessIs);
	g_ledspielState.brightnessManual = true;
	printf("Brightness: %u\r\n", (unsigned int)g_ledspielState.brightnessIs);
}

void BrightnessDown(void) {
	if (g_ledspielState.brightnessIs) {
		g_ledspielState.brightnessIs--;
	}
	MatrixBrightness(g_ledspielState.brightnessIs);
	g_ledspielState.brightnessManual = true;
	printf("Brightness: %u\r\n", (unsigned int)g_ledspielState.brightnessIs);
}

void MatrixFrameTest(void) {
	g_ledspielState.frameTest = (g_ledspielState.frameTest + 1) % FRAME_TEST_NUM;
	FrameTest(g_ledspielState.frameTest);
}

bool PlayContinue(void) {
	if (g_ledspielState.filesystem != 0) {
		return false;
	}
	bool work = false;
	if (g_ledspielState.folderUpdated) {
		g_ledspielState.folderUpdated = false;
		g_ledspielState.animationFileNum = 0;
		g_ledspielState.playbackFileNum = 0;
		AppPlaybackStop(true);
		AppAnimationStop();
		g_ledspielState.animationRepeat = false; //so the first animation is played only once
	}

	if (g_ledspielState.playbackNext) {
		g_ledspielState.playbackNext = false;
		AppPlaybackStop(true);
		g_ledspielState.playbackFileNum++;
		uint32_t num = PlayCountFiles(g_ledspielState.folderNum + FOLDER_OFFSET, ".mp3");
		if (g_ledspielState.playbackFileNum >= num) {
			g_ledspielState.playbackFileNum = 0;
		}
	}
	if (g_ledspielState.animationNext) {
		g_ledspielState.animationNext = false;
		AppAnimationStop();
		g_ledspielState.animationFileNum++;
		uint32_t num = PlayCountFiles(g_ledspielState.folderNum + FOLDER_OFFSET, ".ani");
		if (g_ledspielState.animationFileNum >= num) {
			g_ledspielState.animationFileNum = 0;
		}
		//printf("Animation: %u of %u\r\n", (unsigned int)g_ledspielState.animationFileNum, (unsigned int)num);
	}
	if ((g_ledspielState.playing == false) && (g_ledspielState.playbackFilesAuto < PLAYBACK_FILES_AUTO_MAX)) {
		char filename[128];
		if (PlaySelectFilename(g_ledspielState.folderNum + FOLDER_OFFSET, g_ledspielState.playbackFileNum, ".mp3", filename, sizeof(filename))) {
			printf("Selected %s\r\n", filename);
			g_ledspielState.playing = true;
			PlaybackStart(filename, g_ledspielState.volume);
			g_ledspielState.playbackFilesAuto++;
			work = true;
		}
	}
	/*PlaybackProcess is not considered "work", because it adjusts itself to the
	  amount of data processing on the next call. And calling more than 1000/s
	  is not needed.
	*/
	if (g_ledspielState.playing) {
		if (PlaybackProcess() == false) {
			g_ledspielState.playbackNext = true;
		}
	}
	if (g_ledspielState.animation == false) {
		char filename[128];
		if (PlaySelectFilename(g_ledspielState.folderNum + FOLDER_OFFSET, g_ledspielState.animationFileNum, ".ani", filename, sizeof(filename))) {
			printf("Selected %s\r\n", filename);
			g_ledspielState.animation = true;
			AnimationStartFile(filename, g_ledspielState.animationRepeat);
			g_ledspielState.animationRepeat = true; //now all manual selected animation repeat
			work = true;
		}
	}
	//AnimationProcess() is done in the main loop, as this needs to be done even without an SD card
	return work;
}

#define LIGHT_SETPS 8

static void LightMeasure(void) {
	if (g_ledspielState.brightnessManual) {
		return;
	}
	const uint16_t darkIn[LIGHT_SETPS] =     {0x8B00, 0x8800, 0x8600, 0x8400, 0x8200, 0x8000, 0x4000, 0}; //lower means brighter surrounding light
	const uint16_t brightOut[LIGHT_SETPS]  = {32    ,     64,     96,    128,    160,    192,    224, 255}; //higher means brighter LEDs
	uint16_t adc = AdcLightLevelGet();
	for (uint32_t i = 0; i < LIGHT_SETPS; i++) {
		if (adc >= darkIn[i]) {
			g_ledspielState.brightnessShould = brightOut[i];
			break;
		}
	}
	//printf("Adc %x (%u) -> brightness %u\r\n", (unsigned int)adc, (unsigned int)(adc & 0x3FFF), (unsigned int)g_ledspielState.brightnessShould);
}

static void LightAdjust(void) {
	if (g_ledspielState.brightnessManual) {
		return;
	}
	if (g_ledspielState.brightnessShould > g_ledspielState.brightnessIs) {
		g_ledspielState.brightnessIs++;
		MatrixBrightness(g_ledspielState.brightnessIs);
		//printf("New brightness: %u\r\n", (unsigned int)g_ledspielState.brightnessIs);
	}
	if (g_ledspielState.brightnessShould < g_ledspielState.brightnessIs) {
		g_ledspielState.brightnessIs--;
		MatrixBrightness(g_ledspielState.brightnessIs);
		//printf("New brightness: %u\r\n", (unsigned int)g_ledspielState.brightnessIs);
	}
}

static void PrintTemperature(int32_t temperature) {
	if (temperature < 0) {
		printf("-");
		temperature = -temperature;
	}
	uint32_t degree = temperature / 1000;
	uint32_t sub = temperature % 1000;
	printf("Temperature %u.%03u°C\r\n", (unsigned int)degree, (unsigned int)sub);
}

static void BatEmptyPrepare(void) {
	g_ledspielState.empty = true;
	g_ledspielState.cyclePowerDown = 15;
	AppPlaybackStop(false);
	if (g_ledspielState.filesystem == 0) {
		FilesystemUnmount();
		g_ledspielState.filesystem = 3;
	}
	FlashDisable();
	AnimationStartRam(build_batteryEmpty_ani, build_batteryEmpty_ani_len, false);
}

static void BatteryControl(void) {
	/*If there is no battery connected, parasitic charge builds up, resulting in a
	  higher voltage on the first measurement. Therfore we measure twice if charging is disabled.
	  Typically the MCP1640 needs 0.65V to start up.
	  There is a voltage offset between the multimeter and the MCU because of D84.
	  Measured values if the charger is disabled:
	  By MCU:      By multimeter:
	  0.48V-0.50V, 0.48V-0.52V               No battery is connected -> Must be running on USB
	  0.48V-0.50V, 0.48V-0.52V               No battery is connected -> Must be running on USB
	  0.48V-0.50V, 0V, 0mA(1)                Short circuit           -> Must be running on USB
	  2.46V,       2.55V                     Charged batteries connected, USB connected
	  2.35V-2.37V, 2.51V                     Charged batteries connected, USB disconnected
	  2.35V-2.37V, 2.51V                     Charged batteries connected, USB disconnected
	  2.95V-2.99V, 3.04V                     Non rechargeable batteries connected, USB connected

	  Measured values if the charger is enabled:
	  2.86V,       2.96V                     No battery is connected
	  0.48V-0.50V, 0V, 216mA(2)              Short circuit           -> Must be running on USB
	  1.53V-1.58V, 107mA(1)                  Short circuit           -> Must be running on USB
	  2.57V-2.64V, 2.62V, 15mA(1), 30mA(2)   Charged batteries connected, USB connected
	  2.35V-2.37V, 2.51V                     Charged batteries connected, USB disconnected
	  2.57V-2.62V  2.66V, 20mA(1), 22mA(2)   Defective batteries connected, USB connected

	 (1) measured by 200mA current measurement
	 (2) measured by voltage drop of R88
	*/
	int32_t temperature = AdcTemperatureGet();
	if (g_ledspielState.charging == false) {
		uint32_t cV = AdcBatteryVoltageGet();
		cV = AdcBatteryVoltageGet();
		if (g_ledspielState.simEmpty) {
			cV = 1600; //2 NIMH cells defintion of empty
		}
		if (cV < 800) {
			if (g_ledspielState.chargeTryReason != 1) {
				printf("No battery or defective (%umV)\r\n", (unsigned int)cV);
				g_ledspielState.chargeTryReason = 1;
			}
		} else if (cV > 2500) {
			if (g_ledspielState.chargeTryReason != 2) {
				printf("Full batteries or non rechargeable ones (%umV)\r\n", (unsigned int)cV);
				g_ledspielState.chargeTryReason = 2;
			}
		} else if (temperature >= 45000) {
			if (g_ledspielState.chargeTryReason != 3) {
				printf("Temperature too high\r\n");
				PrintTemperature(temperature);
				g_ledspielState.chargeTryReason = 2;
			}
		} else if (g_ledspielState.chargeNoRetry == 0) {
			g_ledspielState.chargeMaxU = 0;
			if (g_ledspielState.chargeTryReason != 4) {
				printf("Try to charge (%umV)\r\n", (unsigned int)cV);
			}
			ChargerControl(true);
			HAL_Delay(2);
			uint32_t cV2 = AdcBatteryVoltageGet();
			if (g_ledspielState.simEmpty) {
				cV2 = 1600; //2 NIMH cells defintion of empty
			}
			//typically, the voltage increases by about 30mV (resistance of cables, chargers, batteries)
			if ((cV + 20) < cV2) {
				printf("Charging %umV (+%umV)\r\n", (unsigned int)cV2, (unsigned int)(cV2 - cV));
				g_ledspielState.charging = true;
				g_ledspielState.chargeTryReason = 5;
			} else {
				if (g_ledspielState.chargeTryReason != 4) {
					printf("Running on batteries (%umV)\r\n", (unsigned int)cV2);
					g_ledspielState.chargeTryReason = 4;
				}
				ChargerControl(false);
				if (cV < 1900) { //This is the value estimated if one battery is full and one close to 0.9V
					if (g_ledspielState.empty == false) {
						printf("Battery empty (%umV)\r\n", (unsigned int)cV2);
						BatEmptyPrepare();
					}
				} else {
					if (g_ledspielState.chargeTryReason != 4) {
						printf("Running on batteries (%umV)\r\n", (unsigned int)cV2);
						g_ledspielState.chargeTryReason = 4;
					}
				}
			}
		} else {
			g_ledspielState.chargeNoRetry--;
		}
	} else {
		g_ledspielState.chargedSeconds++;
		uint32_t cV2 = AdcBatteryVoltageGet();
		if (g_ledspielState.chargeMaxU < cV2) {
			g_ledspielState.chargeMaxU = cV2;
		}
		if (g_ledspielState.chargedSeconds > (60 * 60 * 14)) {
			printf("Stop charge - timeout (%umV)\r\n", (unsigned int)cV2);
			ChargerControl(false);
			g_ledspielState.charging = false;
			g_ledspielState.chargeNoRetry = 60 * 60;
		} else if (cV2 < 1200) {
			printf("Stop charge - short circuit or defective (%umV)\r\n", (unsigned int)cV2);
			ChargerControl(false);
			g_ledspielState.charging = false;
			g_ledspielState.chargeNoRetry = 60;
		} else if (cV2 > 2700) {
			printf("Stop charge - no battery or defective (%umV)\r\n", (unsigned int)cV2);
			ChargerControl(false);
			g_ledspielState.charging = false;
			g_ledspielState.chargeNoRetry = 60;
		} else if (cV2 > 2650) {
			printf("Stop charge - battery full or defective (%umV)\r\n", (unsigned int)cV2);
			ChargerControl(false);
			g_ledspielState.charging = false;
			g_ledspielState.chargeNoRetry = 60;
		} else if (cV2 + 50 < g_ledspielState.chargeMaxU) {
			uint32_t drop = g_ledspielState.chargeMaxU - cV2;
			printf("Stop charge - battery full or USB detatched (%umV, -%umV)\r\n", (unsigned int)cV2, (unsigned int)drop);
			ChargerControl(false);
			g_ledspielState.charging = false;
			g_ledspielState.chargeNoRetry = 5;
		} else if (temperature > 50000) {
			printf("Stop charge - too hot (%umV)\r\n", (unsigned int)cV2);
			PrintTemperature(temperature);
			ChargerControl(false);
			g_ledspielState.charging = false;
			g_ledspielState.chargeNoRetry = 60;
		}
	}
}

static void ChargerState(void) {
	uint32_t cV = AdcBatteryVoltageGet();
	printf("%s %umV max %umV %us\r\n", g_ledspielState.charging ? "charging" : "disabled",
	       (unsigned int)cV, (unsigned int)g_ledspielState.chargeMaxU, (unsigned int)g_ledspielState.chargedSeconds);
}

static void SimBatEmpty(void) {
	g_ledspielState.simEmpty = !g_ledspielState.simEmpty;
	printf("Simulate empty battery: %u\r\n", g_ledspielState.simEmpty);
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

static void ProcessDebug(void) {
	char input = Rs232GetChar();
	if (input) {
		printf("%c", input);
	}
	switch (input) {
		case 'a': AudioTest(); break;
		case 'b': JumpDfu(); break;
		case 'c': ChargerState(); break;
		case 'e': SimBatEmpty(); break;
		case 'f': MatrixFrameTest(); break;
		case 'h': MainMenu(); break;
		case 'i': CpuIdlePrint(); break;
		case 'l': ListFiles("/", "", 2); break;
		case 'r': NVIC_SystemReset(); break;
		case 's': AppPlaybackStop(false); break;
		case 't': BenchmarkSdcard(); break;
		case 'u': UsbToggle(); break;
		case 'y': BrightnessUp(); break;
		case 'x': BrightnessDown(); break;
		case '+': VolumeUp(); break;
		case '-': VolumeDown(); break;
		case '0' ... '9':
		  g_ledspielState.folderNum = input - '0';
		  g_ledspielState.folderUpdated = true;
		  g_ledspielState.playbackFilesAuto = 0;
		  break;
		case 'A' ... 'F':
		  g_ledspielState.folderNum = input - 'A' + 10;
		  g_ledspielState.folderUpdated = true;
		  g_ledspielState.playbackFilesAuto = 0;
		  break;
		case 'm':
		  g_ledspielState.playbackNext = true;
		  g_ledspielState.playbackFilesAuto = 0;
		  break;
		case 'n':
		  g_ledspielState.animationNext = true;
		  g_ledspielState.playbackFilesAuto = 0;
		  break;
		default: break;
		if (g_ledspielState.usbEnabled) {
			StorageCycle(input);
		}
		break;
	}
}

//static state keys
#define INPUT_FOLDER 0x3C
//push button
#define INPUT_ANIMATION 0x02
//push button
#define INPUT_MUSIC 0x40

static void ProcessKeys(void) {
	/*Intended bit meaning
	  0: unused
	  1: Next animation
	  2...5: Folder selection
	  6: Next music
	*/
	uint32_t keyInput = KeyInputGet();
	if (keyInput != g_ledspielState.inputPrevious) {
		printf("Key states: %x\r\n", (unsigned int)keyInput);
		if ((keyInput & INPUT_FOLDER) != (g_ledspielState.inputPrevious & INPUT_FOLDER)) {
			g_ledspielState.folderNum = (keyInput & INPUT_FOLDER) >> 2;
			g_ledspielState.folderUpdated = true;
			g_ledspielState.playbackFilesAuto = 0;
		}
		if (keyInput & INPUT_ANIMATION) {
			g_ledspielState.animationNext = true;
			g_ledspielState.playbackFilesAuto = 0;
		}
		if (keyInput & INPUT_MUSIC) {
			g_ledspielState.playbackNext = true;
			g_ledspielState.playbackFilesAuto = 0;
		}
		g_ledspielState.inputPrevious = keyInput;
		g_ledspielState.inputNotChanged = 0;
	} else if (g_ledspielState.inputNotChanged < 0xFFFFFFFF) {
		g_ledspielState.inputNotChanged++;
		if (g_ledspielState.inputNotChanged == 300) { //3s
			if ((keyInput & INPUT_ANIMATION) && (keyInput & INPUT_MUSIC)) {
				UsbToggle();
			} else if (keyInput & INPUT_MUSIC) {
				AppPlaybackStop(false);
			}
		}
	}
}

void AppInit(void) {
	LedsInit();
	Led1Yellow();
	WatchdogStart(20000); //Might already be started by the loader (we don't know)
	/* 48MHz: USB does not work reliable with 32MHz, so this is the minimum.
	   There are power of two scalers for the SD card, which supports 25MHz,
	   so 24MHz can be used. Also mp3 needs around 40MHz, so 48MHz is propably the best
	   to be used.
	*/
	uint8_t clockError = McuClockToHsePll(F_CPU, RCC_HCLK_DIV1);
	Rs232Init();
	printf("\r\nLedSpiel %s\r\n", APPVERSION);
	printf("(c) 2025 - 2026 Malte Marwedel\r\nLicense: GPL V3.0 or later\r\n");
	if (clockError) {
		printf("Error, setting up PLL - %u\r\n", clockError);
	}
	g_ledspielState.cyclePowerDown = 60 * 60 * 24;
	KeyInputInit();
	StackSampleInit();
	AdcInit();
	uint16_t uAvcc = AdcAvccGet();
	uAvcc = AdcAvccGet();
	uint16_t uBattery = AdcBatteryVoltageGet();
	printf("Battery: %umV, Vcc: %umV\r\n", (unsigned int)uBattery, (unsigned int)uAvcc);
	int32_t temperature = AdcTemperatureGet();
	PrintTemperature(temperature);
	BatteryControl(); //checks if the battery is empty
	/*It looks like, if higher CPU frequencies are used, the divider needs to be
	  at least 4. Selecting a higher APB divider does not help.
	  A divider of 2 for the SPI works with 96MHz, but with 144MHz,
	  reading works and writing fails with the tested SD card.
	  At  168MHz (APB2Div = 4, SPI div = 2 -> 21MHz) reading fails too.
	  But 168MHz (APB2Div = 2, SPI div = 4 -> 21MHz) works.
	*/
	if (g_ledspielState.empty == false) {
#if (F_CPU > 50000000)
		FlashEnable(4);
#else
		FlashEnable(2);
#endif
		if (FlashReady()) {
			SdmmcCrcMode(SDMMC_CRC_WRITE);
			g_ledspielState.filesystem = FilesystemMount();
			g_ledspielState.usbEnabled = false;
			g_ledspielState.playbackFilesAuto = 0;
			Led1Green();
		} else {
			g_ledspielState.filesystem = 1;
		}
	} else {
		g_ledspielState.filesystem = 1;
	}
	g_ledspielState.volume = 1.0;
	g_ledspielState.brightnessIs = 255;
	g_ledspielState.brightnessShould = g_ledspielState.brightnessIs;
	printf("Ready. Press h for available commands\r\n");
	StackSampleCheck();
	if (g_ledspielState.empty == false) {
		if (g_ledspielState.filesystem == 1) {
			AnimationStartRam(build_noSdCard_ani, build_noSdCard_ani_len, true);
		} else if (g_ledspielState.filesystem > 1) {
			AnimationStartRam(build_errorSdCard_ani, build_errorSdCard_ani_len, true);
		}
	}
	Rs232Flush();
}

//called every second
static void AppCycle1s(void) {
	WatchdogServe();
	LightMeasure();
	StackSampleCheck();
	BatteryControl();
	CpuIdleCalc();
	if (g_ledspielState.cyclePowerDown) {
		g_ledspielState.cyclePowerDown--;
	} else {
		printf("Powering down\r\n");
		Rs232Flush();
		FlashDisable();
		PowerOff(); //will not return
	}
}

//called every 10ms
static void AppCycle10ms(void) {
	LightAdjust();
	ProcessDebug();
	ProcessKeys();
}

void AppCycle(void) {
	bool work = false;
	if (g_ledspielState.usbEnabled) {
		if (g_ledspielState.animation == false) {
			AnimationStartRam(build_usbConnected_ani, build_usbConnected_ani_len, true);
			g_ledspielState.animation = true;
		}
		work = StorageCycle(0);
	} else {
		work = PlayContinue();
	}
	if (g_ledspielState.frameTest == 0) {
		if (AnimationProcess() == false) {
			g_ledspielState.animationNext = true;
		}
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
