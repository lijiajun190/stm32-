/**
 * @file    motor.c
 * @brief   电机控制驱动实现
 */

#include "motor.h"
#include "bsp_pwm.h"
#include "bsp_gpio.h"

static Motor_Info_t motor_info = {0};

/**
 * @brief  初始化电机
 */
void Motor_Init(void)
{
    motor_info.duty = 0;
    motor_info.rpm = 0;
    motor_info.speed_valid = false;
    motor_info.state = MOTOR_STATE_IDLE;
    motor_info.enabled = false;

    Bsp_PWM_Init();
    Bsp_Motor_Enable(false);
    Bsp_Motor_SetDirection(true);
    Bsp_PWM_Stop();
    Bsp_PWM_SetDuty(0);
}

/**
 * @brief  使能电机
 */
void Motor_Enable(bool enable)
{
    if (!enable) {
        Motor_Stop();
    } else {
        /* 先进入允许设定占空比的状态，真正使能在 PWM 就绪后执行。 */
        motor_info.enabled = true;
    }
}

/**
 * @brief  设置电机占空比
 * @param  duty: 占空比 (0~1000)
 */
void Motor_SetDuty(uint16_t duty)
{
    if (!motor_info.enabled) {
        return;
    }

    if (duty > BSP_PWM_MAX_DUTY) {
        duty = BSP_PWM_MAX_DUTY;
    }

    motor_info.duty = duty;
    Bsp_PWM_SetDuty(duty);

    if (duty > 0) {
        if (Bsp_PWM_Start() != HAL_OK) {
            Motor_Stop();
            motor_info.state = MOTOR_STATE_ERROR;
            return;
        }
        Bsp_Motor_Enable(true);
        motor_info.state = MOTOR_STATE_RUNNING;
    } else {
        Motor_Stop();
    }
}

/**
 * @brief  获取电机占空比
 */
uint16_t Motor_GetDuty(void)
{
    return motor_info.duty;
}

/**
 * @brief  停止电机
 */
void Motor_Stop(void)
{
    /* 先禁止功率输出，再停止 PWM，避免关闭互补通道时的电平扰动。 */
    Bsp_Motor_Enable(false);
    Bsp_PWM_Stop();
    Bsp_PWM_SetDuty(0);
    motor_info.duty = 0;
    motor_info.enabled = false;
    motor_info.state = MOTOR_STATE_IDLE;
}

/**
 * @brief  获取电机信息
 */
Motor_Info_t Motor_GetInfo(void)
{
    return motor_info;
}

/**
 * @brief  更新电机转速（由测速模块调用）
 */
void Motor_UpdateSpeed(uint32_t rpm, bool valid)
{
    motor_info.rpm = valid ? rpm : 0U;
    motor_info.speed_valid = valid;
}
