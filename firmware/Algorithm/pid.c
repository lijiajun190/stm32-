/**
 * @file    pid.c
 * @brief   PID 控制器实现
 */

#include "pid.h"
#include <string.h>
#include <math.h>

/**
 * @brief  限幅函数
 */
static float clamp(float value, float min, float max)
{
    if (min > max) { float t = min; min = max; max = t; }
    if (!isfinite(value)) return min;
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
static float minf_local(float a, float b) { return a < b ? a : b; }
static float maxf_local(float a, float b) { return a > b ? a : b; }

/**
 * @brief  初始化 PID 控制器
 */
void PID_Init(PID_Controller_t *pid, const PID_Params_t *params)
{
    if (!pid) return;
    memset(pid, 0, sizeof(PID_Controller_t));
    if (params) {
        memcpy(&pid->params, params, sizeof(PID_Params_t));
    }
    pid->state.enabled = true;
}

/**
 * @brief  重置 PID 状态
 */
void PID_Reset(PID_Controller_t *pid)
{
    if (!pid) return;
    pid->state.error = 0;
    pid->state.last_error = 0;
    pid->state.integral = 0;
    pid->state.derivative = 0;
    pid->state.output = 0;
    pid->state.derivative_initialized = false;
}

/**
 * @brief  设置 PID 参数
 */
void PID_SetParams(PID_Controller_t *pid, const PID_Params_t *params)
{
    if (!pid || !params) return;
    memcpy(&pid->params, params, sizeof(PID_Params_t));
}

/**
 * @brief  设置目标值
 */
void PID_SetSetpoint(PID_Controller_t *pid, float setpoint)
{
    if (!pid) return;
    pid->state.setpoint = setpoint;
}

/**
 * @brief  更新 PID 计算
 * @param  pid: PID 控制器
 * @param  measurement: 测量值
 * @param  dt: 采样周期 (s)
 * @return 控制输出
 */
float PID_Update(PID_Controller_t *pid, float measurement, float dt)
{
    if (!pid) return 0;
    if (!pid->state.enabled || !isfinite(measurement) || !isfinite(pid->state.setpoint) ||
        !isfinite(dt) || dt <= 0.0f || dt > 2.0f ||
        !isfinite(pid->params.kp) || !isfinite(pid->params.ki) || !isfinite(pid->params.kd) ||
        !isfinite(pid->params.output_min) || !isfinite(pid->params.output_max) ||
        !isfinite(pid->params.integral_limit) || !isfinite(pid->params.derivative_limit) ||
        pid->params.output_min > pid->params.output_max || pid->params.integral_limit < 0 ||
        pid->params.derivative_limit < 0) {
        PID_Reset(pid);
        return 0;
    }

    // 保存测量值
    pid->state.measurement = measurement;

    // 计算误差
    pid->state.error = pid->state.setpoint - measurement;

    // 比例项
    float p_term = pid->params.kp * pid->state.error;

    // 积分项（带抗饱和）
    float candidate_integral = pid->state.integral + pid->state.error * dt;
    candidate_integral = clamp(candidate_integral, -fabsf(pid->params.integral_limit), fabsf(pid->params.integral_limit));
    // 微分项：首帧抑制突变，并采用 0.2 的一阶低通。
    float derivative = pid->state.derivative_initialized ? (pid->state.error - pid->state.last_error) / dt : 0.0f;
    pid->state.derivative_initialized = true;
    derivative = clamp(derivative,
                      -pid->params.derivative_limit,
                      pid->params.derivative_limit);
    pid->state.derivative += 0.2f * (derivative - pid->state.derivative);
    float d_term = pid->params.kd * pid->state.derivative;

    // 计算输出
    pid->state.output = p_term + (pid->params.ki * candidate_integral) + d_term;
    pid->state.output = clamp(pid->state.output,
                             minf_local(pid->params.output_min, pid->params.output_max),
                             maxf_local(pid->params.output_min, pid->params.output_max));

    /* 输出饱和且误差继续推动饱和时冻结积分，避免松开目标后长时间过冲。 */
    bool pushing_high = pid->state.output >= maxf_local(pid->params.output_min, pid->params.output_max) && pid->state.error > 0.0f;
    bool pushing_low = pid->state.output <= minf_local(pid->params.output_min, pid->params.output_max) && pid->state.error < 0.0f;
    if (!pushing_high && !pushing_low) pid->state.integral = candidate_integral;

    // 保存误差
    pid->state.last_error = pid->state.error;

    return pid->state.output;
}

/**
 * @brief  使能/禁用 PID
 */
void PID_Enable(PID_Controller_t *pid, bool enable)
{
    if (!pid) return;
    pid->state.enabled = enable;
    if (!enable) {
        PID_Reset(pid);
    }
}
