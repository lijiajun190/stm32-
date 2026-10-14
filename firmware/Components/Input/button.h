/**
 ******************************************************************************
 * @file           : button.h
 * @brief          : 按键驱动头文件 - 独立按键管理
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 * @attention
 *
 * 按键硬件配置：
 * - PA0: 编码器按键 (EXTI0中断)
 * - PC2: 模式切换按键 (EXTI2中断)
 * - PC3: 参数增加按键 (EXTI3中断)
 * - PC4: 参数减少按键 (普通输入)
 *
 * PA0：上拉，按下接地，低有效；PC2/PC3/PC4：下拉，按下接3.3V，高有效。
 *
 ******************************************************************************
 */

#ifndef __BUTTON_H
#define __BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdbool.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 按键ID定义
 */
typedef enum {
    BUTTON_ENCODER = 0,    // PA0 - 编码器按键
    BUTTON_MODE,           // PC2 - 模式切换
    BUTTON_UP,             // PC3 - 参数增加
    BUTTON_DOWN,           // PC4 - 参数减少
    BUTTON_MAX
} Button_ID_t;

/**
 * @brief 按键事件类型
 */
typedef enum {
    BUTTON_EVENT_NONE = 0,      // 无事件
    BUTTON_EVENT_PRESS,         // 按下
    BUTTON_EVENT_RELEASE,       // 释放
    BUTTON_EVENT_CLICK,         // 单击（按下后释放）
    BUTTON_EVENT_LONG_PRESS,    // 长按
    BUTTON_EVENT_REPEAT         // 连续触发（长按后周期触发）
} Button_Event_t;

/**
 * @brief 按键状态
 */
typedef enum {
    BUTTON_STATE_IDLE = 0,      // 空闲
    BUTTON_STATE_DEBOUNCE,      // 消抖中
    BUTTON_STATE_PRESSED,       // 已按下
    BUTTON_STATE_LONG_PRESS,    // 长按中
    BUTTON_STATE_REPEAT         // 连续触发中
} Button_State_t;

/**
 * @brief 按键事件回调函数类型
 * @param button_id: 按键ID
 * @param event: 事件类型
 */
typedef void (*Button_Callback_t)(Button_ID_t button_id, Button_Event_t event);

/**
 * @brief 按键配置结构
 */
typedef struct {
    GPIO_TypeDef *gpio_port;    // GPIO端口
    uint16_t gpio_pin;          // GPIO引脚
    bool active_high;           // true=按下为高电平, false=按下为低电平
    uint16_t debounce_ms;       // 消抖时间(ms)
    uint16_t long_press_ms;     // 长按触发时间(ms)
    uint16_t repeat_ms;         // 连续触发周期(ms)
} Button_Config_t;

/**
 * @brief 按键运行时数据
 */
typedef struct {
    Button_State_t state;       // 当前状态
    uint32_t press_time;        // 按下时刻(ms)
    uint32_t last_event_time;   // 上次事件时刻(ms)
    bool is_pressed;            // 当前物理电平状态
    bool last_level;            // 上次读取的电平
    bool release_pending;       // 释放消抖尚未完成
    uint32_t release_time;      // 首次检测释放的时刻
    bool ignore_until_release; // 上电已按住的按键，释放前不产生操作事件
} Button_Runtime_t;

/* Exported constants --------------------------------------------------------*/

// 默认配置参数
#define BUTTON_DEBOUNCE_MS      20      // 消抖时间
#define BUTTON_LONG_PRESS_MS    1000    // 长按触发时间
#define BUTTON_REPEAT_MS        200     // 连续触发周期

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化按键驱动
 * @note 必须在GPIO初始化之后调用
 */
void Button_Init(void);

/**
 * @brief 注册按键事件回调函数
 * @param callback: 回调函数指针
 */
void Button_RegisterCallback(Button_Callback_t callback);

/**
 * @brief 按键扫描处理（需要在定时任务中周期调用，建议10ms）
 */
void Button_Process(void);

/**
 * @brief 获取按键当前状态
 * @param button_id: 按键ID
 * @return 当前按键状态
 */
Button_State_t Button_GetState(Button_ID_t button_id);

/**
 * @brief 检查按键是否被按下
 * @param button_id: 按键ID
 * @return true=按下, false=未按下
 */
bool Button_IsPressed(Button_ID_t button_id);

/**
 * @brief GPIO中断回调（由HAL_GPIO_EXTI_Callback调用）
 * @param GPIO_Pin: 触发中断的引脚
 */
void Button_IRQHandler(uint16_t GPIO_Pin);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H */
