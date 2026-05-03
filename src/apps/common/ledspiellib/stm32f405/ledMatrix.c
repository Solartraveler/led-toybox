/* Ledspiellib
(c) 2025-2026 by Malte Marwedel

SPDX-License-Identifier:  BSD-3-Clause

Some things to consider:

g_matrixBuffer and g_lineDriver must be in SRAM1 or SRAM2, not in CCM RAM.
The GPIOs are AHB1 peripherals - only DMA2 can access them.
And only TIM1 and TIM8 can trigger DMA2 channels.
And only TIM1 can trigger TIM8, not the other way round.

Then there is errata 2.2.19 "DMA2 data corruption occurs when managing AHB and
APB2 peripherals in a concurrent way".

As workaround the addresses for M0AR and PAR needs to be switched and the
transfer direction becomes peripheral->memory, when in fact the data is
transferred in the other direction.

Without this workaround, the matrix would still work - as long as the DMA2 is
not used for any other transfer. But it is used for the SD card with SPI1.
Then the following would happen:
The bug in the errata results in wrong data to be written to R10_GPIO_Port->ODR
and Line1_GPIO_Port->BSRR.
For R10_GPIO_Port this will stay unnoticed as the right data are written within
some microseconds again and PB1 is configured as input, so noting happens here.
But for Line1_GPIO_Port this results in values written to the other pins
configured as output. And one of the pins is the SD card chip select. A random
enable/disable results in wrong data to be read from the SD card. The SD card
driver only detects this if CRC read checks are enabled. Otherwise it can only
be notified by the application getting corrupted data.

Line1_Pin to Line5_Pin should all be on the same PORT

All the R10_Pin ... should all be on the same PORT. And if other outputs are
on these port too, a pattern for BSRR instead of ODR needs to be generated.
This would double the required memory amount.
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ledspiellib/ledMatrix.h"

//Should provide F_CPU, RAM_SUPPORTS_DMA and all other definitions for the register accesses
#include "main.h"

#define DMA_STREAMX DMA2_Stream5

//Instead of Stream1, Stream4 with TIM8_CH3 could be used, but then other
//values must be adjusted too.
#define DMA_STREAMY DMA2_Stream1

static_assert(MATRIX_X * MATRIX_COLORS_PER_PIXEL <= 16, "Error, one port only supports 16 channels");

RAM_SUPPORTS_DMA static uint16_t g_matrixBuffer[MATRIX_COLORS_VAL_MAX * MATRIX_Y];
static uint16_t g_colorMax;
/*
0 = nothing is shown
MATRIX_DIM_FACTOR_MAX = maximum brighness
*/
static uint16_t g_brightFactor = MATRIX_DIM_FACTOR_MAX;

RAM_SUPPORTS_DMA static uint32_t g_lineDriver[MATRIX_DIM_FACTOR_MAX * MATRIX_Y];

void MatrixStop(void) {
	if (TIM1->CR1 & TIM_CR1_CEN) {
		while(TIM8->CNT == 0);
		//printf("Wait for CNT to be non 0\r\n");
		while(TIM8->CNT != 0); //so now TIM1 should be very near to 0 too
	}
	TIM1->CR1 = 0;
	TIM8->CR1 = 0;
	DMA_STREAMX->CR &= ~DMA_SxCR_EN;
	while (DMA_STREAMX->CR & DMA_SxCR_EN);

	DMA_STREAMY->CR &= ~DMA_SxCR_EN;
	while (DMA_STREAMY->CR & DMA_SxCR_EN);
}

void MatrixStart(void) {
	uint32_t transfersX = g_colorMax * MATRIX_Y;
	uint32_t transfersY = MATRIX_Y * MATRIX_DIM_FACTOR_MAX;

	uint32_t transfersXsecond = transfersX * MATRIX_REFRESHRATE * MATRIX_DIM_FACTOR_MAX;

	/* Min and max estimate:
	   168MHz, 1 color -> 1 transfer every 336000 clocks
	   48Mhz, 1024 colors, 5lines, g_brightFactor 8 -> 1 transfer every 11 clocks
	*/

	uint32_t timerXperiod = F_CPU / transfersXsecond;
	uint32_t timerXprescaler = timerXperiod / 65500;
	timerXperiod /= (timerXprescaler + 1);

	//Timer1 is responsible for the X data
	TIM1->CR1 = 0;
	TIM1->CR2 = (0b010 << TIM_CR2_MMS_Pos); //MMS=010 -> update event for slave timer
	TIM1->PSC = timerXprescaler;
	TIM1->CNT = 0;
	TIM1->ARR = timerXperiod;
	TIM1->DIER |= TIM_DIER_UDE;

	/*Timer8 is the slave of timer 1 and responsible for the line (Y) data.
	  It will overflow and trigger a DMA event after the data for each row have been transferred
	*/
	TIM8->CR1 = 0;
	TIM8->CR2 = 0;
	TIM8->PSC = 0;
	TIM8->CNT = g_colorMax - 1;
	TIM8->ARR = g_colorMax - 1;
	TIM8->DIER |= TIM_DIER_UDE;
	TIM8->SMCR = (0b000 << TIM_SMCR_TS_Pos) | (0b111 << TIM_SMCR_SMS_Pos);  //Select TIM1 as trigger (ITR0), External clock mode 1
	TIM8->CR1 |= TIM_CR1_CEN;

	//setup the X dma channel
	DMA2->HIFCR = DMA_HIFCR_CFEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CTEIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTCIF5;
	DMA_STREAMX->M0AR = (uint32_t)&((R10_GPIO_Port->ODR)); //applying errata
	DMA_STREAMX->PAR = (uint32_t)g_matrixBuffer; //applying errata
	DMA_STREAMX->NDTR = transfersX;
	//TIM1_UP channel, periph->memory, 16Bit, circular
	DMA_STREAMX->CR = (6 << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PINC |
	                    DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_CIRC;
	DMA_STREAMX->CR |= DMA_SxCR_EN;

	//setup the Y dma channel
	DMA2->LIFCR = DMA_LIFCR_CFEIF1 | DMA_LIFCR_CDMEIF1 | DMA_LIFCR_CTEIF1 | DMA_LIFCR_CHTIF1 | DMA_LIFCR_CTCIF1;
	DMA_STREAMY->M0AR = (uint32_t)&(Line1_GPIO_Port->BSRR); //applying errata
	DMA_STREAMY->PAR = (uint32_t)g_lineDriver; //applying errata
	DMA_STREAMY->NDTR = transfersY;
	//TIM8_UP, periph->memory, 32Bit, circular
	DMA_STREAMY->CR = (7 << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PINC |
	                   DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1 | DMA_SxCR_CIRC;
	DMA_STREAMY->CR |= DMA_SxCR_EN;

	//printf("Tim1: %u Tim8: %u, DMA2-5 %u, DMA2-1: %u\r\n", TIM1->CNT, TIM8->CNT, DMA_STREAMX->NDTR, DMA_STREAMY->NDTR);

	//Now start the whole LED matrix
	TIM1->CR1 |= TIM_CR1_CEN;
}

static void MatrixLinesCreate(void) {
	//printf("Bright factor: %u\r\n", (unsigned int)g_brightFactor);
	for (uint32_t i = 0; i < MATRIX_DIM_FACTOR_MAX; i++) {
		uint32_t offset = i * MATRIX_Y;
		//recreate line driver array (GPIO Port set and reset register values)
		if (i < g_brightFactor) {
			g_lineDriver[offset + 0] = Line1_Pin | ((Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16);
			g_lineDriver[offset + 1] = Line2_Pin | ((Line1_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16);
			g_lineDriver[offset + 2] = Line3_Pin | ((Line1_Pin | Line2_Pin | Line4_Pin | Line5_Pin) << 16);
			g_lineDriver[offset + 3] = Line4_Pin | ((Line1_Pin | Line2_Pin | Line3_Pin | Line5_Pin) << 16);
			g_lineDriver[offset + 4] = Line5_Pin | ((Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin) << 16);
		} else {
			g_lineDriver[offset + 0] = ((Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16); //all lines off
			g_lineDriver[offset + 1] = ((Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16); //all lines off
			g_lineDriver[offset + 2] = ((Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16); //all lines off
			g_lineDriver[offset + 3] = ((Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16); //all lines off
			g_lineDriver[offset + 4] = ((Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin) << 16); //all lines off
		}
	}
}

bool MatrixInit(uint16_t colorMax) {
	if ((colorMax > MATRIX_COLORS_VAL_MAX) || (colorMax < 2)) {
		return false;
	}

	__HAL_RCC_TIM1_CLK_ENABLE();
	__HAL_RCC_TIM8_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	MatrixStop();

	g_colorMax = colorMax;

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(Line1_GPIO_Port, &GPIO_InitStruct); //PORTC

	GPIO_InitStruct.Pin = R10_Pin | R11_Pin | R12_Pin | R13_Pin | R14_Pin |
	                      G10_Pin | G11_Pin | G12_Pin | G13_Pin | G14_Pin |
	                      B10_Pin | B11_Pin | B12_Pin | B13_Pin | B14_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(R10_GPIO_Port, &GPIO_InitStruct); //PORTB

	memset(g_matrixBuffer, 0, sizeof(g_matrixBuffer));
	memset(g_lineDriver, 0, sizeof(g_lineDriver));

	MatrixLinesCreate();

	MatrixStart();

	return true;
}

static void MatrixDecodeColor(uint8_t bytesPerPixel, const uint8_t * pFrame, uint16_t * pR, uint16_t * pG, uint16_t * pB) {
	if (bytesPerPixel == 1) {
		uint8_t value = *pFrame; //2bits blue + 2 bits green + 2 bits red
		*pB = value >> 4;
		*pG = (value >> 2) & 0x3;
		*pR = value & 0x3;
	} else if (bytesPerPixel == 2) {
		uint8_t value = *pFrame; //5 bits blue + 2 bits green
		*pB = value >> 2;
		*pG = ((value & 3) << 3);
		value = *(pFrame + 1); //3 bits green + 5 bits red
		*pG |= value >> 5;
		*pR = value & 0x1F;
	} else if (bytesPerPixel == 3) {
		*pB = *pFrame;
		*pG = *(pFrame + 1);
		*pR = *(pFrame + 2);
	} else if (bytesPerPixel == 6) {
		*pB = (*(pFrame + 0) << 8) | *(pFrame + 1);
		*pG = (*(pFrame + 2) << 8) | *(pFrame + 3);
		*pR = (*(pFrame + 4) << 8) | *(pFrame + 5);
	}
}

/*Pin connection
  PB0: green 0
  PB1: input (BOOT1 pin), writes are ignored
  PB2: blue 0
  PB3: red 0
  PB4: green 1
  PB5: blue 1
  PB6: red 1
  PB7: green 2
  PB8: blue 2
  PB9: red 2
  PB10: green 3
  PB11: blue 3
  PB12: red 3
  PB13: green 4
  PB14: blue 4
  PB15: red 4
*/
void MatrixFrame(uint8_t bytesPerPixel, const uint8_t * pFrame) {
	uint16_t r = 0, g = 0, b = 0;
	for (uint16_t y = 0; y < MATRIX_Y; y++) {
		uint32_t idxOffset = y * g_colorMax;
		for (uint16_t x = 0; x < MATRIX_X; x++) {
			MatrixDecodeColor(bytesPerPixel, pFrame, &r, &g, &b);
			for (uint16_t unaryColor = 0; unaryColor < g_colorMax; unaryColor++) {
				uint8_t extraOffset = 0; //addressing unused PB1
				if (x > 0) {
					extraOffset = 1;
				}
				uint16_t value = g_matrixBuffer[idxOffset + unaryColor];
				uint16_t bitmask = 1 << (x * 3 + extraOffset);
				if (g > unaryColor) {
					value |= bitmask;
				} else {
					value &= ~bitmask;
				}
				extraOffset = 1; //PB0 is now set, next is PB2
				bitmask = 1 << (x * 3 + 1 + extraOffset);
				if (b > unaryColor) {
					value |= bitmask;
				} else {
					value &= ~bitmask;
				}
				bitmask = 1 << (x * 3 + 2 + extraOffset);
				if (r > unaryColor) {
					value |= bitmask;
				} else {
					value &= ~bitmask;
				}
				g_matrixBuffer[idxOffset + unaryColor] = value;
			}
			pFrame += bytesPerPixel;
		}
	}
}

void MatrixBrightness(uint8_t value) {
	g_brightFactor = (value * MATRIX_DIM_FACTOR_MAX) / 255;
	MatrixLinesCreate();
}

void MatrixDisable(void) {
	MatrixStop();
	__HAL_RCC_TIM1_CLK_DISABLE();
	__HAL_RCC_TIM8_CLK_DISABLE();

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(Line1_GPIO_Port, &GPIO_InitStruct); //PORTC

	GPIO_InitStruct.Pin = R10_Pin | R11_Pin | R12_Pin | R13_Pin | R14_Pin |
	                      G10_Pin | G11_Pin | G12_Pin | G13_Pin | G14_Pin |
	                      B10_Pin | B11_Pin | B12_Pin | B13_Pin | B14_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(R10_GPIO_Port, &GPIO_InitStruct); //PORTB
}

