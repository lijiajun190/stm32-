/**
 * @file    bsp_gpio.c
 * @brief   GPIO 板级支持包实现
 */

#include "bsp_gpio.h"
#include "main.h"

/**
 * @brief  GPIO 初始化
 */
void Bsp_GPIO_Init(void)
{
    /* GPIO 模式由 CubeMX 完成，这里只写入安全初始电平。 */
    Bsp_LED_Set(BSP_LED_RED, false);
    Bsp_LED_Set(BSP_LED_GREEN, false);
    Bsp_LED_Set(BSP_LED_BLUE, false);
    Bsp_Motor_Enable(false);
    Bsp_Motor_SetDirection(true);
}

/**
 * @brief  设置 LED 状态
 * @param  id: LED ID
 * @param  on: true=点亮, false=熄灭
 */
void Bsp_LED_Set(Bsp_LedId_t id, bool on)
{
    if (id >= BSP_LED_MAX) return;

    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0U;
    switch (id) {
        case BSP_LED_RED:   port = LED_R_GPIO_Port; pin = LED_R_Pin; break;
        case BSP_LED_GREEN: port = LED_G_GPIO_Port; pin = LED_G_Pin; break;
        case BSP_LED_BLUE:  port = LED_BLUE_GPIO_Port; pin = LED_BLUE_Pin; break;
        default: return;
    }
    /* 三个 LED 均为低电平点亮。 */
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief  翻转 LED 状态
 * @param  id: LED ID
 */
void Bsp_LED_Toggle(Bsp_LedId_t id)
{
    if (id >= BSP_LED_MAX) return;

    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0U;
    switch (id) {
        case BSP_LED_RED:   port = LED_R_GPIO_Port; pin = LED_R_Pin; break;
        case BSP_LED_GREEN: port = LED_G_GPIO_Port; pin = LED_G_Pin; break;
        case BSP_LED_BLUE:  port = LED_BLUE_GPIO_Port; pin = LED_BLUE_Pin; break;
        default: return;
    }
    HAL_GPIO_TogglePin(port, pin);
}

bool Bsp_LED_Get(Bsp_LedId_t id)
{
    switch (id) {
        case BSP_LED_RED: return HAL_GPIO_ReadPin(LED_R_GPIO_Port, LED_R_Pin) == GPIO_PIN_RESET;
        case BSP_LED_GREEN: return HAL_GPIO_ReadPin(LED_G_GPIO_Port, LED_G_Pin) == GPIO_PIN_RESET;
        case BSP_LED_BLUE: return HAL_GPIO_ReadPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin) == GPIO_PIN_RESET;
        default: return false;
    }
}

/**
 * @brief  读取按键状态
 * @param  id: 按键 ID
 * @return true=按下, false=未按下
 */
bool Bsp_Key_Read(Bsp_KeyId_t id)
{
    if (id >= BSP_KEY_MAX) return false;

    GPIO_TypeDef *port = NULL;
    uint16_t pin = 0U;
    bool active_high = true;
    switch (id) {
        case BSP_KEY_ENCODER:
            port = KEY_ENCODER_GPIO_Port; pin = KEY_ENCODER_Pin; active_high = false; break;
        case BSP_KEY_MODE:
            port = KEY_MODE_GPIO_Port; pin = KEY_MODE_Pin; break;
        case BSP_KEY_UP:
            port = KEY_UP_GPIO_Port; pin = KEY_UP_Pin; break;
        case BSP_KEY_DOWN:
            port = KEY_DOWN_GPIO_Port; pin = KEY_DOWN_Pin; break;
        default: return false;
    }
    GPIO_PinState level = HAL_GPIO_ReadPin(port, pin);
    return active_high ? (level == GPIO_PIN_SET) : (level == GPIO_PIN_RESET);
}

/**
 * @brief  电机使能控制
 * @param  enable: true=使能, false=禁止
 */
void Bsp_Motor_Enable(bool enable)
{
    /* 引脚表规定 PC7 高电平使能。方向由独立接口管理。 */
    HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port, MOTOR_EN_Pin,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Bsp_Motor_SetDirection(bool forward)
{
    /* PC9 低电平为默认方向，高电平为反向。 */
    HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin,
                      forward ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
