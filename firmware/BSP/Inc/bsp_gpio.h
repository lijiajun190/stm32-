/**
 * @file    bsp_gpio.h
 * @brief   GPIO 板级支持包头文件
 * @details 封装 LED、按键、电机使能等 GPIO 操作
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

/* LED 定义 */
typedef enum {
    BSP_LED_RED = 0,       /* PB12，低电平点亮 */
    BSP_LED_GREEN,         /* PB14，低电平点亮 */
    BSP_LED_BLUE,          /* PC13，低电平点亮 */
    BSP_LED_MAX
} Bsp_LedId_t;

/* 保留旧编号名称，避免已有调用方因重命名失效。 */
#define BSP_LED_1 BSP_LED_RED
#define BSP_LED_2 BSP_LED_GREEN
#define BSP_LED_3 BSP_LED_BLUE

/* 按键定义 */
typedef enum {
    BSP_KEY_ENCODER = 0, /* PA0，上拉，低电平按下 */
    BSP_KEY_MODE,        /* PC2，下拉，高电平按下 */
    BSP_KEY_UP,          /* PC3，下拉，高电平按下 */
    BSP_KEY_DOWN,        /* PC4，下拉，高电平按下 */
    BSP_KEY_MAX
} Bsp_KeyId_t;

/* 兼容旧接口名称。 */
#define BSP_KEY_CONFIRM BSP_KEY_DOWN

/* 函数声明 */
void Bsp_GPIO_Init(void);
void Bsp_LED_Set(Bsp_LedId_t id, bool on);
void Bsp_LED_Toggle(Bsp_LedId_t id);
bool Bsp_LED_Get(Bsp_LedId_t id);
bool Bsp_Key_Read(Bsp_KeyId_t id);
void Bsp_Motor_Enable(bool enable);
void Bsp_Motor_SetDirection(bool forward);

#endif /* BSP_GPIO_H */
