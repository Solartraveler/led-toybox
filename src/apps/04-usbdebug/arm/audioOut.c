#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audioOut.h"

#include "main.h"
#include "utility.h"

static uint16_t * g_audioBuffer;
static size_t g_audioBufferEntries;
static size_t g_audioBufferWritePtr;

static void AudioOutputAnalog(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = Dac1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(Dac1_GPIO_Port, &GPIO_InitStruct);
}

static void AudioOutputEnable(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = AudioOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(AudioOn_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(AudioOn_GPIO_Port, AudioOn_Pin, GPIO_PIN_RESET);
}

static void AudioOutputDisable(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = AudioOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(AudioOn_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(AudioOn_GPIO_Port, AudioOn_Pin, GPIO_PIN_SET);
}

void AudioStop(void) {
	TIM7->CR1 = 0;
	DMA1_Stream2->CR = 0;
	AudioOutputDisable();
	__HAL_RCC_DAC_CLK_DISABLE();
}

bool AudioInit(uint16_t * pBuffer, size_t entries, uint16_t samplerate) {
	if ((!pBuffer) || (entries < 2) || (samplerate == 0) || ((F_CPU / samplerate) > 0xFFFF)) {
		return false;
	}
	//stop any old transfer
	AudioStop();
	g_audioBuffer = pBuffer;
	g_audioBufferEntries = entries;
	g_audioBufferWritePtr = 0;
	//Enable audio amplifier and outputs
	AudioOutputAnalog();
	AudioOutputEnable();
	//Enable DAC
	__HAL_RCC_DAC_CLK_ENABLE();
	DAC1->DHR12R1 = 0;
	DAC1->CR = DAC_CR_EN1;
	//Enable DMA (Stream 2, Channel 1, triggered by timer 7)
	__HAL_RCC_DMA1_CLK_ENABLE();
	DMA1_Stream2->CR &= ~DMA_SxCR_EN;
	while(DMA1_Stream2->CR & DMA_SxCR_EN); //if a transfer is ongoing, this waits until it ends
	//Memory increment, Circular mode, Read from memory, Memory size = 16 bits, Peripheral size = 16 bits, Priority medium-high, channel 1
	DMA1_Stream2->CR = DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_DIR_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 | DMA_SxCR_PL_1 | DMA_SxCR_CHSEL_0;
	DMA1_Stream2->PAR = (uint32_t)&DAC->DHR12R1;
	DMA1_Stream2->M0AR = (uint32_t)pBuffer;
	DMA1_Stream2->NDTR = entries;
	DMA1_Stream2->CR |= DMA_SxCR_EN;
	//Enable timer
	__HAL_RCC_TIM7_CLK_ENABLE();
	TIM7->PSC = 0;
#if (F_CPU > 84000000)
	TIM7->ARR = (F_CPU / 2 / samplerate) - 1;
#else
	TIM7->ARR = (F_CPU / samplerate) - 1;
#endif
	TIM7->CNT = 0;
	TIM7->CR2 = 0;
	TIM7->DIER = TIM_DIER_UDE;
	TIM7->EGR = TIM_EGR_UG;
	TIM7->CR1 = TIM_CR1_CEN;
	//printf("Audio started with %uHz\r\n", (unsigned int) (F_CPU / (TIM7->ARR + 1)));
	return true;
}

/*Lets assume a buffer with just 3 elements, so there would be 9 states
  read  write -> free
  0     0        0
  0     1        2
  0     2        1
  1     0        1
  1     1        0
  1     2        2
  2     0        2
  2     1        1
  2     2        0
  -> One element is never used.
*/
size_t AudioBufferFreeGet(void) {
	uint32_t dmaReadPtr = g_audioBufferEntries - DMA1_Stream2->NDTR;
	if (dmaReadPtr >= g_audioBufferEntries) {
		dmaReadPtr = 0;
	}
	if (dmaReadPtr > g_audioBufferWritePtr) {
		return dmaReadPtr - g_audioBufferWritePtr;
	} else if (dmaReadPtr < g_audioBufferWritePtr) {
		return (g_audioBufferEntries - g_audioBufferWritePtr) + dmaReadPtr;
	}
	return 0; //equal case
}

void AudioBufferPut(uint16_t data) {
	g_audioBuffer[g_audioBufferWritePtr] = data;
	g_audioBufferWritePtr++;
	if (g_audioBufferWritePtr == g_audioBufferEntries) {
		g_audioBufferWritePtr = 0;
	}
}
