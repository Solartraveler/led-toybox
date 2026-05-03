/* Ledspiellib
(c) 2026 by Malte Marwedel

SPDX-License-Identifier:  BSD-3-Clause
*/

#include <stdbool.h>

#include "ledspiellib/chargerControl.h"

#include "main.h"

void ChargerControl(bool on) {
	static bool initDone = false;
	if (!initDone) {
		__HAL_RCC_GPIOC_CLK_ENABLE();
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = Charge_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(Charge_GPIO_Port, &GPIO_InitStruct);
	}
	if (on) {
		HAL_GPIO_WritePin(Charge_GPIO_Port, Charge_Pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(Charge_GPIO_Port, Charge_Pin, GPIO_PIN_RESET);
	}
}
