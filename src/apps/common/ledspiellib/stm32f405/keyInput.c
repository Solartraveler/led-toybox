/* Ledspiellib
(c) 2026 by Malte Marwedel

SPDX-License-Identifier:  BSD-3-Clause
*/

#include <stdint.h>

#include "ledspiellib/keyInput.h"

#include "main.h"

void KeyInputInit(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	GPIO_InitStruct.Pin = Key2_Pin | Key3_Pin | Key4_Pin | Key5_Pin | Key6_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = Key1_Pin; //connected to the BOOT0 pin
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(Key1_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = Boot1_Pin; //could have been named Key7_Pin
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(Boot1_GPIO_Port, &GPIO_InitStruct);
}

void KeyInputDeinit(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = Key2_Pin | Key3_Pin | Key4_Pin | Key5_Pin | Key6_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = Key1_Pin; //connected to the BOOT0 pin
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(Key1_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = Boot1_Pin; //could have been named Key7_Pin
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(Boot1_GPIO_Port, &GPIO_InitStruct);
}
#include <stdio.h>
uint32_t KeyInputGet(void) {
	uint32_t result = 0;
	if ((Boot1_GPIO_Port->IDR) & Boot1_Pin) result |= 0x1;
	uint16_t portC = GPIOC->IDR;
	if (portC & Key6_Pin) result |= 0x2;
	if (portC & Key5_Pin) result |= 0x4;
	if (portC & Key4_Pin) result |= 0x8;
	if (portC & Key3_Pin) result |= 0x10;
	if (portC & Key2_Pin) result |= 0x20;
	if ((Key1_GPIO_Port->IDR) & Key1_Pin) result |= 0x40;
	return result;
}
