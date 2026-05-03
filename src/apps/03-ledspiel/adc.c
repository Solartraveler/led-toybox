#include <stdint.h>
#include <stdio.h>

#include "adc.h"

#include "ledspiellib/simpleadc.h"
#include "main.h"

#define ADC_MAX 4095

#define ADC_LIM (ADC_MAX / 4)

//range: min: 1.18V, typ: 1.21V, max: 1.24V
#define ADC_INT_REF 1.21

uint16_t AdcBatteryVoltageGet(void) {
	uint16_t adcBat = AdcGet(18);
	uint16_t vBatMv = 6.6 * 1000.0 * (float)adcBat / (float)ADC_MAX; //vBat has a 1:2 voltage divider. Reference voltage is 3.3V.
	return vBatMv;
}

uint32_t AdcLightLevelGet(void) {
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = LightIn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LightIn_GPIO_Port, &GPIO_InitStruct);

	//check for 1kOhm
	GPIO_InitStruct.Pin = LightOut2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LightOut2_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(LightOut2_GPIO_Port, LightOut2_Pin, GPIO_PIN_SET);

	HAL_Delay(1);
	uint32_t lightValue = AdcGet(0);

	GPIO_InitStruct.Pin = LightOut2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LightOut2_GPIO_Port, &GPIO_InitStruct);

	if (lightValue < ADC_LIM) {
		return lightValue;
	}

	//Check for 56kOhm
	GPIO_InitStruct.Pin = LightOut1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LightOut1_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(LightOut1_GPIO_Port, LightOut1_Pin, GPIO_PIN_SET);

	HAL_Delay(1);
	lightValue = AdcGet(0);

	GPIO_InitStruct.Pin = LightOut1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LightOut2_GPIO_Port, &GPIO_InitStruct);

	if (lightValue < ADC_LIM) {
		return lightValue | 0x4000;
	}

	//Check for 1MOhm
	HAL_Delay(1);
	lightValue = AdcGet(0);

	return lightValue | 0x8000;
}

int32_t AdcTemperatureGet(void) {
	uint16_t adcTemp = AdcGet(16);
	float vSense = (float)ADC_INT_REF * (float)adcTemp / (float)ADC_MAX;
	float temperatureCelsius = (vSense - 0.76f) / 2.5f + 25.0f;
	return temperatureCelsius * 1000.0f;
}

uint16_t AdcAvrefGet(void) {
	return 3300;
}

uint16_t AdcAvccGet(void) {
	float adcIntRef = AdcGet(17);
	if (adcIntRef) {
		return ((float)ADC_INT_REF * (float)ADC_MAX * 1000.0f) / adcIntRef;
	}
	return 0;
}
