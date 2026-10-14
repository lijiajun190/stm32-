/**
 * @file    bsp_adc.h
 * @brief   ADC 板级支持包头文件
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ADC 通道定义 */
#define BSP_ADC_CH_MQ135       0  // PA4
#define BSP_ADC_CH_WIND_SPEED  1  // PA5
#define BSP_ADC_CH_RESERVE1    2  // PC0
#define BSP_ADC_CH_RESERVE2    3  // PC1
#define BSP_ADC_CH_MAX         4

/* 函数声明 */
void Bsp_ADC_Init(void);
uint16_t Bsp_ADC_GetValue(uint8_t channel);
float Bsp_ADC_GetVoltage(uint8_t channel);
/* 最新四通道样本副本；连续扫描通道存在微秒级采样时间差。 */
bool Bsp_ADC_GetSnapshot(uint16_t raw[BSP_ADC_CH_MAX]);
HAL_StatusTypeDef Bsp_ADC_StartDMA(void);
void Bsp_ADC_StopDMA(void);

#endif /* BSP_ADC_H */
