/* Ledspiellib based on Boxlib
(c) 2026 by Malte Marwedel

SPDX-License-Identifier: BSD-3-Clause
*/


#include <stdint.h>
#include <stdio.h>

#include "ledspiellib/stackSampler.h"

#include "main.h"

static uintptr_t g_stackMin;
static uintptr_t g_stackMax;


void StackSampleInit(void) {
	uint8_t * dummy;
	uintptr_t addr = (uintptr_t)&dummy;
	g_stackMax = g_stackMin = addr;
	__HAL_RCC_TIM10_CLK_ENABLE();
	TIM10->CR1 = 0; //all stopped
	TIM10->CR2 = 0;
	TIM10->CNT = 0;
	TIM10->PSC = 0;
	TIM10->SR = 0;
	TIM10->DIER = TIM_DIER_UIE;
	TIM10->ARR = 1024; //higher resolution with a value of 255 possible, but this would take a lot of CPU time
	HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
	TIM10->CR1 |= TIM_CR1_CEN;
}

void TIM1_UP_TIM10_IRQHandler(void) {
	uint8_t dummy;
	uintptr_t addr = (uintptr_t)&dummy;
	if (addr < g_stackMin) {
		g_stackMin = addr;
	}
	if (addr > g_stackMax) {
		g_stackMax = addr;
	}
	TIM10->SR = 0;
	NVIC_ClearPendingIRQ(TIM1_UP_TIM10_IRQn);
}

void StackSampleCheck(void) {
	uintptr_t delta = g_stackMax - g_stackMin;
	static uintptr_t deltaLast = 0;
	if (delta > deltaLast) {
		printf("Stack max 0x%x min 0x%x delta %u\r\n", (unsigned int)g_stackMax, (unsigned int)g_stackMin, (unsigned int)delta);
		deltaLast = delta;
	}
}
