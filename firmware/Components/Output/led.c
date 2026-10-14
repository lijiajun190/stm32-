/**
 ******************************************************************************
 * @file           : led.c
 * @brief          : LED驱动实现 - RGB三色LED控制
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "led.h"
#include "bsp_gpio.h"

/* Private defines -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* GPIO 映射和有效电平仅由 BSP 管理，组件层提供颜色和闪烁等组合操作。 */
static const Bsp_LedId_t led_ids[LED_MAX] = {
    BSP_LED_RED, BSP_LED_GREEN, BSP_LED_BLUE
};

/* Private function prototypes -----------------------------------------------*/
static inline void LED_Write(LED_ID_t led_id, bool state);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化LED
 */
void LED_Init(void)
{
    // GPIO已由CubeMX初始化，这里只需确保初始状态为全灭
    LED_AllOff();
}

/**
 * @brief 点亮指定LED
 */
void LED_On(LED_ID_t led_id)
{
    if ((unsigned)led_id >= LED_MAX) return;
    LED_Write(led_id, true);
}

/**
 * @brief 熄灭指定LED
 */
void LED_Off(LED_ID_t led_id)
{
    if ((unsigned)led_id >= LED_MAX) return;
    LED_Write(led_id, false);
}

/**
 * @brief 切换指定LED状态
 */
void LED_Toggle(LED_ID_t led_id)
{
    if ((unsigned)led_id >= LED_MAX) return;
    Bsp_LED_Toggle(led_ids[led_id]);
}

/**
 * @brief 设置指定LED状态
 */
void LED_Set(LED_ID_t led_id, bool state)
{
    if (state) {
        LED_On(led_id);
    } else {
        LED_Off(led_id);
    }
}

/**
 * @brief 点亮所有LED
 */
void LED_AllOn(void)
{
    for (LED_ID_t i = LED_RED; i < LED_MAX; i++) {
        LED_On(i);
    }
}

/**
 * @brief 熄灭所有LED
 */
void LED_AllOff(void)
{
    for (LED_ID_t i = LED_RED; i < LED_MAX; i++) {
        LED_Off(i);
    }
}

/**
 * @brief 设置RGB颜色
 */
void LED_SetColor(LED_Color_t color)
{
    switch (color) {
        case LED_COLOR_OFF:
            LED_AllOff();
            break;

        case LED_COLOR_RED:
            LED_On(LED_RED);
            LED_Off(LED_GREEN);
            LED_Off(LED_BLUE);
            break;

        case LED_COLOR_GREEN:
            LED_Off(LED_RED);
            LED_On(LED_GREEN);
            LED_Off(LED_BLUE);
            break;

        case LED_COLOR_BLUE:
            LED_Off(LED_RED);
            LED_Off(LED_GREEN);
            LED_On(LED_BLUE);
            break;

        case LED_COLOR_YELLOW:  // 红+绿
            LED_On(LED_RED);
            LED_On(LED_GREEN);
            LED_Off(LED_BLUE);
            break;

        case LED_COLOR_CYAN:  // 绿+蓝
            LED_Off(LED_RED);
            LED_On(LED_GREEN);
            LED_On(LED_BLUE);
            break;

        case LED_COLOR_MAGENTA:  // 红+蓝
            LED_On(LED_RED);
            LED_Off(LED_GREEN);
            LED_On(LED_BLUE);
            break;

        case LED_COLOR_WHITE:  // 红+绿+蓝
            LED_AllOn();
            break;

        default:
            LED_AllOff();
            break;
    }
}

/**
 * @brief LED闪烁
 */
void LED_Blink(LED_ID_t led_id, uint8_t count, uint32_t on_ms, uint32_t off_ms)
{
    if ((unsigned)led_id >= LED_MAX) return;

    for (uint8_t i = 0; i < count; i++) {
        LED_On(led_id);
        HAL_Delay(on_ms);
        LED_Off(led_id);

        if (i < count - 1) {  // 最后一次不延时
            HAL_Delay(off_ms);
        }
    }
}

/**
 * @brief RGB流水灯效果
 */
void LED_ColorCycle(uint32_t delay_ms)
{
    const LED_Color_t colors[] = {
        LED_COLOR_RED,
        LED_COLOR_GREEN,
        LED_COLOR_BLUE,
        LED_COLOR_YELLOW,
        LED_COLOR_CYAN,
        LED_COLOR_MAGENTA,
        LED_COLOR_WHITE
    };

    for (uint8_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        LED_SetColor(colors[i]);
        HAL_Delay(delay_ms);
    }

    LED_AllOff();
}

/**
 * @brief 获取LED当前状态
 */
bool LED_GetState(LED_ID_t led_id)
{
    if ((unsigned)led_id >= LED_MAX) return false;
    return Bsp_LED_Get(led_ids[led_id]);
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 写LED状态（底层函数）
 */
static inline void LED_Write(LED_ID_t led_id, bool state)
{
    Bsp_LED_Set(led_ids[led_id], state);
}
