/**
 ******************************************************************************
 * @file           : buzzer.c
 * @brief          : 蜂鸣器驱动实现
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "buzzer.h"

/* Private defines -----------------------------------------------------------*/
// 有源蜂鸣器：低电平触发（0=响，1=静音）
#define BUZZER_ACTIVE_LOW   1

#if BUZZER_ACTIVE_LOW
    #define BUZZER_ON()     HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET)
    #define BUZZER_OFF()    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET)
#else
    #define BUZZER_ON()     HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET)
    #define BUZZER_OFF()    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET)
#endif

static bool sequence_active, sequence_on;
static uint8_t remaining_beeps;
static uint32_t phase_started_ms, on_time_ms, off_time_ms;
static Buzzer_Signal_t sequence_signal;

void Buzzer_Request(Buzzer_Signal_t signal, uint32_t now_ms)
{
    if ((unsigned)signal > BUZZER_SIGNAL_ERROR) return;
    /* 普通按键反馈不能覆盖故障提示；每次请求均为有限次鸣叫。 */
    if (sequence_active && signal < sequence_signal) return;
    sequence_signal = signal;
    remaining_beeps = signal == BUZZER_SIGNAL_ERROR ? 3U :
                      signal == BUZZER_SIGNAL_WARNING ? 2U : 1U;
    on_time_ms = signal == BUZZER_SIGNAL_MODE ? 200U : 50U;
    off_time_ms = 100U;
    sequence_active = sequence_on = true;
    phase_started_ms = now_ms;
    BUZZER_ON();
}

void Buzzer_Process(uint32_t now_ms)
{
    if (!sequence_active) return;
    uint32_t duration = sequence_on ? on_time_ms : off_time_ms;
    if ((uint32_t)(now_ms - phase_started_ms) < duration) return;
    phase_started_ms = now_ms;
    if (sequence_on) {
        BUZZER_OFF();
        sequence_on = false;
        if (--remaining_beeps == 0U) sequence_active = false;
    } else {
        sequence_on = true;
        BUZZER_ON();
    }
}

bool Buzzer_IsBusy(void) { return sequence_active; }

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化蜂鸣器
 */
void Buzzer_Init(void)
{
    // GPIO已由CubeMX初始化，这里只需确保初始状态为关闭
    Buzzer_Off();
    sequence_active = sequence_on = false;
    remaining_beeps = 0;
}

/**
 * @brief 打开蜂鸣器
 */
void Buzzer_On(void)
{
    BUZZER_ON();
}

/**
 * @brief 关闭蜂鸣器
 */
void Buzzer_Off(void)
{
    BUZZER_OFF();
}

/**
 * @brief 蜂鸣器状态切换
 */
void Buzzer_Toggle(void)
{
    HAL_GPIO_TogglePin(BUZZER_GPIO_Port, BUZZER_Pin);
}

/**
 * @brief 蜂鸣一次
 */
void Buzzer_Beep(uint32_t duration_ms)
{
    BUZZER_ON();
    HAL_Delay(duration_ms);
    BUZZER_OFF();
}

/**
 * @brief 蜂鸣多次
 */
void Buzzer_BeepMultiple(uint8_t count, uint32_t on_ms, uint32_t off_ms)
{
    for (uint8_t i = 0; i < count; i++) {
        BUZZER_ON();
        HAL_Delay(on_ms);
        BUZZER_OFF();

        if (i < count - 1) {  // 最后一次不延时
            HAL_Delay(off_ms);
        }
    }
}

/**
 * @brief 按键确认音（短促"滴"一声）
 */
void Buzzer_BeepClick(void)
{
    Buzzer_Beep(BUZZER_BEEP_SHORT_MS);
}

/**
 * @brief 警告音（两短鸣）
 */
void Buzzer_BeepWarning(void)
{
    Buzzer_BeepMultiple(2, BUZZER_BEEP_SHORT_MS, BUZZER_BEEP_SHORT_MS);
}

/**
 * @brief 错误音（三短鸣）
 */
void Buzzer_BeepError(void)
{
    Buzzer_BeepMultiple(3, BUZZER_BEEP_SHORT_MS, BUZZER_BEEP_SHORT_MS);
}

/**
 * @brief 成功音（一长鸣）
 */
void Buzzer_BeepSuccess(void)
{
    Buzzer_Beep(BUZZER_BEEP_LONG_MS);
}
