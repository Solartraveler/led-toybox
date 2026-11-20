/* LedSpiel
(c) 2026 by Malte Marwedel

SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "animationPlayback.h"

#include "ff.h"
#include "ledspiellib/ledMatrix.h"
#include "main.h"

//Describes our .ani format
#define ANI_TYPE_END 0
#define ANI_TYPE_HEADER 1
#define ANI_TYPE_FRAME 2

#define ANI_HEADER_SIZE 12
#define ANI_FRAME_METADATA_SIZE 3
#define ANI_FRAME_BYTES_PER_PIXEL_MAX 6
//should also at least 12 which is ANIMATION_HEADER_SIZE
#define ANI_MAX_FRAME_SIZE (ANI_FRAME_METADATA_SIZE + MATRIX_X * MATRIX_Y * ANI_FRAME_BYTES_PER_PIXEL_MAX)

typedef struct {
	bool running; //if true, the animation is running
	bool repeat;
	uint8_t bytesPerPixel;
	size_t len; //length of animationData
	size_t rptr; //read pointer of dataRam
	const uint8_t * dataRam; //if NULL, the running animation is a file
	FIL file; //file to read the input data from
	uint32_t wait; //timestamp in [ms] until the next frame should be read
	uint8_t fileBuffer[ANI_MAX_FRAME_SIZE];
} animationState_t;

static animationState_t g_animationState;

static const uint8_t * AnimationDataGet(size_t len) {
	if (g_animationState.running == false) {
		return NULL;
	}
	if (g_animationState.dataRam) {
		if ((g_animationState.rptr) + len <= g_animationState.len) {
			const uint8_t * rptr = g_animationState.dataRam + g_animationState.rptr;
			g_animationState.rptr += len;
			return rptr;
		}
	} else {
		if (len > ANI_MAX_FRAME_SIZE) {
			return NULL;
		}
		UINT r = 0;
		FRESULT rRead = f_read(&(g_animationState.file), g_animationState.fileBuffer, len, &r);
		if ((rRead == FR_OK) && (r == len)) {
			return g_animationState.fileBuffer;
		}
	}
	return NULL;
}

bool AnimationProcess(void) {
	if (!g_animationState.running) {
		return false;
	}
	if (HAL_GetTick() < g_animationState.wait) {
		return true;
	}
	size_t dataSize;
	size_t frameSize;
	bool widen;
	if (g_animationState.bytesPerPixel) {
		dataSize = g_animationState.bytesPerPixel * MATRIX_X * MATRIX_Y;
		widen = false;
	} else {
		dataSize = (MATRIX_X * MATRIX_Y + 1) / 2;
		widen = true;
	}
	frameSize = dataSize + ANI_FRAME_METADATA_SIZE;
	const uint8_t * pFrame = AnimationDataGet(frameSize);
	if ((!pFrame) || (pFrame[0] != ANI_TYPE_FRAME)) {
		if (g_animationState.repeat) {
			if (g_animationState.dataRam) {
				g_animationState.rptr = ANI_HEADER_SIZE;
			} else {
				f_lseek(&(g_animationState.file), ANI_HEADER_SIZE);
			}
			return true;
		}
		return false;
	}
	uint16_t delay = (pFrame[1] << 8) | pFrame[2];
	g_animationState.wait += delay;
	if (widen == false) {
		MatrixFrame(g_animationState.bytesPerPixel, pFrame + ANI_FRAME_METADATA_SIZE);
	} else {
		//make a 1 byte per pixel format out of a 0.5 bytes per pixel format
		memmove(g_animationState.fileBuffer, pFrame + ANI_FRAME_METADATA_SIZE, dataSize);
		const uint8_t table[8] = {0, 0x3, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F};
		for (size_t i = dataSize; i-- > 0;) {
			uint8_t twoPixels = g_animationState.fileBuffer[i];
			uint8_t highPixel = (twoPixels >> 4) & 0x7;
			uint8_t lowPixel = twoPixels & 0x7;
			g_animationState.fileBuffer[i * 2] = table[lowPixel];
			g_animationState.fileBuffer[i * 2 + 1] = table[highPixel];
		}
		MatrixFrame(1, g_animationState.fileBuffer);
	}
	return true;
}

static bool AnimationStart(void) {
	const uint8_t * pFrameHeader = AnimationDataGet(ANI_HEADER_SIZE);
	if (!pFrameHeader) {
		printf("  reading failed\r\n");
		return false;
	}
	if (pFrameHeader[0] != ANI_TYPE_HEADER) {
		printf("  no metadata\r\n");
		return false;
	}
	if ((pFrameHeader[5] != 1) && (pFrameHeader[5] != 2)) { //unsupported format version
		printf("  unsupported version\r\n");
		return false;
	}
	uint16_t bitsPerColor = pFrameHeader[6];
	uint16_t bitsPerColorMatrix = bitsPerColor;
	if ((bitsPerColor < 1) || (bitsPerColor > 16)) {
		printf("  unsupported bit number\r\n");
		return false;
	}
	if (bitsPerColorMatrix == 1) {
		bitsPerColorMatrix = 2;
	}
	uint16_t sx = (pFrameHeader[7] << 8) | pFrameHeader[8];
	uint16_t sy = (pFrameHeader[9] << 8) | pFrameHeader[10];
	if ((sx != MATRIX_X) || (sy != MATRIX_Y)) {
		printf("  unsupported size\r\n");
		return false;
	}
	if (!MatrixInit((1 << bitsPerColorMatrix) - 1)) {
		printf("  unsupported bit number\r\n");
		return false;
	}

	g_animationState.bytesPerPixel = 0; //actually 0.5 bytes per pixel
	if (bitsPerColor > 1) {
		g_animationState.bytesPerPixel = 1;
	}
	if (bitsPerColor > 2) {
		g_animationState.bytesPerPixel = 2;
	}
	if (bitsPerColor > 5) {
		g_animationState.bytesPerPixel = 3;
	}
	if (bitsPerColor > 8) {
		g_animationState.bytesPerPixel = 6;
	}
	g_animationState.running = true;
	AnimationProcess();
	return true;
}

bool AnimationStartRam(const uint8_t * pData, size_t len, bool repeat) {
	printf("Starting buildin animation\r\n");
	g_animationState.wait = HAL_GetTick();
	g_animationState.len = len;
	g_animationState.dataRam = pData;
	g_animationState.rptr = 0;
	g_animationState.repeat = repeat;
	g_animationState.running = true;
	if (!AnimationStart()) {
		AnimationStop();
		printf("...failed\r\n");
		return false;
	}
	return true;
}

bool AnimationStartFile(const char * filename, bool repeat) {
	printf("Starting file animation\r\n");
	g_animationState.wait = HAL_GetTick();
	if (f_open(&g_animationState.file, filename, FA_READ) != FR_OK) {
		printf("...failed to open\r\n");
		return false;
	}
	g_animationState.len = f_size(&g_animationState.file);
	g_animationState.dataRam = NULL;
	g_animationState.rptr = 0;
	g_animationState.repeat = repeat;
	g_animationState.running = true;
	if (!AnimationStart()) {
		AnimationStop();
		return false;
	}
	return true;
}

void AnimationStop(void) {
	if (g_animationState.running == false) {
		return;
	}
	g_animationState.running = false;
	if (g_animationState.dataRam == false) {
		f_close(&g_animationState.file);
	}
	g_animationState.dataRam = NULL;
	MatrixDisable();
}
