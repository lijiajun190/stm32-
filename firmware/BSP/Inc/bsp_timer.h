/**
 * @file    bsp_timer.h
 * @brief   定时器板级支持包头文件
 */

#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 每转脉冲数需按实际码盘设置；计数频率由 APB1 时钟和 TIM3 PSC 计算。 */
#ifndef BSP_SPEED_PULSES_PER_REV
#define BSP_SPEED_PULSES_PER_REV      1UL
#endif
#ifndef BSP_SPEED_SIGNAL_TIMEOUT_MS
#define BSP_SPEED_SIGNAL_TIMEOUT_MS   1000UL
#endif
#if BSP_SPEED_PULSES_PER_REV == 0 || BSP_SPEED_SIGNAL_TIMEOUT_MS == 0
#error "Speed capture parameters must be nonzero"
#endif

typedef struct
{
    uint32_t period_ticks;  /* 相邻脉冲间的定时器计数值 */
    uint32_t timer_tick_hz;
    float frequency_hz;     /* 保留低于 1 Hz 的脉冲频率 */
    uint32_t rpm;            /* 根据每转脉冲数换算的 RPM */
    uint32_t last_capture_ms;
    uint8_t valid;
} Bsp_SpeedInfo_t;

/* 函数声明 */
void Bsp_Timer_Init(void);
void Bsp_Timer_Encoder_Start(void);
void Bsp_Timer_Encoder_Stop(void);
int32_t Bsp_Timer_Encoder_GetCount(void);
void Bsp_Timer_Encoder_ResetCount(void);
HAL_StatusTypeDef Bsp_Timer_SpeedCapture_Start(void);
void Bsp_Timer_SpeedCapture_Stop(void);
uint32_t Bsp_Timer_SpeedCapture_GetPeriod(void);
float Bsp_Timer_SpeedCapture_GetFrequency(void);
uint32_t Bsp_Timer_SpeedCapture_GetRpm(void);
uint8_t Bsp_Timer_SpeedCapture_IsValid(void);
void Bsp_Timer_SpeedCapture_GetInfo(Bsp_SpeedInfo_t *info);
/* 由 HAL_TIM_PeriodElapsedCallback 分发 TIM3 更新中断。 */
void Bsp_Timer_SpeedCapture_Overflow(void);

#endif /* BSP_TIMER_H */
