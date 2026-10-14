/**
 ******************************************************************************
 * @file           : led.h
 * @brief          : LED驱动头文件 - RGB三色LED控制
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 * @attention
 *
 * 硬件配置：
 * - PB12: LED_R (红色LED)
 * - PC13: LED_BLUE (蓝色LED - 板载)
 * - PB14: LED_G (绿色LED)
 *
 * 所有LED：低电平点亮，高电平熄灭
 *
 ******************************************************************************
 */

#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

/**
 * @brief LED标识
 */
typedef enum {
    LED_RED = 0,        // 红色LED
    LED_GREEN,          // 绿色LED
    LED_BLUE,           // 蓝色LED
    LED_MAX
} LED_ID_t;

/**
 * @brief LED颜色组合（用于RGB混合）
 */
typedef enum {
    LED_COLOR_OFF = 0,      // 全灭
    LED_COLOR_RED,          // 红色
    LED_COLOR_GREEN,        // 绿色
    LED_COLOR_BLUE,         // 蓝色
    LED_COLOR_YELLOW,       // 黄色（红+绿）
    LED_COLOR_CYAN,         // 青色（绿+蓝）
    LED_COLOR_MAGENTA,      // 品红（红+蓝）
    LED_COLOR_WHITE         // 白色（红+绿+蓝）
} LED_Color_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化LED（可选，GPIO已由CubeMX初始化）
 */
void LED_Init(void);

/**
 * @brief 点亮指定LED
 * @param led_id LED标识
 */
void LED_On(LED_ID_t led_id);

/**
 * @brief 熄灭指定LED
 * @param led_id LED标识
 */
void LED_Off(LED_ID_t led_id);

/**
 * @brief 切换指定LED状态
 * @param led_id LED标识
 */
void LED_Toggle(LED_ID_t led_id);

/**
 * @brief 设置指定LED状态
 * @param led_id LED标识
 * @param state true=点亮，false=熄灭
 */
void LED_Set(LED_ID_t led_id, bool state);

/**
 * @brief 点亮所有LED
 */
void LED_AllOn(void);

/**
 * @brief 熄灭所有LED
 */
void LED_AllOff(void);

/**
 * @brief 设置RGB颜色
 * @param color 颜色组合
 */
void LED_SetColor(LED_Color_t color);

/**
 * @brief LED闪烁
 * @param led_id LED标识
 * @param count 闪烁次数
 * @param on_ms 点亮时长（毫秒）
 * @param off_ms 熄灭时长（毫秒）
 * @note 阻塞式延时
 */
void LED_Blink(LED_ID_t led_id, uint8_t count, uint32_t on_ms, uint32_t off_ms);

/**
 * @brief RGB流水灯效果
 * @param delay_ms 每个颜色持续时长（毫秒）
 * @note 阻塞式延时
 */
void LED_ColorCycle(uint32_t delay_ms);

/**
 * @brief 获取LED当前状态
 * @param led_id LED标识
 * @return true=点亮，false=熄灭
 */
bool LED_GetState(LED_ID_t led_id);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H */
