/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "sdmmcAccess.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LED_R 1
#define LED_G 4
#define LED_B 2

#define F_CPU 64000000ULL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */

bool g_runPinTest = true;
uint32_t g_audioTest = 0;
uint32_t g_blinkDelay = 1000;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void PrintUart(const char * text) {
	HAL_UART_Transmit(&huart1, (const uint8_t *)text, strlen(text), 100);
}

void PrintfUart(const char * format, ...) {
	char buffer[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	PrintUart(buffer);
}

char UartInput(void) {
	uint8_t data = 0;
	HAL_UART_Receive(&huart1, &data, 1, 1);
	return data;
}

void TestPortPinsHigh(bool * highOk, GPIO_TypeDef * gpioPort, uint32_t testMask) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	for (uint32_t i = 0; i < 16; i++) {
		GPIO_InitStruct.Pin = 1 << i;
		if (!(GPIO_InitStruct.Pin & testMask)) {
			continue;
		}
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(gpioPort, &GPIO_InitStruct);
		HAL_Delay(1);
		GPIO_PinState pinState = HAL_GPIO_ReadPin(gpioPort, 1 << i);
		if (pinState == GPIO_PIN_SET) {
			highOk[i] = true;
		}
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;
		HAL_GPIO_Init(gpioPort, &GPIO_InitStruct);
	}
}

void TestPortPinsLow(bool * lowOk, GPIO_TypeDef * gpioPort, uint32_t testMask) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	for (uint32_t i = 0; i < 16; i++) {
		GPIO_InitStruct.Pin = 1 << i;
		if (!(GPIO_InitStruct.Pin & testMask)) {
			continue;
		}
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(gpioPort, &GPIO_InitStruct);
		HAL_Delay(1);
		GPIO_PinState pinState = HAL_GPIO_ReadPin(gpioPort, 1 << i);
		if (pinState == GPIO_PIN_RESET) {
			lowOk[i] = true;
		}
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		HAL_GPIO_Init(gpioPort, &GPIO_InitStruct);
	}
}


void TestAllPinInterconnects(void) {
	const uint32_t testMasks[4] = {0xF9FF, 0xFFFF, 0xFFFF, 0x4};
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	//High test
	GPIO_InitStruct.Pin = testMasks[0];
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = testMasks[1];
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = testMasks[2];
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = testMasks[3];
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	/*We do not test pins GPIOH, they are connected to the crystal, and if they would
	  not work, we could not run the program anyway.
	*/
	bool highOk[64] = {false};
	TestPortPinsHigh(highOk + 0,  GPIOA, testMasks[0]);
	TestPortPinsHigh(highOk + 16, GPIOB, testMasks[1]);
	TestPortPinsHigh(highOk + 32, GPIOC, testMasks[2]);
	TestPortPinsHigh(highOk + 48, GPIOD, testMasks[3]);

	//Low test
	GPIO_InitStruct.Pin = testMasks[0];
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = testMasks[1];
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = testMasks[2];
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = testMasks[3];
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	bool lowOk[64] = {false};
	TestPortPinsLow(lowOk + 0,  GPIOA, testMasks[0]);
	TestPortPinsLow(lowOk + 16, GPIOB, testMasks[1]);
	TestPortPinsLow(lowOk + 32, GPIOC, testMasks[2]);
	TestPortPinsLow(lowOk + 48, GPIOD, testMasks[3]);

	//Restore pins for printing the results
	MX_GPIO_Init();
	MX_USART1_UART_Init();
	for (uint32_t i = 0; i < sizeof(highOk); i++) {
		uint8_t port = i / 16;
		uint8_t pin = i % 16;

		char portChar = 'A' + port;
		if (testMasks[port] & (1 << pin)) {
			const char * result = "?";
			if (highOk[i] && lowOk[i]) {
				result = "ok";
			} else if (lowOk[i] == false) {
				result = "no low";
			} else if (highOk[i] == false) {
				result = "no high";
			} else {
				result = "error - no control";
			}
			PrintfUart("Port%c%u -> %s\r\n", portChar, pin, result);
		}
	}
}

void LedLine(uint32_t pattern) {
	//PB1 is not used as output
	pattern = ((pattern & 0xFFFE) << 1) | (pattern & 1);
	pattern |= (~(pattern << 16)) & 0xFFFD0000;
	//high bits clear, low bits set
	GPIOB->BSRR = pattern;
}

void LedLineEqual(uint8_t color) {
	LedLine((color << 0) | (color << 3) | (color << 6) | (color << 9) | (color << 12));
}

void LedLineSelect(uint8_t line) {
	uint32_t bit = 1 << line;
	uint32_t pattern = bit | (0x1F0000 & ~(bit << 16));
	GPIOC->BSRR = pattern;
}

void McuStartOtherProgram(void * startAddress) {
	volatile uint32_t * pStackTop = (uint32_t *)(startAddress);
	volatile uint32_t * pProgramStart = (uint32_t *)(startAddress + 0x4);
	__HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
	__HAL_FLASH_DATA_CACHE_DISABLE();
	__HAL_FLASH_PREFETCH_BUFFER_DISABLE();
	HAL_RCC_DeInit();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	__disable_irq();
	__DSB();
	__HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
	__DSB();
	__ISB();
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
	__enable_irq(); //actually, the system seems to start with enabled interrupts
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


typedef void (ptrFunction_t)(void);

//See https://stm32f4-discovery.net/2017/04/tutorial-jump-system-memory-software-stm32/
void JumpDfu(void) {
	uint32_t dfuStart = 0x1FFF0000;
	PrintUart("Directly jump to the DFU bootloader\r\n");
	//first all peripheral clocks should be disabled
	McuStartOtherProgram((void *)dfuStart); //usually does not return
}

void AudioOutputAnalog(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = Dac1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(Dac1_GPIO_Port, &GPIO_InitStruct);
}

void AudioOutputEnable(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = AudioOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(AudioOn_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(AudioOn_GPIO_Port, AudioOn_Pin, GPIO_PIN_RESET);
}

void AudioOutputDisable(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = AudioOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(AudioOn_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(AudioOn_GPIO_Port, AudioOn_Pin, GPIO_PIN_SET);
}

void DacTest(void) {
	AudioOutputAnalog();
	__HAL_RCC_DAC_CLK_ENABLE();
	DAC1->DHR12R1 = 0;
	DAC1->CR = DAC_CR_EN1;
	for (uint32_t i = 0; i < 16; i++) {
		DAC1->DHR12R1 = 0;
		PrintUart("Dac zero\r\n");
		LedLineEqual(LED_R);
		HAL_Delay(2000);

		DAC1->DHR12R1 = 2047;
		PrintUart("Dac 2047\r\n");
		LedLineEqual(LED_G);
		HAL_Delay(2000);

		DAC1->DHR12R1 = 4095;
		PrintUart("Dac 4095\r\n");
		LedLineEqual(LED_B);
		HAL_Delay(2000);
		if (UartInput() != '\0') {
			break;
		}
	}
	DAC1->DHR12R1 = 0;
	PrintUart("Dac test complete\r\n");
}

void DacEqual(void) {
	AudioOutputAnalog();
	AudioOutputEnable();
	__HAL_RCC_DAC_CLK_ENABLE();
	DAC1->DHR12R1 = 0;
	DAC1->CR = DAC_CR_EN1;
	DAC1->DHR12R1 = 2047;
	g_runPinTest = false;
	PrintUart("Dac equal. Pin test disabled\r\n");
}

#define DAC_MAX 4095
#define SINE_VALUES 1000

void AudioStop(void) {
	TIM7->CR1 = 0;
	DMA1_Stream2->CR = 0;
	AudioOutputDisable();
}

void AudioPlay(uint16_t frequency) {
	static uint16_t data[SINE_VALUES];
	float pi2 = (float)M_PI * (float)2.0;
	for (uint32_t i = 0; i < SINE_VALUES; i++) {
		data[i] = ((float)DAC_MAX/2) + ((float)DAC_MAX/2) * sinf((float)i * pi2 / (float)SINE_VALUES);
	}
	PrintUart("Starting sine test\r\n");
	//stop any old transfer
	AudioStop();
	//Enable audio amplifier and outputs
	AudioOutputAnalog();
	AudioOutputEnable();
	//Enable DAC
	DAC1->DHR12R1 = 0;
	DAC1->CR = DAC_CR_EN1;
	//Enable DMA (Stream 2, Channel 1, triggered by timer 7)
	__HAL_RCC_DMA1_CLK_ENABLE();
	DMA1_Stream2->CR &= ~DMA_SxCR_EN;
	while(DMA1_Stream2->CR & DMA_SxCR_EN); //if a transfer is ongoing, this waits until it ends
	//Memory increment, Circular mode, Read from memory, Memory size = 16 bits, Peripheral size = 16 bits, Priority medium-high, channel 1
	DMA1_Stream2->CR = DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_DIR_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 | DMA_SxCR_PL_1 | DMA_SxCR_CHSEL_0;
	DMA1_Stream2->PAR = (uint32_t)&DAC->DHR12R1;
	DMA1_Stream2->M0AR = (uint32_t)data;
	DMA1_Stream2->NDTR = SINE_VALUES;
	DMA1_Stream2->CR |= DMA_SxCR_EN;
	//Enable timer
	__HAL_RCC_TIM7_CLK_ENABLE();
	TIM7->PSC = 0;
	TIM7->ARR = (F_CPU / (SINE_VALUES * frequency)) - 1;
	TIM7->CNT = 0;
	TIM7->CR2 = 0;
	TIM7->DIER = TIM_DIER_UDE;
	TIM7->EGR = TIM_EGR_UG;
	TIM7->CR1 = TIM_CR1_CEN;
	PrintfUart("%uHz sine test started\r\n", (unsigned int)frequency);
}

void AudioToggle(void) {
	g_audioTest = (g_audioTest + 1) % 15; //max 14KHz
	if (g_audioTest) {
		AudioPlay(g_audioTest * 100);
	} else {
		AudioStop();
	}
}

void AdcInit(void) {
	__HAL_RCC_ADC1_CLK_ENABLE();
 	//temperature sensor + battery sensor enabled, voltage reference enabled div by 4
	ADC123_COMMON->CCR = ADC_CCR_VBATE | ADC_CCR_TSVREFE | ADC_CCR_ADCPRE_0;
	/* Configure all channnels with 84 ADC clock cycles sampling time.
	*/
	ADC1->SMPR1 = ADC_SMPR1_SMP18_2 | ADC_SMPR1_SMP17_2 | ADC_SMPR1_SMP16_2 |
	              ADC_SMPR1_SMP15_2 | ADC_SMPR1_SMP14_2 | ADC_SMPR1_SMP13_2 |
	              ADC_SMPR1_SMP12_2 | ADC_SMPR1_SMP11_2 | ADC_SMPR1_SMP10_2;
	ADC1->SMPR2 = ADC_SMPR2_SMP9_2 | ADC_SMPR2_SMP8_2 | ADC_SMPR2_SMP7_2 |
	              ADC_SMPR2_SMP6_2 | ADC_SMPR2_SMP5_2 | ADC_SMPR2_SMP4_2 |
	              ADC_SMPR2_SMP3_2 | ADC_SMPR2_SMP2_2 | ADC_SMPR2_SMP1_2 | ADC_SMPR2_SMP0_2;
}

uint16_t AdcGet(uint32_t channel) {
	if (channel >= 32) {
		return 0xFFFF; //impossible value
	}
	ADC1->CR2 &= ~ADC_CR2_ADON;
	ADC1->SR &= ~(ADC_SR_EOC | ADC_SR_STRT | ADC_SR_OVR); //clear end of conversion bit
	ADC1->SQR3 = channel << ADC_SQR3_SQ1_Pos;
	ADC1->CR2 |= ADC_CR2_ADON;
	ADC1->CR2 |= ADC_CR2_SWSTART;
	while ((ADC1->SR & ADC_SR_EOC) == 0);
	uint16_t val = ADC1->DR;
	return val;
}

void AdcStop(void) {
	ADC1->CR2 &= ~ADC_CR2_ADON;
	__HAL_RCC_ADC1_CLK_DISABLE();
}

void LightSelect(uint8_t mode) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if (mode == 1) { //56kOhm
		GPIO_InitStruct.Pin = LightOut1_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(LightOut1_GPIO_Port, &GPIO_InitStruct);
		HAL_GPIO_WritePin(LightOut1_GPIO_Port, LightOut1_Pin, GPIO_PIN_SET);

		GPIO_InitStruct.Pin = LightOut2_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(LightOut2_GPIO_Port, &GPIO_InitStruct);
	} else if (mode == 2) { //1kOhm
		GPIO_InitStruct.Pin = LightOut1_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(LightOut1_GPIO_Port, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = LightOut2_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(LightOut2_GPIO_Port, &GPIO_InitStruct);
		HAL_GPIO_WritePin(LightOut2_GPIO_Port, LightOut2_Pin, GPIO_PIN_SET);
	} else if (mode == 3) { //1MOhm
		GPIO_InitStruct.Pin = LightOut1_Pin | LightOut2_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(LightOut1_GPIO_Port, &GPIO_InitStruct);
	}
}

void LightInit(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = LightIn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LightIn_GPIO_Port, &GPIO_InitStruct);
}

void AnalogTest(void) {
	LightInit();
	AdcInit();
	uint16_t adcBat = AdcGet(18);
	uint16_t vBatMv = 6.6 * 1000.0 * (float)adcBat / (float)4095.0; //vBat has a 1:2 voltage divider. Reference voltage is 3.3V.
	PrintfUart("Vbat: %u -> %umV\r\n", adcBat, vBatMv);

	LightSelect(1);
	HAL_Delay(1);
	uint16_t vLight1 = AdcGet(0);
	PrintfUart("Light1 56kOhm: %u\r\n", vLight1);

	LightSelect(2);
	HAL_Delay(1);
	uint16_t vLight2 = AdcGet(0);
	PrintfUart("Light2  1kOhm: %u\r\n", vLight2);


	LightSelect(3);
	HAL_Delay(1);
	uint16_t vLight3 = AdcGet(0);
	PrintfUart("Light3  1MOhm: %u\r\n", vLight3);

	AdcStop();
}

void LightRead(void) {
	PrintUart("Keys 1-3 to select pull-up.\r\nx: Light LED line.\r\nOther keys exit\r\n");
	bool led = false;
	LedLineEqual(0); //LED light disturb the reading
	LightInit();
	LightSelect(3);
	AdcInit();
	while(1) {
		char input = UartInput();
		if (input == '1') {
			LightSelect(1);
			PrintUart("56kOhm\r\n");
		} else if (input == '2') {
			LightSelect(2);
			PrintUart("1kOhm\r\n");
		} else if (input == '3') {
			LightSelect(3);
			PrintUart("1MOhm\r\n");
		} else if (input == 'x') {
			led = !led;
			if (led) {
				LedLineEqual(LED_R | LED_G | LED_B);
			} else {
				LedLineEqual(0);
			}
		}
		else if (input != '\0') {
			break;
		}
		uint32_t vLight = 0;
		for (uint32_t i = 0; i < 32; i++) {
			vLight += AdcGet(0);
			HAL_Delay(1);
		}
		vLight /= 32;
		PrintfUart("Light: %u\r\n", (uint32_t)vLight);
		HAL_Delay(200);
	}
	AdcStop();
	LightSelect(3);
}

void InfraredTest(void) {
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

	HAL_GPIO_WritePin(RemoteOn_GPIO_Port, RemoteOn_Pin, GPIO_PIN_SET);

	PrintUart("Press any key to exit IR test\r\n");
	GPIO_PinState stateOld = HAL_GPIO_ReadPin(RemoteIn_GPIO_Port, RemoteIn_Pin);
	while (UartInput() == '\0') {
		GPIO_PinState pinState = HAL_GPIO_ReadPin(RemoteIn_GPIO_Port, RemoteIn_Pin);
		if (pinState != stateOld) {
			if (pinState == GPIO_PIN_SET) {
				PrintUart("IR 1\r\n");
			} else {
				PrintUart("IR 0\r\n");
			}
			stateOld = pinState;
		}
	}
	PrintUart("Test exit\r\n");
	HAL_GPIO_WritePin(RemoteOn_GPIO_Port, RemoteOn_Pin, GPIO_PIN_RESET);
}

#define SPIQUEUEDEPTH 1

/*Transfers data over SPI by polling. SPI needs to be initialized, the output
  pins configured and chip select properly set.
  pSpi: Give the desired SPI: SPI1, SPI2, SPI3 etc. Defined in the stm32******.h
  dataOut: Data to send. If not NULL, must have the length len.
  dataIn: Buffer to store the incoming data to. If not NULL, must have the length len.
  len: Number of bytes to send or receive.
  If dataOut is NULL, 0xFF will be send.
  If dataIn is NULL, the incoming data are just discarded.
  If dataOut and dataIn are NULL, just 0xFF is sent with the lenght len.
  Returns true if sending did not end in a timeout. Timeout is 100 ticks in most cases.
  (100 ticks are usually 100ms).
*/
static bool SpiGenericPolling(SPI_TypeDef * pSpi, const uint8_t * dataOut, uint8_t * dataIn, size_t len) {
	/*We can not simply fill data to the input buffer as long as there
	  is room free. Because if we have still data in the incoming buffer and then
	  then fill the outgoing buffer and then get a longer interrupt (by a interrupt
	  or a task scheduler) our input would be filled up by more bytes than the rx
	  buffer can handle and end with an overflow there.
	  So we can only fill as much data to the outgoing buffer as there is space in the incoming buffer too.
	  For the STM32F411, according to the datasheet there the buffers seems to support only one element.
	  For the STM32L452, the datasheet mentions 32bit of FIFO. This should be 4x 8bit data.
	  As result, there will be an interruption for the STM32F411 between one byte sent and the next one put
	  into the TX buffer, whereas the STM32L452 could work without interruption and reach the maximum possible
	  SPI speed.
	*/
	size_t inQueue = 0;
	const size_t inQueueMax = SPIQUEUEDEPTH; //defined by hardware
	bool success = true;
	bool timeout = false;
	size_t txLen = len;
	size_t rxLen = len;
	//printf("Len: %u\r\n", (unsigned int)len);
	uint32_t timeStart = HAL_GetTick();
	while ((rxLen) || (pSpi->SR & SPI_SR_BSY)) {
		//Because a set overflow bit is cleared by each read, so read an check only once in the loop
		uint32_t sr = pSpi->SR;
		if ((sr & SPI_SR_TXE) && (inQueue < inQueueMax) && (txLen)) {
			uint8_t data = 0xFF; //leave signal high if we are only want to receive
			if (dataOut) {
				data = *dataOut;
				dataOut++;
			}
			//printf("W\r\n");
			//Without the cast, 16bit are written, resulting in a second byte with value 0x0 send
			*(__IO uint8_t *)&(pSpi->DR) = data;
			inQueue++;
			txLen--;
			timeout = false;
		}
		if ((sr & SPI_SR_RXNE) && (inQueue) && (rxLen)) {
			//printf("R");
			//Without the cast, we might get up to two bytes at once
			uint8_t data = *(__IO uint8_t *)&(pSpi->DR);
			if (dataIn) {
				*dataIn = data;
				//printf("%x\r\n", data);
				dataIn++;
			}
			inQueue--;
			rxLen--;
			timeout = false;
		}
		if (sr & SPI_SR_OVR) {
			//printf("Overflow!\r\n");
			success = false;
			break;
		}
		if (timeout) {
			//printf("Timeout!\r\n");
			success = false;
			break;
		}
		if ((HAL_GetTick() - timeStart) >= 100) {
			/*If we have a multitasking system, the timeout might be just the cause
			  of the scheduler running a higher priority task for 100ms. So we need
			  to check if there is really nothing to do right now. So only if the
			  next loop still does not produce anything, we assume the reason of the
			  timeout is a non working SPI.
			*/
			//printf("Timeout?\r\n");
			timeout = true;
			timeStart = HAL_GetTick();
		}
	}
	return success;
}

void SdCardOn(void) {
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
	HAL_GPIO_Init(SdOn_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(SdCs_GPIO_Port, SdCs_Pin, GPIO_PIN_SET);

	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	__HAL_RCC_SPI1_CLK_ENABLE();
	SPI1->CR1 = 0;
	SPI1->CR1 = SPI_CR1_MSTR | SPI_BAUDRATEPRESCALER_64 | SPI_CR1_SSM | SPI_CR1_SSI;
	SPI1->CR2 = 0;
	SPI1->CR1 |= SPI_CR1_SPE;
}

void SdChipSelect(bool selected) {
	GPIO_PinState state = GPIO_PIN_SET;
	if (selected) {
		state = GPIO_PIN_RESET;
	}
	HAL_GPIO_WritePin(SdCs_GPIO_Port, SdCs_Pin, state);
}

void SdTransferPolling(const uint8_t * dataOut, uint8_t * dataIn, size_t len, uint8_t /*chipSelect*/, bool resetChipSelect) {
	SdChipSelect(true);
	SpiGenericPolling(SPI1, dataOut, dataIn, len);
	if (resetChipSelect) {
		SdChipSelect(false);
	}
}

void SdCardOff(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = SdCs_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SdOn_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = SdOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SdOn_GPIO_Port, &GPIO_InitStruct);

	__HAL_RCC_SPI1_CLK_DISABLE();
}

void SdCardTest(void) {
	PrintUart("SD card test\r\n");
	SdCardOn();
	uint32_t result = SdmmcInit(&SdTransferPolling, 0);
	if (result == 0) {
		uint32_t cap = SdmmcCapacity();
		cap /= (2 * 1024); //blocks -> MiB
		PrintfUart("SD card working, %uMiB\r\n", (unsigned int)cap);
	} else if (result == 1) {
		PrintUart("SD card has errors on init\r\n");
	} else if (result == 2) {
		PrintUart("SD card not compatible\r\n");
	} else {
		PrintUart("Internal error\r\n");
	}
	SdCardOff();
}

void SwdTest(void) {
	PrintUart("SWD test. Press any key to exit\r\n");
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	bool ignoreDataToggle = false;

	GPIO_PinState stateOldClk = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_14);
	GPIO_PinState stateOldData = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_13);
	PrintfUart("Clk %u, data %u\r\n", (unsigned int)stateOldClk, (unsigned int)stateOldData);
	while (UartInput() == '\0') {
		GPIO_PinState stateClk = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_14);
		GPIO_PinState stateData = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_13);
		if (stateClk != stateOldClk) {
			if (stateClk == GPIO_PIN_SET) {
				PrintUart("Clk 1\r\n");
			} else {
				PrintUart("Clk 0\r\n");
			}
		}
		if (ignoreDataToggle) {
			continue;
		}
		if (stateData != stateOldData) {
			if (stateData == GPIO_PIN_SET) {
				PrintUart("Data 1\r\n");
			} else {
				PrintUart("Data 0\r\n");
			}
		}
		stateOldData = stateData;
		stateOldClk = stateClk;
	}
	PrintUart("Test exit\r\nSet to alternate function SWD? Y/n");
	char c;
	while ((c = UartInput()) == '\0');
	PrintUart("\r\n");
	if ((c == 'y') || (c == 'Y') || (c == '\r')) {
		GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_13;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Alternate = GPIO_AF0_SWJ;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		PrintUart("\r\nSet. Disabling pin test to keep the setting\r\n");
		PrintUart("1MHz frequency seems to work well, the default 1.5MHz ... 2.5MHz not\r\n");
		g_runPinTest = false;
	}
}

void CheckUserInput(void) {
	char input = UartInput();
	switch (input) {
		case 'a': AudioToggle(); break;
		case 'b': JumpDfu(); break;
		case 'd': DacTest(); break;
		case 'e': DacEqual(); break;
		case 'i': InfraredTest(); break;
		case 'l': LightRead(); break;
		case 'p': g_runPinTest = !g_runPinTest; break;
		case 'r': NVIC_SystemReset(); break;
		case 's': SdCardTest(); break;
		case 'u': AnalogTest(); break;
		case 'w': SwdTest(); break;
		case '1': LedLineSelect(0); break;
		case '2': LedLineSelect(1); break;
		case '3': LedLineSelect(2); break;
		case '4': LedLineSelect(3); break;
		case '5': LedLineSelect(4); break;
		case '-': if (g_blinkDelay < 10000) {
			          g_blinkDelay *= 2;
		          };
		          break;
		case '+': if (g_blinkDelay > 100) {
			          g_blinkDelay /= 2;
		          };
		          break;
	}
}

void AllLedFlash(void) {
	uint32_t colors[3] = {LED_R, LED_G, LED_B};
	for (uint32_t colorIdx = 0; colorIdx < 3; colorIdx++) {
		uint32_t color = colors[colorIdx];
		for (uint32_t line = 0; line < 5; line++) {
			LedLineSelect(line);
			for (uint32_t index = 0; index < 15; index += 3) {
				LedLine(color << index);
				HAL_Delay(50);
			}
		}
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
	/* USER CODE BEGIN WHILE */
	LedLineSelect(4);
	HAL_Delay(10); //to sync the UART (no strange chars on the terminal)
	PrintUart(">>>>>>>>Hello World<<<<<<<<\r\n");
	while (1)
	{
		HAL_GPIO_WritePin(Line5_GPIO_Port, Line5_Pin, GPIO_PIN_SET);

		LedLineEqual(LED_R);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);
		PrintUart("a: Audio output test\r\n");
		PrintUart("b: Jump to bootloader\r\n");
		PrintUart("d: DAC test\r\n");
		PrintUart("e: DAC equal\r\n");
		PrintUart("i: Infrared test\r\n");
		PrintUart("l: Ligh read test\r\n");
		PrintUart("p: Toggle pin test\r\n");
		PrintUart("r: Reboot\r\n");
		PrintUart("s: SD card test\r\n");
		PrintUart("u: Read analog\r\n");
		PrintUart("w: SWD pin test\r\n");
		PrintUart("+-: Alter blink delay\r\n");
		PrintUart("1-5: Switch flashing lines\r\n");

		LedLineEqual(LED_G);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);

		LedLineEqual(LED_B);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);

		LedLineEqual(LED_R | LED_G);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);

		LedLineEqual(LED_R | LED_B);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);

		LedLineEqual(LED_G | LED_B);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);

		LedLineEqual(LED_R | LED_G | LED_B);
		CheckUserInput();
		HAL_Delay(g_blinkDelay);

		AllLedFlash();

		if (g_runPinTest) {
			TestAllPinInterconnects();
		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV6;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 19200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, Charge_Pin|Line1_Pin|Line2_Pin|Line3_Pin
                          |Line4_Pin|Line5_Pin|SdCs_Pin|RemoteOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LightOut1_Pin|LightOut2_Pin|AudioOn_Pin|SdOn_Pin
                          |PowerOff_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, G10_Pin|B10_Pin|G13_Pin|B13_Pin
                          |R13_Pin|G14_Pin|B14_Pin|R14_Pin
                          |R10_Pin|G11_Pin|B11_Pin|R11_Pin
                          |G12_Pin|B12_Pin|R12_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : Charge_Pin Line1_Pin Line2_Pin Line3_Pin
                           Line4_Pin Line5_Pin SdCs_Pin RemoteOn_Pin */
  GPIO_InitStruct.Pin = Charge_Pin|Line1_Pin|Line2_Pin|Line3_Pin
                          |Line4_Pin|Line5_Pin|SdCs_Pin|RemoteOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LightOut1_Pin LightOut2_Pin AudioOn_Pin SdOn_Pin
                           PowerOff_Pin */
  GPIO_InitStruct.Pin = LightOut1_Pin|LightOut2_Pin|AudioOn_Pin|SdOn_Pin
                          |PowerOff_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : G10_Pin B10_Pin G13_Pin B13_Pin
                           R13_Pin G14_Pin B14_Pin R14_Pin
                           R10_Pin G11_Pin B11_Pin R11_Pin
                           G12_Pin B12_Pin R12_Pin */
  GPIO_InitStruct.Pin = G10_Pin|B10_Pin|G13_Pin|B13_Pin
                          |R13_Pin|G14_Pin|B14_Pin|R14_Pin
                          |R10_Pin|G11_Pin|B11_Pin|R11_Pin
                          |G12_Pin|B12_Pin|R12_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : Boot1_Pin */
  GPIO_InitStruct.Pin = Boot1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Boot1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Key2_Pin Key3_Pin Key4_Pin Key5_Pin
                           Key6_Pin RemoteIn_Pin */
  GPIO_InitStruct.Pin = Key2_Pin|Key3_Pin|Key4_Pin|Key5_Pin
                          |Key6_Pin|RemoteIn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : Key1_Pin */
  GPIO_InitStruct.Pin = Key1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Key1_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
