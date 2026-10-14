/**
 * @file    motor.h
 * @brief   电机控制驱动
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

/* 电机状态 */
typedef enum {
    MOTOR_STATE_IDLE = 0,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_STALL,
    MOTOR_STATE_OVERSPEED,
    MOTOR_STATE_ERROR
} Motor_State_t;

/* 电机参数 */
typedef struct {
    uint16_t duty;          // 占空比 (0~1000)
    uint32_t rpm;           // 实际转速 (RPM)，避免异常高频脉冲截断为 16 位
    bool speed_valid;       // false 表示未测得完整周期或无脉冲超时
    Motor_State_t state;    // 状态
    bool enabled;           // 使能标志
} Motor_Info_t;

/* 函数声明 */
void Motor_Init(void);
void Motor_Enable(bool enable);
void Motor_SetDuty(uint16_t duty);
uint16_t Motor_GetDuty(void);
void Motor_Stop(void);
Motor_Info_t Motor_GetInfo(void);
void Motor_UpdateSpeed(uint32_t rpm, bool valid);

#endif /* MOTOR_H */
