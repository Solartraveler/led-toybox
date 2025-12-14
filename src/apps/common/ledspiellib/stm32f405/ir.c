/* Ledspiellib based on Boxlib
(c) 2025 by Malte Marwedel

SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdbool.h>

#include "ledspiellib/ir.h"

#include "main.h"

void IrInit(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = RemoteIn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(RemoteIn_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = RemoteOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(RemoteOn_GPIO_Port, &GPIO_InitStruct);
}

void IrOn(void) {
	HAL_GPIO_WritePin(RemoteOn_GPIO_Port, RemoteOn_Pin, GPIO_PIN_SET);
}

void IrOff(void) {
	HAL_GPIO_WritePin(RemoteOn_GPIO_Port, RemoteOn_Pin, GPIO_PIN_RESET);
}

bool IrPinSignal(void) {
	if (HAL_GPIO_ReadPin(RemoteIn_GPIO_Port, RemoteIn_Pin) == GPIO_PIN_RESET) {
		return true;
	}
	return false;
}

