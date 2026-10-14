/**
 ******************************************************************************
 * @file           : encoder.h
 * @brief          : 旋转编码器驱动头文件 - 硬件定时器解码
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 * @attention
 *
 * 编码器硬件配置：
 * - PB6: TIM4_CH1 (编码器A相)
 * - PB7: TIM4_CH2 (编码器B相)
 * - 定时器模式：编码器接口模式3（双边沿计数）
 *
 * 编码器类型：增量式旋转编码器（典型值：20脉冲/圈）
 *
 ******************************************************************************
 */

#ifndef __ENCODER_H
#define __ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdbool.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 编码器旋转方向
 */
typedef enum {
    ENCODER_DIR_CW = 1,     // 顺时针
    ENCODER_DIR_CCW = -1,   // 逆时针
    ENCODER_DIR_NONE = 0    // 无变化
} Encoder_Direction_t;

/**
 * @brief 编码器状态
 */
typedef enum {
    ENCODER_OK = 0,         // 正常
    ENCODER_ERROR_INIT,     // 初始化失败
    ENCODER_ERROR_TIMEOUT   // 超时
} Encoder_Status_t;

/**
 * @brief 编码器旋转事件回调函数类型
 * @param delta: 变化量（正数=顺时针，负数=逆时针）
 */
typedef void (*Encoder_Callback_t)(int16_t delta);

/* Exported constants --------------------------------------------------------*/

// 编码器参数
#define ENCODER_PPR             20      // 脉冲数/圈（Pulses Per Revolution）
#define ENCODER_MAX_COUNT       65535   // 16位定时器最大值
#define ENCODER_RESET_VALUE     32768   // 中点值（避免溢出）

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化编码器驱动
 * @param htim: 定时器句柄（TIM4）
 * @return 初始化状态
 * @note 必须在TIM4初始化之后调用
 */
Encoder_Status_t Encoder_Init(TIM_HandleTypeDef *htim);

/**
 * @brief 注册编码器旋转事件回调函数
 * @param callback: 回调函数指针
 */
void Encoder_RegisterCallback(Encoder_Callback_t callback);

/**
 * @brief 获取编码器当前计数值
 * @return 当前计数值
 */
int32_t Encoder_GetCount(void);

/**
 * @brief 重置编码器计数
 */
void Encoder_Reset(void);

/**
 * @brief 获取编码器变化量（相对于上次读取）
 * @return 变化量（正数=顺时针，负数=逆时针）
 */
int16_t Encoder_GetDelta(void);

/**
 * @brief 获取编码器旋转方向
 * @return 旋转方向
 */
Encoder_Direction_t Encoder_GetDirection(void);

/**
 * @brief 编码器处理函数（需要在定时任务中周期调用，建议10-50ms）
 * @note 检测编码器变化并触发回调
 */
void Encoder_Process(void);

/**
 * @brief 获取编码器速度（脉冲/秒）
 * @return 旋转速度
 */
float Encoder_GetSpeed(void);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H */
