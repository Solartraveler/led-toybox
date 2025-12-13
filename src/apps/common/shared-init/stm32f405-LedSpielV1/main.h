/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/*Define this in the makefile if software is ported, which only includes main.h
  to get all the other functions or types its needs. */
#ifdef MAIN_INC_EXTRA
#include MAIN_INC_EXTRA
#endif


/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
#define Charge_Pin GPIO_PIN_13
#define Charge_GPIO_Port GPIOC
#define Line1_Pin GPIO_PIN_0
#define Line1_GPIO_Port GPIOC
#define Line2_Pin GPIO_PIN_1
#define Line2_GPIO_Port GPIOC
#define Line3_Pin GPIO_PIN_2
#define Line3_GPIO_Port GPIOC
#define Line4_Pin GPIO_PIN_3
#define Line4_GPIO_Port GPIOC
#define LightIn_Pin GPIO_PIN_0
#define LightIn_GPIO_Port GPIOA
#define LightOut1_Pin GPIO_PIN_1
#define LightOut1_GPIO_Port GPIOA
#define LightOut2_Pin GPIO_PIN_2
#define LightOut2_GPIO_Port GPIOA
#define AudioOn_Pin GPIO_PIN_3
#define AudioOn_GPIO_Port GPIOA
#define Dac1_Pin GPIO_PIN_4
#define Dac1_GPIO_Port GPIOA
#define Line5_Pin GPIO_PIN_4
#define Line5_GPIO_Port GPIOC
#define SdCs_Pin GPIO_PIN_5
#define SdCs_GPIO_Port GPIOC
#define G10_Pin GPIO_PIN_0
#define G10_GPIO_Port GPIOB
#define Boot1_Pin GPIO_PIN_1
#define Boot1_GPIO_Port GPIOB
#define B10_Pin GPIO_PIN_2
#define B10_GPIO_Port GPIOB
#define G13_Pin GPIO_PIN_10
#define G13_GPIO_Port GPIOB
#define B13_Pin GPIO_PIN_11
#define B13_GPIO_Port GPIOB
#define R13_Pin GPIO_PIN_12
#define R13_GPIO_Port GPIOB
#define G14_Pin GPIO_PIN_13
#define G14_GPIO_Port GPIOB
#define B14_Pin GPIO_PIN_14
#define B14_GPIO_Port GPIOB
#define R14_Pin GPIO_PIN_15
#define R14_GPIO_Port GPIOB
#define Key2_Pin GPIO_PIN_6
#define Key2_GPIO_Port GPIOC
#define Key3_Pin GPIO_PIN_7
#define Key3_GPIO_Port GPIOC
#define Key4_Pin GPIO_PIN_8
#define Key4_GPIO_Port GPIOC
#define Key5_Pin GPIO_PIN_9
#define Key5_GPIO_Port GPIOC
#define SdOn_Pin GPIO_PIN_8
#define SdOn_GPIO_Port GPIOA
#define PowerOff_Pin GPIO_PIN_15
#define PowerOff_GPIO_Port GPIOA
#define Key6_Pin GPIO_PIN_10
#define Key6_GPIO_Port GPIOC
#define RemoteOn_Pin GPIO_PIN_11
#define RemoteOn_GPIO_Port GPIOC
#define RemoteIn_Pin GPIO_PIN_12
#define RemoteIn_GPIO_Port GPIOC
#define Key1_Pin GPIO_PIN_2
#define Key1_GPIO_Port GPIOD
#define R10_Pin GPIO_PIN_3
#define R10_GPIO_Port GPIOB
#define G11_Pin GPIO_PIN_4
#define G11_GPIO_Port GPIOB
#define B11_Pin GPIO_PIN_5
#define B11_GPIO_Port GPIOB
#define R11_Pin GPIO_PIN_6
#define R11_GPIO_Port GPIOB
#define G12_Pin GPIO_PIN_7
#define G12_GPIO_Port GPIOB
#define B12_Pin GPIO_PIN_8
#define B12_GPIO_Port GPIOB
#define R12_Pin GPIO_PIN_9
#define R12_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
