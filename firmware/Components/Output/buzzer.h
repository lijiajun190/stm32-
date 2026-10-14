/**
 ******************************************************************************
 * @file           : buzzer.h
 * @brief          : 蜂鸣器驱动头文件
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 * @attention
 *
 * 硬件配置：
 * - PC6: 蜂鸣器控制引脚
 * - 有源蜂鸣器，低电平触发（0=响，1=静音）
 *
 ******************************************************************************
 */

#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 蜂鸣器音调类型
 */
typedef enum {
    BUZZER_TONE_LOW = 0,      // 低音（长鸣）
    BUZZER_TONE_MEDIUM,       // 中音（短促）
    BUZZER_TONE_HIGH          // 高音（急促）
} Buzzer_Tone_t;

/* Exported constants --------------------------------------------------------*/

#define BUZZER_BEEP_SHORT_MS    100   // 短鸣时长
#define BUZZER_BEEP_MEDIUM_MS   200   // 中等时长
#define BUZZER_BEEP_LONG_MS     500   // 长鸣时长

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化蜂鸣器（可选，GPIO已由CubeMX初始化）
 */
void Buzzer_Init(void);
/* 正式 UI 使用有界非阻塞序列；ERROR 优先，可打断普通提示。 */
typedef enum { BUZZER_SIGNAL_CLICK, BUZZER_SIGNAL_MODE, BUZZER_SIGNAL_WARNING,
               BUZZER_SIGNAL_ERROR } Buzzer_Signal_t;
void Buzzer_Request(Buzzer_Signal_t signal, uint32_t now_ms);
void Buzzer_Process(uint32_t now_ms);
bool Buzzer_IsBusy(void);

/**
 * @brief 打开蜂鸣器（持续响）
 */
void Buzzer_On(void);

/**
 * @brief 关闭蜂鸣器
 */
void Buzzer_Off(void);

/**
 * @brief 蜂鸣器状态切换
 */
void Buzzer_Toggle(void);

/**
 * @brief 蜂鸣一次（短鸣）
 * @param duration_ms 持续时长（毫秒）
 * @note 阻塞式延时，适合简单场景
 */
void Buzzer_Beep(uint32_t duration_ms);

/**
 * @brief 蜂鸣多次
 * @param count 蜂鸣次数
 * @param on_ms 每次响的时长（毫秒）
 * @param off_ms 每次静音的时长（毫秒）
 * @note 阻塞式延时
 */
void Buzzer_BeepMultiple(uint8_t count, uint32_t on_ms, uint32_t off_ms);

/**
 * @brief 按键确认音（短促"滴"一声）
 */
void Buzzer_BeepClick(void);

/**
 * @brief 警告音（两短鸣）
 */
void Buzzer_BeepWarning(void);

/**
 * @brief 错误音（三短鸣）
 */
void Buzzer_BeepError(void);

/**
 * @brief 成功音（一长鸣）
 */
void Buzzer_BeepSuccess(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
