/* Ledspiellib based on Boxlib
(c) 2025 by Malte Marwedel

License: BSD-3-Clause
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ledspiellib/mcu.h"

#include "ledspiellib/leds.h"
#include "main.h"

uint32_t g_mcuFrequceny = 16000000;
uint32_t g_mcuApbDivider = 1;

//See https://stm32f4-discovery.net/2017/04/tutorial-jump-system-memory-software-stm32/
void McuStartOtherProgram(void * startAddress, bool ledSignalling) {
	volatile uint32_t * pStackTop = (uint32_t *)(startAddress);
	volatile uint32_t * pProgramStart = (uint32_t *)(startAddress + 0x4);
	if (ledSignalling) {
		Led2Green();
	}
	__HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
	__HAL_FLASH_DATA_CACHE_DISABLE();
	__HAL_FLASH_PREFETCH_BUFFER_DISABLE();
	uint8_t result = McuClockToHsi(RCC_HCLK_DIV1);
	if (result != 0) {
		printf("Error, failed to switch to HSI - %u\r\n", result);
		HAL_Delay(50); //to print the error message
	}
	HAL_RCC_DeInit();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	__disable_irq();
	__DSB();
	__HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
	__DSB();
	__ISB();
	__HAL_RCC_USB_OTG_FS_FORCE_RESET();
	__HAL_RCC_TIM7_FORCE_RESET();
	__HAL_RCC_SPI1_FORCE_RESET();
	__HAL_RCC_SPI2_FORCE_RESET();
	__HAL_RCC_SPI3_FORCE_RESET();
	__HAL_RCC_USART1_FORCE_RESET();
	__HAL_RCC_USART2_FORCE_RESET();
	__HAL_RCC_USART3_FORCE_RESET();
	__HAL_RCC_UART4_FORCE_RESET();
	__HAL_RCC_UART5_FORCE_RESET();
	__HAL_RCC_USART6_FORCE_RESET();
	__HAL_RCC_USB_OTG_FS_RELEASE_RESET();
	__HAL_RCC_TIM7_RELEASE_RESET();
	__HAL_RCC_SPI1_RELEASE_RESET();
	__HAL_RCC_SPI2_RELEASE_RESET();
	__HAL_RCC_SPI3_RELEASE_RESET();
	__HAL_RCC_USART1_RELEASE_RESET();
	__HAL_RCC_USART2_RELEASE_RESET();
	__HAL_RCC_USART3_RELEASE_RESET();
	__HAL_RCC_UART4_RELEASE_RESET();
	__HAL_RCC_UART5_RELEASE_RESET();
	__HAL_RCC_USART6_RELEASE_RESET();
	//Is there a generic maximum interrupt number defined somewhere?
	for (uint32_t i = 0; i <= FPU_IRQn; i++) {
		NVIC_DisableIRQ(i);
		NVIC_ClearPendingIRQ(i);
	}
	//Restore more original settings, otherwise the internal hardware bootloader won't start up
	RCC->CR = 0x00000083;
	RCC->PLLCFGR = 0x24003010;
	PWR->CR = 0x0000C000;
	PWR->CSR = 0x00000000;
	__enable_irq(); //actually, the system seems to start with enabled interrupts
	if (ledSignalling) {
		Led1Off();
	}
	/* Writing the stack change as C code is a bad idea, because the compiler
	   can insert stack changeing code before the function call. And in fact, it
	   does with some optimization. So
	       __set_MSP(*pStackTop);
	       ptrFunction_t * pDfu = (ptrFunction_t *)(*pProgramStart);
	       pDfu();
	   would work with -Og optimization, but not with -Os optimization.
	   Instead we use two commands of assembly, where the compiler can't add code
	   inbetween.
*/
	asm("msr msp, %[newStack]\n bx %[newProg]"
	     : : [newStack]"r"(*pStackTop), [newProg]"r"(*pProgramStart));
}

static uint8_t McuClockToPll(uint32_t frequency, uint32_t apbDivider, uint32_t pllSource) {
	//We always need to maintain 48MHz for USB (PLLQ), but this might not work with the HSI.
	uint32_t latency; //div 2
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	if (frequency <= 144000000) {
		__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
	} else {
		__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	}
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = pllSource;
	if (pllSource == RCC_PLLSOURCE_HSE) {
		RCC_OscInitStruct.PLL.PLLM = 4; //external crystal has 8MHz
		RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
		RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	} else {
		RCC_OscInitStruct.PLL.PLLM = 8; //HSI provides 16MHz
	}

	if (frequency == 16000000) {
		latency = FLASH_LATENCY_0;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
		RCC_OscInitStruct.PLL.PLLN = 96;
		RCC_OscInitStruct.PLL.PLLP = 6;
		RCC_OscInitStruct.PLL.PLLQ = 4;
	} else if (frequency == 32000000) {
		latency = FLASH_LATENCY_1;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_OscInitStruct.PLL.PLLN = 96;
		RCC_OscInitStruct.PLL.PLLP = 6;
		RCC_OscInitStruct.PLL.PLLQ = 4;
	} else if (frequency == 48000000) {
		latency = FLASH_LATENCY_1;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_OscInitStruct.PLL.PLLN = 96;
		RCC_OscInitStruct.PLL.PLLP = 4;
		RCC_OscInitStruct.PLL.PLLQ = 4;
	} else if (frequency == 64000000) {
		latency = FLASH_LATENCY_2;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_OscInitStruct.PLL.PLLN = 192;
		RCC_OscInitStruct.PLL.PLLP = 6;
		RCC_OscInitStruct.PLL.PLLQ = 8;
	} else if (frequency == 96000000) {
		latency = FLASH_LATENCY_3;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_OscInitStruct.PLL.PLLN = 96;
		RCC_OscInitStruct.PLL.PLLP = 2;
		RCC_OscInitStruct.PLL.PLLQ = 4;
	} else if (frequency == 144000000) {
		latency = FLASH_LATENCY_4;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_OscInitStruct.PLL.PLLN = 144;
		RCC_OscInitStruct.PLL.PLLP = 2;
		RCC_OscInitStruct.PLL.PLLQ = 6;
	} else if (frequency == 168000000) {
		latency = FLASH_LATENCY_5;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_OscInitStruct.PLL.PLLN = 168;
		RCC_OscInitStruct.PLL.PLLP = 2;
		RCC_OscInitStruct.PLL.PLLQ = 7;

	} else {
		return 1;
	}
	//first set slowest latency, suitable for all frequencies
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
		return 2;
	}
	HAL_StatusTypeDef result = HAL_RCC_OscConfig(&RCC_OscInitStruct);
	if (result != HAL_OK) {
		return 3;
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
	                              RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	//APB2 limit is 84MHz
	if ((frequency > 84000000) && (apbDivider == RCC_HCLK_DIV1)) {
		apbDivider = RCC_HCLK_DIV2;
	}
	RCC_ClkInitStruct.APB2CLKDivider = apbDivider;
	//APB1 limit is 42MHz
	if ((frequency > 84000000) && (apbDivider == RCC_HCLK_DIV2)) {
		apbDivider = RCC_HCLK_DIV4;
	} else if ((frequency > 42000000) && (apbDivider == RCC_HCLK_DIV1)) {
		apbDivider = RCC_HCLK_DIV2;
	}
	RCC_ClkInitStruct.APB1CLKDivider = apbDivider;
	//now set new dividers
	result = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, latency);
	if (result != HAL_OK) {
		return 4;
	}
	SystemCoreClockUpdate();
	g_mcuFrequceny = frequency;
	g_mcuApbDivider = apbDivider;
	return 0;
}

uint8_t McuClockToHsiPll(uint32_t frequency, uint32_t apbDivider) {
	return McuClockToPll(frequency, apbDivider, RCC_PLLSOURCE_HSI);
}

uint8_t McuClockToHsi(uint32_t apbDivider) {
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	//enable HSI
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	HAL_StatusTypeDef result = HAL_RCC_OscConfig(&RCC_OscInitStruct);
	if (result != HAL_OK) {
		return 1;
	}
	//first switch to HSI without PLL
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
		return 2;
	}
	//now set the PLL to off
	RCC_OscInitStruct.OscillatorType = 0;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_OFF;
	result = HAL_RCC_OscConfig(&RCC_OscInitStruct);
	if (result != HAL_OK) {
		return 3;
	}
	//now set the HSE to off (can not be done while running on the HSE)
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	result = HAL_RCC_OscConfig(&RCC_OscInitStruct);
	if (result != HAL_OK) {
		return 4;
	}
	//now set new dividers
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.APB2CLKDivider = apbDivider;
	RCC_ClkInitStruct.APB1CLKDivider = apbDivider;
	result = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
	if (result != HAL_OK) {
		return 5;
	}
	SystemCoreClockUpdate();
	g_mcuFrequceny = 16000000;
	g_mcuApbDivider = apbDivider;
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
	return 0;
}

uint8_t McuClockToHsePll(uint32_t frequency, uint32_t apbDivider) {
	return McuClockToPll(frequency, apbDivider, RCC_PLLSOURCE_HSE);
}

void McuLockCriticalPins(void) {
}

uint64_t McuTimestampUs(void) {
	uint32_t stamp1, stamp2;
	uint32_t substamp;
	//get the data
	do {
		stamp1 = HAL_GetTick();
		substamp = SysTick->VAL;
		stamp2 = HAL_GetTick();
	} while (stamp1 != stamp2); //don't read VAL while an overflow happened
	uint32_t load = SysTick->LOAD;
	//NVIC_GetPendingIRQ(SysTick_IRQn) does not work!
	if (SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) {
		/* Looks like the systick interrupts is locked, and 2x HAL_GetTick
		   just got the same timestamp because the increment could not be done.
		   So if the substamp is > load/2, it has underflown before the readout and
		   the stamp value needs to be increased.
		   Note: The pending alredy gets set when the value switches from 1 -> 0,
		   but we only want to add 1ms when a 0 -> load underflow happened.
		   The 1 -> 0 never is a problem with ISRs enabled, because this case is
		   catched by the stamp1 != stamp2 comparison.
		   Important: This logic assumes SCB_ICSR_PENDSTSET_Msk could never be set
		   as pending, when the ISR is not blocked (and this function is not called
		   from within the systick interrupt itself).
		*/
		if (substamp > (load / 2)) {
			stamp1++;
		}
	}
	//now calculate
	substamp = load - substamp; //this counts down, so invert it
	substamp = substamp * 1000 / load;
	uint64_t stamp = stamp1;
	stamp *= 1000;
	stamp += substamp;
	//printf("Stamp: %x-%x\r\n", (uint32_t)(stamp >> 32LLU), (uint32_t)stamp);
	return stamp;
}

void McuDelayUs(uint32_t us) {
	uint64_t tEnd = McuTimestampUs() + us;
	while (tEnd > McuTimestampUs());
}

