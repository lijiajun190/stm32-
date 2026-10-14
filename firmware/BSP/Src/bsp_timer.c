/**
 * @file    bsp_timer.c
 * @brief   定时器板级支持包实现
 */

#include "bsp_timer.h"
#include "tim.h"

/* 更新中断把 16 位 TIM3 扩展为 32 位计数，支持两个脉冲之间多次溢出。 */
static volatile uint32_t speed_counter_base = 0;
static volatile uint32_t speed_last_capture = 0;
static volatile uint32_t speed_period_ticks = 0;
static volatile uint32_t speed_last_capture_ms = 0;
static volatile uint8_t speed_capture_valid = 0;
static volatile uint8_t speed_has_previous_capture = 0;
static volatile uint8_t speed_capture_running = 0;
static uint32_t speed_timer_tick_hz = 0;

/**
 * @brief  定时器初始化
 */
void Bsp_Timer_Init(void)
{
    Bsp_Timer_SpeedCapture_Stop();
}

/**
 * @brief  启动编码器模式（TIM4）
 */
void Bsp_Timer_Encoder_Start(void)
{
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

/**
 * @brief  停止编码器模式
 */
void Bsp_Timer_Encoder_Stop(void)
{
    HAL_TIM_Encoder_Stop(&htim4, TIM_CHANNEL_ALL);
}

/**
 * @brief  获取编码器计数值
 * @return 计数值（有符号）
 */
int32_t Bsp_Timer_Encoder_GetCount(void)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim4);
}

/**
 * @brief  重置编码器计数
 */
void Bsp_Timer_Encoder_ResetCount(void)
{
    __HAL_TIM_SET_COUNTER(&htim4, 0);
}

/**
 * @brief  启动测速输入捕获（TIM3）
 */
HAL_StatusTypeDef Bsp_Timer_SpeedCapture_Start(void)
{
    RCC_ClkInitTypeDef clocks;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();
    if (clocks.APB1CLKDivider != RCC_HCLK_DIV1) {
        timer_clock *= 2U;
    }
    speed_timer_tick_hz = timer_clock / (htim3.Init.Prescaler + 1U);
    Bsp_Timer_SpeedCapture_Stop();
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE | TIM_FLAG_CC1 | TIM_FLAG_CC1OF);
    speed_counter_base = 0;
    speed_has_previous_capture = 0;
    speed_capture_valid = 0;
    speed_capture_running = 1U;
    /* 必须同时使能更新中断，否则低速时会丢失完整的计数周期。 */
    __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);
    HAL_StatusTypeDef status = HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
    if (status != HAL_OK) {
        Bsp_Timer_SpeedCapture_Stop();
    }
    return status;
}

/**
 * @brief  停止测速输入捕获
 */
void Bsp_Timer_SpeedCapture_Stop(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    speed_capture_running = 0U;
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);
    HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_1);
    speed_has_previous_capture = 0U;
    speed_capture_valid = 0U;
    speed_period_ticks = 0U;
    speed_last_capture_ms = 0U;
    __set_PRIMASK(primask);
}

/**
 * @brief  获取捕获周期
 * @return 周期值（定时器计数单位）
 */
uint32_t Bsp_Timer_SpeedCapture_GetPeriod(void)
{
    Bsp_SpeedInfo_t info;
    Bsp_Timer_SpeedCapture_GetInfo(&info);
    return info.period_ticks;
}

float Bsp_Timer_SpeedCapture_GetFrequency(void)
{
    Bsp_SpeedInfo_t info;
    Bsp_Timer_SpeedCapture_GetInfo(&info);
    return info.frequency_hz;
}

uint32_t Bsp_Timer_SpeedCapture_GetRpm(void)
{
    Bsp_SpeedInfo_t info;
    Bsp_Timer_SpeedCapture_GetInfo(&info);
    return info.rpm;
}

uint8_t Bsp_Timer_SpeedCapture_IsValid(void)
{
    Bsp_SpeedInfo_t info;
    Bsp_Timer_SpeedCapture_GetInfo(&info);
    return info.valid;
}

void Bsp_Timer_SpeedCapture_GetInfo(Bsp_SpeedInfo_t *info)
{
    if (info == NULL) {
        return;
    }

    /* 短临界区只复制整数状态，避免任务读取到新旧两组捕获数据。 */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t period = speed_period_ticks;
    info->last_capture_ms = speed_last_capture_ms;
    uint32_t age_ms = HAL_GetTick() - info->last_capture_ms;
    info->valid = speed_capture_running && speed_capture_valid && period != 0U &&
                  age_ms < BSP_SPEED_SIGNAL_TIMEOUT_MS;
    info->timer_tick_hz = speed_timer_tick_hz;
    __set_PRIMASK(primask);

    info->period_ticks = info->valid ? period : 0U;
    info->frequency_hz = info->valid ? (float)info->timer_tick_hz / period : 0.0f;
    /* 先用完整周期换算 RPM，再取整数，避免先取整频率导致低速误差。 */
    info->rpm = info->valid ? (uint32_t)(((uint64_t)info->timer_tick_hz * 60U) /
                ((uint64_t)period * BSP_SPEED_PULSES_PER_REV)) : 0U;
}

void Bsp_Timer_SpeedCapture_Overflow(void)
{
    if (speed_capture_running) {
        speed_counter_base += __HAL_TIM_GET_AUTORELOAD(&htim3) + 1U;
    }
}

/**
 * @brief TIM3 输入捕获回调
 * @note  中断只保存周期和时间戳，单位换算在任务上下文执行。
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim == NULL || htim->Instance != TIM3 ||
        htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1 || !speed_capture_running) {
        return;
    }

    uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    uint32_t now_ms = HAL_GetTick();
    uint32_t base = speed_counter_base;
    /* HAL 先处理 CC1 再处理 UPDATE。UIF 未处理时区分溢出前后的捕获。 */
    if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE) != RESET &&
        capture <= __HAL_TIM_GET_COUNTER(htim)) {
        base += __HAL_TIM_GET_AUTORELOAD(htim) + 1U;
    }
    uint32_t extended_capture = base + capture;
    /* 过捕获表示至少漏了一个边沿，本次只作为新的测量基准。 */
    uint8_t overcapture = __HAL_TIM_GET_FLAG(htim, TIM_FLAG_CC1OF) != RESET;
    if (overcapture) {
        __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_CC1OF);
    }

    if (speed_has_previous_capture && !overcapture &&
        (now_ms - speed_last_capture_ms) < BSP_SPEED_SIGNAL_TIMEOUT_MS) {
        uint32_t period = extended_capture - speed_last_capture;
        if (period != 0U) {
            speed_period_ticks = period;
            speed_capture_valid = 1U;
        } else {
            speed_capture_valid = 0U;
        }
    } else {
        /* 超时后第一个脉冲无有效周期，必须等第二个脉冲才能恢复测速。 */
        speed_capture_valid = 0U;
    }

    speed_last_capture = extended_capture;
    speed_last_capture_ms = now_ms;
    speed_has_previous_capture = 1U;
}
