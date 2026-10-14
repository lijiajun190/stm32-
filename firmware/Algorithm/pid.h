/**
 * @file    pid.h
 * @brief   PID 控制器
 */

#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <stdbool.h>

/* PID 参数 */
typedef struct {
    float kp;               // 比例系数
    float ki;               // 积分系数
    float kd;               // 微分系数
    float integral_limit;   // 积分限幅
    float output_min;       // 输出下限
    float output_max;       // 输出上限
    float derivative_limit; // 微分限幅
} PID_Params_t;

/* PID 状态 */
typedef struct {
    float setpoint;         // 目标值
    float measurement;      // 测量值
    float error;            // 误差
    float last_error;       // 上次误差
    float integral;         // 积分项
    float derivative;       // 微分项
    float output;           // 输出值
    uint32_t last_time;     // 上次计算时间
    bool enabled;           // 使能标志
    bool derivative_initialized;
} PID_State_t;

/* PID 控制器 */
typedef struct {
    PID_Params_t params;
    PID_State_t state;
} PID_Controller_t;

/* 函数声明 */
void PID_Init(PID_Controller_t *pid, const PID_Params_t *params);
void PID_Reset(PID_Controller_t *pid);
void PID_SetParams(PID_Controller_t *pid, const PID_Params_t *params);
void PID_SetSetpoint(PID_Controller_t *pid, float setpoint);
float PID_Update(PID_Controller_t *pid, float measurement, float dt);
void PID_Enable(PID_Controller_t *pid, bool enable);

#endif /* PID_H */
