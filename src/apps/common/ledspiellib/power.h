#pragma once

/* Ledspiellib
(c) 2026 by Malte Marwedel

SPDX-License-Identifier:  BSD-3-Clause
*/

#include "main.h"

static inline void PowerOff(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = PowerOff_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(PowerOff_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(PowerOff_GPIO_Port, PowerOff_Pin, GPIO_PIN_SET);
	while(1); //It may take some µs or even ms until the power is gone out of the capacitors
}
