/**
 ******************************************************************************
 * @file           : encoder.c
 * @brief          : 旋转编码器驱动实现 - 基于TIM4硬件编码器模式
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "encoder.h"
#include <stdio.h>

#ifndef INPUT_DEBUG_LOG
#define INPUT_DEBUG_LOG 0
#endif

/* Private variables ---------------------------------------------------------*/
static TIM_HandleTypeDef *g_htim_encoder = NULL;  // 定时器句柄
static int32_t g_last_count = 0;                   // 上次计数值
static int32_t g_total_count = 0;                  // 累计计数值
static uint32_t g_last_time = 0;                   // 上次采样时刻
static float g_speed = 0.0f;                       // 当前速度(脉冲/秒)
static Encoder_Callback_t g_encoder_callback = NULL; // 旋转事件回调

/* Private function prototypes -----------------------------------------------*/
static int16_t Encoder_ReadRawDelta(void);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化编码器驱动
 */
Encoder_Status_t Encoder_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL) {
        return ENCODER_ERROR_INIT;
    }

    g_htim_encoder = htim;

    // 启动编码器接口
    if (HAL_TIM_Encoder_Start(g_htim_encoder, TIM_CHANNEL_ALL) != HAL_OK) {
        return ENCODER_ERROR_INIT;
    }

    // 设置计数器为中点值（避免溢出）
    __HAL_TIM_SET_COUNTER(g_htim_encoder, ENCODER_RESET_VALUE);
    g_last_count = ENCODER_RESET_VALUE;
    g_total_count = 0;
    g_last_time = HAL_GetTick();

    return ENCODER_OK;
}

/**
 * @brief 注册编码器旋转事件回调函数
 */
void Encoder_RegisterCallback(Encoder_Callback_t callback)
{
    g_encoder_callback = callback;
}

/**
 * @brief 获取编码器当前计数值
 */
int32_t Encoder_GetCount(void)
{
    return g_total_count;
}

/**
 * @brief 重置编码器计数
 */
void Encoder_Reset(void)
{
    __HAL_TIM_SET_COUNTER(g_htim_encoder, ENCODER_RESET_VALUE);
    g_last_count = ENCODER_RESET_VALUE;
    g_total_count = 0;
    g_speed = 0.0f;
}

/**
 * @brief 获取编码器变化量（相对于上次读取）
 */
int16_t Encoder_GetDelta(void)
{
    int16_t delta = Encoder_ReadRawDelta();
    g_total_count += delta;

    // 如果计数器接近边界，重置到中点
    uint32_t current_count = __HAL_TIM_GET_COUNTER(g_htim_encoder);
    if (current_count < 1000 || current_count > 64535) {
        __HAL_TIM_SET_COUNTER(g_htim_encoder, ENCODER_RESET_VALUE);
        g_last_count = ENCODER_RESET_VALUE;
    } else {
        g_last_count = current_count;
    }

    return delta;
}

/**
 * @brief 获取编码器旋转方向
 */
Encoder_Direction_t Encoder_GetDirection(void)
{
    int16_t delta = Encoder_ReadRawDelta();

    if (delta > 0) {
        return ENCODER_DIR_CW;
    } else if (delta < 0) {
        return ENCODER_DIR_CCW;
    } else {
        return ENCODER_DIR_NONE;
    }
}

/**
 * @brief 编码器处理函数（建议10-50ms调用）
 */
void Encoder_Process(void)
{
#if INPUT_DEBUG_LOG
    static uint32_t debug_time = 0;
#endif
    uint32_t current_time = HAL_GetTick();

    // 调试：每秒打印一次TIM4计数器状态
#if INPUT_DEBUG_LOG
    if (current_time - debug_time >= 1000) {
        if (g_htim_encoder != NULL) {
            uint32_t cnt = __HAL_TIM_GET_COUNTER(g_htim_encoder);
            printf("[DEBUG] TIM4 CNT:%lu last:%ld total:%ld\r\n", cnt, g_last_count, g_total_count);
        }
        debug_time = current_time;
    }
#endif

    int16_t delta = Encoder_GetDelta();

    // 计算速度（脉冲/秒）
    uint32_t elapsed_ms = current_time - g_last_time;
    if (elapsed_ms > 0) {
        g_speed = (float)delta * 1000.0f / (float)elapsed_ms;
    }
    g_last_time = current_time;

    // 如果有变化，触发回调
    if (delta != 0 && g_encoder_callback != NULL) {
        g_encoder_callback(delta);
    }
}

/**
 * @brief 获取编码器速度（脉冲/秒）
 */
float Encoder_GetSpeed(void)
{
    return g_speed;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 读取原始变化量（不更新总计数）
 */
static int16_t Encoder_ReadRawDelta(void)
{
    if (g_htim_encoder == NULL) {
        return 0;
    }

    // 读取当前计数值
    uint32_t current_count = __HAL_TIM_GET_COUNTER(g_htim_encoder);

    // 计算差值（带符号）
    int16_t delta = (int16_t)(current_count - g_last_count);

    return delta;
}
