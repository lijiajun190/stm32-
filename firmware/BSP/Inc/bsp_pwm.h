/**
 * @file    bsp_pwm.h
 * @brief   PWM 板级支持包头文件
 */

#ifndef BSP_PWM_H
#define BSP_PWM_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* PWM 参数 */
#define BSP_PWM_FREQ_HZ     20000   // 20kHz
#define BSP_PWM_MAX_DUTY    1000    // 0~1000 对应 0~100.0%

/* 函数声明 */
void Bsp_PWM_Init(void);
HAL_StatusTypeDef Bsp_PWM_Start(void);
void Bsp_PWM_Stop(void);
void Bsp_PWM_SetDuty(uint16_t duty);
uint16_t Bsp_PWM_GetDuty(void);

#endif /* BSP_PWM_H */
