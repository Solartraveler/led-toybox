/* Ledspiellib based on Boxlib
(c) 2024, 2025 by Malte Marwedel

SPDX-License-Identifier: BSD-3-Clause

*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ledspiellib/flash.h"

#include "ledspiellib/spiGeneric.h"
#include "sdmmcAccess.h"

#include "main.h"

//These values must fit together and are defined in the datasheet
#define SPIPORT SPI1

#define DMASTREAMTX DMA2_Stream3
#define DMASTREAMTXCHANNEL 3
#define DMASTREAMTXCLEARFLAGS (DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3)
#define DMASTREAMTXCLEARREG (DMA2->LIFCR)
#define DMASTREAMTXCOMPLETEFLAG DMA_LISR_TCIF3
#define DMASTREAMTXCOMPLETEREG (DMA2->LISR)

#define DMASTREAMRX DMA2_Stream2
#define DMASTREAMRXCHANNEL 3
#define DMASTREAMRXCLEARFLAGS (DMA_LIFCR_CTCIF2 | DMA_LIFCR_CHTIF2 | DMA_LIFCR_CTEIF2 | DMA_LIFCR_CDMEIF2 | DMA_LIFCR_CFEIF2)
#define DMASTREAMRXCLEARREG (DMA2->LIFCR)
#define DMASTREAMRXCOMPLETEFLAG DMA_LISR_TCIF2
#define DMASTREAMRXCOMPLETEREG (DMA2->LISR)



static bool g_flashInitSuccess;

static void FlashChipSelect(bool selected) {
	GPIO_PinState state = GPIO_PIN_SET;
	if (selected) {
		state = GPIO_PIN_RESET;
	}
	HAL_GPIO_WritePin(SdCs_GPIO_Port, SdCs_Pin, state);
}

static void SdTransfer(const uint8_t * dataOut, uint8_t * dataIn, size_t len, uint8_t /*chipSelect*/, bool resetChipSelect) {
	FlashChipSelect(true);
#if 1
	//Use polling
	SpiGenericPolling(SPIPORT, dataOut, dataIn, len);
#else
	//Use DMA
	uint8_t transferMode = SpiPlatformTransferBackground(SPIPORT, DMASTREAMTX, DMASTREAMRX,
	                &DMASTREAMTXCLEARREG, DMASTREAMTXCLEARFLAGS,
	                &DMASTREAMRXCLEARREG, DMASTREAMRXCLEARFLAGS,dataOut, dataIn, len);
	if (transferMode) {
		/*The nops are in place because I had some strange timing issues when using
		  the DMA with the ADC. There at least one NOP was needed, otherwise the bit
		  was set already on the first check. The second NOP is just to be sure.
		*/
		asm volatile ("nop");
		asm volatile ("nop");
		//According to the datasheet the stream needs to be disabled before the peripheral
		while ((DMASTREAMTXCOMPLETEREG & DMASTREAMTXCOMPLETEFLAG) == 0);
		if (transferMode == 2) {
			while ((DMASTREAMRXCOMPLETEREG & DMASTREAMRXCOMPLETEFLAG) == 0);
			SpiPlatformDisableDma(DMASTREAMRX);
		}
		SpiPlatformDisableDma(DMASTREAMTX);
		SpiPlatformWaitDone(SPIPORT);
	}
#endif
	if (resetChipSelect) {
		FlashChipSelect(false);
	}
}

void FlashEnable(uint32_t clockPrescaler) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = SdOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SdOn_GPIO_Port, &GPIO_InitStruct);

	HAL_GPIO_WritePin(SdOn_GPIO_Port, SdOn_Pin, GPIO_PIN_RESET);

	GPIO_InitStruct.Pin = SdCs_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SdCs_GPIO_Port, &GPIO_InitStruct);

	HAL_GPIO_WritePin(SdCs_GPIO_Port, SdCs_Pin, GPIO_PIN_SET);

	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	__HAL_RCC_SPI1_CLK_ENABLE();
	SpiPlatformInit(SPIPORT);

	__HAL_RCC_DMA2_CLK_ENABLE();
	SpiPlatformInitDma(SPIPORT, DMASTREAMTX, DMASTREAMRX, DMASTREAMTXCHANNEL, DMASTREAMRXCHANNEL);
	SpiGenericPrescaler(SPIPORT, 256);
	if (SdmmcInit(&SdTransfer, 0) == 0) {
		g_flashInitSuccess = true;
		SpiGenericPrescaler(SPI1, clockPrescaler);
	}
}

void FlashDisable(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = SdCs_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SdCs_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = SdOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SdOn_GPIO_Port, &GPIO_InitStruct);

	__HAL_RCC_SPI1_CLK_DISABLE();
	g_flashInitSuccess = false;
}

uint16_t FlashGetStatus(void) {
	return 0;
}

bool FlashReady(void) {
	return g_flashInitSuccess;
}

uint32_t FlashBlocksizeGet(void) {
	return SDMMC_BLOCKSIZE;
}

uint64_t FlashSizeGet(void) {
	return SdmmcCapacity() * SDMMC_BLOCKSIZE;
}

bool FlashRead(uint64_t address, uint8_t * buffer, size_t len) {
	return SdmmcRead(buffer, address / SDMMC_BLOCKSIZE, len / SDMMC_BLOCKSIZE);
}

bool FlashWrite(uint64_t address, const uint8_t * buffer, size_t len) {
	return SdmmcWrite(buffer, address / SDMMC_BLOCKSIZE, len / SDMMC_BLOCKSIZE);
}
