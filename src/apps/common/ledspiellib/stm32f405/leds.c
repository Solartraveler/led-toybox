/* Ledspiellib based on Boxlib
(c) 2025 by Malte Marwedel

SPDX-License-Identifier: BSD-3-Clause
*/

#include "ledspiellib/leds.h"

#include "main.h"

void LedsInit(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	HAL_GPIO_WritePin(GPIOB, R10_Pin | G10_Pin | B10_Pin | R11_Pin | G11_Pin | B11_Pin | R12_Pin | G12_Pin | B12_Pin |
	                         R13_Pin | G13_Pin | B13_Pin | R14_Pin | G14_Pin | B14_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOC, Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin, GPIO_PIN_RESET);

	GPIO_InitStruct.Pin = R10_Pin | G10_Pin | B10_Pin | R11_Pin | G11_Pin | B11_Pin | R12_Pin | G12_Pin | B12_Pin |
	                      R13_Pin | G13_Pin | B13_Pin | R14_Pin | G14_Pin | B14_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = Line1_Pin | Line2_Pin | Line3_Pin | Line4_Pin | Line5_Pin;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Led1Red(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R11_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, G11_Pin, GPIO_PIN_RESET);
}

void Led1Green(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R11_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, G11_Pin, GPIO_PIN_SET);
}

void Led1Yellow(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R11_Pin | G11_Pin, GPIO_PIN_SET);
}

void Led1Off(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R11_Pin | G11_Pin, GPIO_PIN_RESET);
}

void Led2Red(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R13_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, G13_Pin, GPIO_PIN_RESET);
}

void Led2Green(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R13_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, G13_Pin, GPIO_PIN_SET);
}

void Led2Yellow(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R13_Pin | G13_Pin, GPIO_PIN_SET);
}

void Led2Off(void) {
	HAL_GPIO_WritePin(GPIOC, Line3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, R13_Pin | G13_Pin, GPIO_PIN_RESET);
}
