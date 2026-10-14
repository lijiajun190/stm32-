/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#define LED_BLUE_Pin GPIO_PIN_13
#define LED_BLUE_GPIO_Port GPIOC
#define SPARE_ADC1_Pin GPIO_PIN_0
#define SPARE_ADC1_GPIO_Port GPIOC
#define SPARE_ADC2_Pin GPIO_PIN_1
#define SPARE_ADC2_GPIO_Port GPIOC
#define KEY_MODE_Pin GPIO_PIN_2
#define KEY_MODE_GPIO_Port GPIOC
#define KEY_MODE_EXTI_IRQn EXTI2_IRQn
#define KEY_UP_Pin GPIO_PIN_3
#define KEY_UP_GPIO_Port GPIOC
#define KEY_UP_EXTI_IRQn EXTI3_IRQn
#define KEY_ENCODER_Pin GPIO_PIN_0
#define KEY_ENCODER_GPIO_Port GPIOA
#define KEY_ENCODER_EXTI_IRQn EXTI0_IRQn
#define MQ135_ADC_Pin GPIO_PIN_4
#define MQ135_ADC_GPIO_Port GPIOA
#define WIND_SPEED_ADC_Pin GPIO_PIN_5
#define WIND_SPEED_ADC_GPIO_Port GPIOA
#define KEY_DOWN_Pin GPIO_PIN_4
#define KEY_DOWN_GPIO_Port GPIOC
#define OLED_SCL_Pin GPIO_PIN_10
#define OLED_SCL_GPIO_Port GPIOB
#define LED_R_Pin GPIO_PIN_12
#define LED_R_GPIO_Port GPIOB
#define LED_G_Pin GPIO_PIN_14
#define LED_G_GPIO_Port GPIOB
#define MOTOR_PWM_Pin GPIO_PIN_15
#define MOTOR_PWM_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_6
#define BUZZER_GPIO_Port GPIOC
#define MOTOR_EN_Pin GPIO_PIN_7
#define MOTOR_EN_GPIO_Port GPIOC
#define MOTOR_DIR_Pin GPIO_PIN_9
#define MOTOR_DIR_GPIO_Port GPIOC
#define OLED_SDA_Pin GPIO_PIN_3
#define OLED_SDA_GPIO_Port GPIOB
#define ENCODER_A_Pin GPIO_PIN_6
#define ENCODER_A_GPIO_Port GPIOB
#define ENCODER_B_Pin GPIO_PIN_7
#define ENCODER_B_GPIO_Port GPIOB
#define SENSOR_SCL_Pin GPIO_PIN_8
#define SENSOR_SCL_GPIO_Port GPIOB
#define SENSOR_SDA_Pin GPIO_PIN_9
#define SENSOR_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
