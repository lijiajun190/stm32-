/**
 ******************************************************************************
 * @file           : button.c
 * @brief          : 按键驱动实现 - 支持消抖、长按、连续触发
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "button.h"
#include <string.h>
#include <stdio.h>

/* 正式启动默认关闭输入测试日志；需要观察原始电平时可编译期开启。 */
#ifndef INPUT_DEBUG_LOG
#define INPUT_DEBUG_LOG 0
#endif

/* Private variables ---------------------------------------------------------*/

// 按键配置表
static const Button_Config_t button_configs[BUTTON_MAX] = {
    // BUTTON_ENCODER (PA0) - 按下接地，上拉到3.3V，按下时为低电平
    {
        .gpio_port = GPIOA,
        .gpio_pin = GPIO_PIN_0,
        .active_high = false,  // 按下时为低电平
        .debounce_ms = BUTTON_DEBOUNCE_MS,
        .long_press_ms = BUTTON_LONG_PRESS_MS,
        .repeat_ms = BUTTON_REPEAT_MS
    },
    // BUTTON_MODE (PC2) - 下拉输入，按下接3.3V，按下时为高电平
    {
        .gpio_port = GPIOC,
        .gpio_pin = GPIO_PIN_2,
        .active_high = true,   // 按下时为高电平
        .debounce_ms = BUTTON_DEBOUNCE_MS,
        .long_press_ms = BUTTON_LONG_PRESS_MS,
        .repeat_ms = BUTTON_REPEAT_MS
    },
    // BUTTON_UP (PC3) - 下拉输入，按下接3.3V，按下时为高电平
    {
        .gpio_port = GPIOC,
        .gpio_pin = GPIO_PIN_3,
        .active_high = true,   // 按下时为高电平
        .debounce_ms = BUTTON_DEBOUNCE_MS,
        .long_press_ms = BUTTON_LONG_PRESS_MS,
        .repeat_ms = BUTTON_REPEAT_MS
    },
    // BUTTON_DOWN (PC4) - 下拉输入，按下接3.3V，按下时为高电平
    {
        .gpio_port = GPIOC,
        .gpio_pin = GPIO_PIN_4,
        .active_high = true,   // 按下时为高电平
        .debounce_ms = BUTTON_DEBOUNCE_MS,
        .long_press_ms = BUTTON_LONG_PRESS_MS,
        .repeat_ms = BUTTON_REPEAT_MS
    }
};

// 按键运行时数据
static Button_Runtime_t button_runtime[BUTTON_MAX];

// 事件回调函数
static Button_Callback_t g_button_callback = NULL;

/* Private function prototypes -----------------------------------------------*/
static bool Button_ReadLevel(Button_ID_t button_id);
static void Button_EmitEvent(Button_ID_t button_id, Button_Event_t event);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化按键驱动
 */
void Button_Init(void)
{
    // 清空运行时数据
    memset(button_runtime, 0, sizeof(button_runtime));

    // 读取初始电平
    for (Button_ID_t i = BUTTON_ENCODER; i < BUTTON_MAX; i++) {
        button_runtime[i].last_level = Button_ReadLevel(i);
        /* 上电已经按住的按键不能在调度器启动后直接触发电机命令。 */
        bool level = button_runtime[i].last_level;
        button_runtime[i].ignore_until_release = button_configs[i].active_high ? level : !level;
        button_runtime[i].state = BUTTON_STATE_IDLE;
    }
}

/**
 * @brief 注册按键事件回调函数
 */
void Button_RegisterCallback(Button_Callback_t callback)
{
    g_button_callback = callback;
}

/**
 * @brief 按键扫描处理（建议10ms调用一次）
 */
void Button_Process(void)
{
    uint32_t current_time = HAL_GetTick();
#if INPUT_DEBUG_LOG
    static uint32_t debug_time = 0;
#endif

    for (Button_ID_t i = BUTTON_ENCODER; i < BUTTON_MAX; i++) {
        const Button_Config_t *cfg = &button_configs[i];
        Button_Runtime_t *rt = &button_runtime[i];

        // 读取当前电平
        bool current_level = Button_ReadLevel(i);
        bool is_pressed = (cfg->active_high) ? current_level : !current_level;

        // 调试：每秒打印一次所有按键状态
#if INPUT_DEBUG_LOG
        if (current_time - debug_time >= 1000 && i == BUTTON_ENCODER) {
            printf("[DEBUG] Button GPIO states - PA0:%d PC2:%d PC3:%d PC4:%d\r\n",
                   HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0),
                   HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2),
                   HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_3),
                   HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4));
            debug_time = current_time;
        }
#endif

        /* 释放同样需要消抖，避免抖动把一次短按拆成多个点击。 */
        if (rt->ignore_until_release || rt->is_pressed) {
            if (is_pressed) {
                rt->release_pending = false;
            } else {
                if (!rt->release_pending) {
                    rt->release_pending = true;
                    rt->release_time = current_time;
                }
                if (current_time - rt->release_time < cfg->debounce_ms) {
                    continue;
                }
                rt->release_pending = false;
                if (rt->ignore_until_release) {
                    rt->ignore_until_release = false;
                    continue;
                }
            }
        }
        if (rt->ignore_until_release) {
            continue;
        }

        // 状态机处理
        switch (rt->state) {
            case BUTTON_STATE_IDLE:
                if (is_pressed && !rt->is_pressed) {
                    // 检测到按下沿
                    rt->state = BUTTON_STATE_DEBOUNCE;
                    rt->press_time = current_time;
                }
                break;

            case BUTTON_STATE_DEBOUNCE:
                if (current_time - rt->press_time >= cfg->debounce_ms) {
                    if (is_pressed) {
                        // 消抖确认，按键有效
                        rt->state = BUTTON_STATE_PRESSED;
                        rt->is_pressed = true;
                        rt->last_event_time = current_time;
                        Button_EmitEvent(i, BUTTON_EVENT_PRESS);
                    } else {
                        // 抖动，返回空闲
                        rt->state = BUTTON_STATE_IDLE;
                    }
                }
                break;

            case BUTTON_STATE_PRESSED:
                if (!is_pressed) {
                    // 释放
                    rt->state = BUTTON_STATE_IDLE;
                    rt->is_pressed = false;
                    Button_EmitEvent(i, BUTTON_EVENT_RELEASE);
                    Button_EmitEvent(i, BUTTON_EVENT_CLICK);
                } else if (current_time - rt->press_time >= cfg->long_press_ms) {
                    // 长按触发
                    rt->state = BUTTON_STATE_LONG_PRESS;
                    rt->last_event_time = current_time;
                    Button_EmitEvent(i, BUTTON_EVENT_LONG_PRESS);
                }
                break;

            case BUTTON_STATE_LONG_PRESS:
                if (!is_pressed) {
                    // 长按释放
                    rt->state = BUTTON_STATE_IDLE;
                    rt->is_pressed = false;
                    Button_EmitEvent(i, BUTTON_EVENT_RELEASE);
                } else if (current_time - rt->last_event_time >= cfg->repeat_ms) {
                    // 进入连续触发
                    rt->state = BUTTON_STATE_REPEAT;
                    rt->last_event_time = current_time;
                    Button_EmitEvent(i, BUTTON_EVENT_REPEAT);
                }
                break;

            case BUTTON_STATE_REPEAT:
                if (!is_pressed) {
                    // 释放
                    rt->state = BUTTON_STATE_IDLE;
                    rt->is_pressed = false;
                    Button_EmitEvent(i, BUTTON_EVENT_RELEASE);
                } else if (current_time - rt->last_event_time >= cfg->repeat_ms) {
                    // 连续触发
                    rt->last_event_time = current_time;
                    Button_EmitEvent(i, BUTTON_EVENT_REPEAT);
                }
                break;

            default:
                rt->state = BUTTON_STATE_IDLE;
                break;
        }

        rt->last_level = current_level;
    }
}

/**
 * @brief 获取按键当前状态
 */
Button_State_t Button_GetState(Button_ID_t button_id)
{
    if ((unsigned)button_id >= BUTTON_MAX) {
        return BUTTON_STATE_IDLE;
    }
    return button_runtime[button_id].state;
}

/**
 * @brief 检查按键是否被按下
 */
bool Button_IsPressed(Button_ID_t button_id)
{
    if ((unsigned)button_id >= BUTTON_MAX) {
        return false;
    }
    return button_runtime[button_id].is_pressed;
}

/**
 * @brief GPIO中断回调（由HAL_GPIO_EXTI_Callback调用）
 */
void Button_IRQHandler(uint16_t GPIO_Pin)
{
    // 中断只做标记，实际处理在Button_Process中
    // 这样可以避免在中断中执行复杂逻辑

    // 可以在这里做快速响应（如立即触发某个任务）
    // 但按键消抖和事件发送仍由轮询处理
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 读取按键电平
 */
static bool Button_ReadLevel(Button_ID_t button_id)
{
    if ((unsigned)button_id >= BUTTON_MAX) {
        return false;
    }

    const Button_Config_t *cfg = &button_configs[button_id];
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(cfg->gpio_port, cfg->gpio_pin);

    return (pin_state == GPIO_PIN_SET);
}

/**
 * @brief 发送按键事件
 */
static void Button_EmitEvent(Button_ID_t button_id, Button_Event_t event)
{
    if (g_button_callback != NULL) {
        g_button_callback(button_id, event);
    }
}
