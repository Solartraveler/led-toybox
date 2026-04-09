/* Ledspiellib
(c) 2026 by Malte Marwedel

SPDX-License-Identifier:  BSD-3-Clause
*/

#include <stdint.h>

#include "ledspiellib/keyInput.h"

#include "main.h"

void KeyInputInit(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

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

uint32_t KeyInputGet(void) {
	uint32_t result = 0;
	if ((Key1_GPIO_Port->IDR) & Key1_Pin) result |= 1;
	result |= ((GPIOC->IDR >> 6) & 0x1F) << 1;
	if ((Boot1_GPIO_Port->IDR) & Boot1_Pin) result |= 40;
	return result;
}
