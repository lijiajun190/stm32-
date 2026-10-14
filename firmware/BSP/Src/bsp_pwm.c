/**
 * @file    bsp_pwm.c
 * @brief   PWM 板级支持包实现
 */

#include "bsp_pwm.h"
#include "tim.h"

static uint16_t current_duty = 0;
static uint8_t pwm_running = 0;

/**
 * @brief  PWM 初始化
 */
void Bsp_PWM_Init(void)
{
    // PWM 由 CubeMX 初始化（TIM1_CH3N, PB15）
}

/**
 * @brief  启动 PWM 输出
 */
HAL_StatusTypeDef Bsp_PWM_Start(void)
{
    // PB15 使用 TIM1_CH3N，必须启动互补输出通道。
    if (pwm_running) {
        return HAL_OK;
    }
    HAL_StatusTypeDef status = HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
    pwm_running = status == HAL_OK;
    return status;
}

/**
 * @brief  停止 PWM 输出
 */
void Bsp_PWM_Stop(void)
{
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_3);
    pwm_running = 0;
}

/**
 * @brief  设置 PWM 占空比
 * @param  duty: 占空比 (0~1000 对应 0~100%)
 */
void Bsp_PWM_SetDuty(uint16_t duty)
{
    if (duty > BSP_PWM_MAX_DUTY) {
        duty = BSP_PWM_MAX_DUTY;
    }

    current_duty = duty;

    // ARR+1 才是完整周期；CCR=ARR+1 时，整个周期均保持有效输出。
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    uint32_t ccr = ((arr + 1U) * duty) / BSP_PWM_MAX_DUTY;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr);
}

/**
 * @brief  获取当前占空比
 * @return 占空比 (0~1000)
 */
uint16_t Bsp_PWM_GetDuty(void)
{
    return current_duty;
}
