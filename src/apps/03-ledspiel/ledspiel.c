/* LedSpiel
(c) 2025 - 2026 by Malte Marwedel

SPDX-License-Identifier: GPL-3.0-or-later

Status:
USB: Partially working: Currently just allows reading the SD card over as MSC on the USB
  port. And this is very slow. Writing does not work.
Playing mp3: Working
Driving LED matrix: Working
Reading brightness: Not working
Reading infrared keys: Not working
Playing animations: Not working
Reading keys: Not working
Charging: Not working

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
#include "ledspiellib/ledMatrix.h"
#include "ledspiellib/leds.h"
#include "ledspiellib/mcu.h"
#include "ledspiellib/rs232debug.h"
#include "ledspiellib/stackSampler.h"
#include "ledspiellib/watchdog.h"

#include "audioOut.h"
#include "charging.h"
#include "errorSdCard.h"
#include "filesystem.h"
#include "keyInput.h"
#include "main.h"
#include "mp3Playback.h"
#include "noFiles.h"
#include "noSdCard.h"
#include "sdmmcAccess.h"
#include "usbMsc.h"
#include "usbConnected.h"
#include "utility.h"
#include "json.h"

typedef struct {
	bool usbEnabled;
	bool playing;
	float volume;
	uint16_t brightness;
	uint8_t frameTest;
	uint8_t filesystem; //0 = mounted, card working
	bool animation; //if true, the animation is running
	bool animationRepeat;
	uint32_t animationWait; //timestamp in [ms] until the next frame should be read
	size_t animationLen; //length of animationData
	size_t animationRptr; //read pointer of animationData
	const uint8_t * animationDataRam;
	uint8_t animationBytesPerPixel;

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
	printf("y: Increase brightness\r\n");
	printf("x: Decrease brightness\r\n");
	printf("c: LED frame test\r\n");
}

#define FLIPBYTESU16(X) ((((X) >> 8) & 0xFF) | (((X) << 8) & 0xFF00))

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
	}
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
	if (g_ledspielState.filesystem != 0) {
		return;
	}
	if (g_ledspielState.playing == false) {
		char filename[128];
		PlaySelectFilename(1, 0, filename, sizeof(filename));
		printf("Selected %s\r\n", filename);
		g_ledspielState.playing = true;
		PlaybackStart(filename, g_ledspielState.volume);
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

void BrightnessUp(void) {
	if (g_ledspielState.brightness < 255) {
		g_ledspielState.brightness++;
	}
	MatrixBrightness(g_ledspielState.brightness);
	printf("Brightness: %u\r\n", (unsigned int)g_ledspielState.brightness);
}

void BrightnessDown(void) {
	if (g_ledspielState.brightness) {
		g_ledspielState.brightness--;
	}
	MatrixBrightness(g_ledspielState.brightness);
	printf("Brightness: %u\r\n", (unsigned int)g_ledspielState.brightness);
}

void MatrixFrameTest(void) {
	g_ledspielState.frameTest = (g_ledspielState.frameTest + 1) % 6;
	FrameTest(g_ledspielState.frameTest);
}

const uint8_t * MatrixAnimationDataGet(size_t len) {
	if (g_ledspielState.animationDataRam) {
		if ((g_ledspielState.animationRptr) + len <= g_ledspielState.animationLen) {
			const uint8_t * rptr = g_ledspielState.animationDataRam + g_ledspielState.animationRptr;
			g_ledspielState.animationRptr += len;
			return rptr;
		}
	}
	return NULL;
}

void MatrixAnimationServe(void) {
	if (g_ledspielState.frameTest) {
		return;
	}
	if (HAL_GetTick() < g_ledspielState.animationWait) {
		return;
	}
	size_t frameSize = g_ledspielState.animationBytesPerPixel * MATRIX_X * MATRIX_Y + 3;
	const uint8_t * pFrame = MatrixAnimationDataGet(frameSize);
	if ((!pFrame) || (pFrame[0] != 2)) {
		if (g_ledspielState.animationRepeat) {
			g_ledspielState.animationRptr = 12;
		}
		return;
	}
	uint16_t delay = (pFrame[1] << 8) | pFrame[2];
	g_ledspielState.animationWait += delay;
	MatrixFrame(g_ledspielState.animationBytesPerPixel, pFrame + 3);
}

bool MatrixAnimationStart(void) {
	const uint8_t * pFrameHeader = MatrixAnimationDataGet(12);
	if (!pFrameHeader) {
		return false;
	}
	if (pFrameHeader[0] != 1) { //no meta information
		return false;
	}
	if (pFrameHeader[5] != 1) { //unsupported format version
		return false;
	}
	uint16_t bitsPerColor = pFrameHeader[6];
	if ((bitsPerColor < 2) || (bitsPerColor > 16)) {
		return false;
	}
	uint16_t sx = (pFrameHeader[7] << 8) | pFrameHeader[8];
	uint16_t sy = (pFrameHeader[9] << 8) | pFrameHeader[10];
	if ((sx != MATRIX_X) || (sy != MATRIX_Y)) {
		return false;
	}
	if (!MatrixInit((1 << bitsPerColor) - 1)) {
		return false;
	}
	g_ledspielState.animationBytesPerPixel = 1;
	if (bitsPerColor > 2) {
		g_ledspielState.animationBytesPerPixel = 2;
	}
	if (bitsPerColor > 5) {
		g_ledspielState.animationBytesPerPixel = 3;
	}
	if (bitsPerColor > 8) {
		g_ledspielState.animationBytesPerPixel = 6;
	}
	g_ledspielState.animation = true;
	MatrixAnimationServe();
	return true;
}

void MatrixAnimationStartRam(const uint8_t * pData, size_t len, bool repeat) {
	printf("Starting buildin animation\r\n");
	g_ledspielState.animationWait = HAL_GetTick();
	g_ledspielState.animationLen = len;
	g_ledspielState.animationDataRam = pData;
	g_ledspielState.animationRptr = 0;
	g_ledspielState.animationRepeat = true;
	if (!MatrixAnimationStart()) {
		printf("...failed\r\n");
	}
}

void AppInit(void) {
	LedsInit();
	Led1Yellow();
	/* 48MHz: USB does not work reliable with 32MHz, so this is the minimum.
	   There are power of two scalers for the SD card, which supports 25MHz,
	   so 24MHz can be used. Also mp3 needs around 40MHz, so 48MHz is propably the best
	   to be used.
	   It turns out, while mp3 plays with 48MHz, the DMA transfer from the SD card
	   gets data errors as soon as the DMA is used for the LED matrix too.
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
	if (FlashReady()) {
		SdmmcCrcMode(SDMMC_CRC_WRITE);
		g_ledspielState.filesystem = FilesystemMount();
		g_ledspielState.usbEnabled = false;
		Led1Green();
	} else {
		g_ledspielState.filesystem = 1;
	}
	g_ledspielState.volume = 1.0;
	g_ledspielState.brightness = 255;
	printf("Ready. Press h for available commands\r\n");
	StackSampleCheck();
	if (g_ledspielState.filesystem == 1) {
		MatrixAnimationStartRam(build_noSdCard_ani, build_noSdCard_ani_len, true);
	} else if (g_ledspielState.filesystem > 1) {
		MatrixAnimationStartRam(build_errorSdCard_ani, build_errorSdCard_ani_len, true);
	}
}

void AppCycle(void) {
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
		case 'y': BrightnessUp(); break;
		case 'x': BrightnessDown(); break;
		case 'c': MatrixFrameTest(); break;
			if (g_ledspielState.usbEnabled) {
				StorageCycle(input);
			}
			break;
		default: break;
	}
	if (g_ledspielState.usbEnabled) {
		if (g_ledspielState.animation == false) {
			MatrixAnimationStartRam(build_usbConnected_ani, build_usbConnected_ani_len, true);
		}
		StorageCycle(0);
	} else {
		PlayContinue();
	}
	WatchdogServe();
	MatrixAnimationServe();
	StackSampleCheck();
}

