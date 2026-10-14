#include "motor_control.h"
#include "control_config.h"
#include <string.h>
#include <math.h>

void MotorControl_Init(MotorControl_t *control)
{
    PID_Params_t params = { CONTROL_PID_KP, CONTROL_PID_KI, CONTROL_PID_KD,
        CONTROL_PID_INTEGRAL_LIMIT, CONTROL_MIN_RUN_DUTY, CONTROL_MAX_DUTY, 20000.0f };
    memset(control, 0, sizeof(*control));
    PID_Init(&control->pid, &params);
}
MotorControl_Output_t MotorControl_ApplyDutySlew(uint16_t previous, uint16_t requested, uint32_t elapsed_ms)
{
    MotorControl_Output_t output;
    output.requested_duty = output.applied_duty = requested; output.limited = false;
    uint32_t up = (uint32_t)((uint64_t)CONTROL_DUTY_SLEW_UP * elapsed_ms / 50U);
    uint32_t down = (uint32_t)((uint64_t)CONTROL_DUTY_SLEW_DOWN * elapsed_ms / 50U);
    if (requested > previous && (uint32_t)requested - previous > up) {
        output.applied_duty = (uint16_t)(previous + up);
        output.limited = true;
    } else if (previous > requested && (uint32_t)previous - requested > down) {
        output.applied_duty = (uint16_t)(previous - down);
        output.limited = true;
    }
    return output;
}
void MotorControl_LatchFault(MotorControl_t *control, uint32_t faults)
{
    if (!control->faults || control->running) control->stopped_since_ms = control->last_step_ms;
    control->faults |= faults;
    control->running = false;
    control->duty = 0;
    PID_Reset(&control->pid);
}
bool MotorControl_SensorsReady(const SensorData_t *sensor, uint32_t now_ms, bool automatic)
{
    uint32_t required = automatic ? SENSOR_AUTO_REQUIRED : SENSOR_VALID_SHT30;
    return sensor && (uint32_t)(now_ms - sensor->timestamp_ms) <= CONTROL_SENSOR_STALE_MS &&
        (sensor->valid_flags & required) == required &&
        isfinite(sensor->temperature_raw) && isfinite(sensor->temperature) &&
        sensor->temperature_raw >= -40.0f && sensor->temperature_raw <= 125.0f &&
        (!automatic || (isfinite(sensor->gas_voltage) && isfinite(sensor->wind_speed) &&
         sensor->gas_voltage > 0.1f && sensor->gas_voltage < 3.25f &&
         sensor->wind_speed >= 0.0f && sensor->wind_speed <= 30.0f));
}
uint16_t MotorControl_AutoTarget(const SensorData_t *sensor, uint16_t previous)
{
    SystemConfig_t config; SystemConfig_Defaults(&config);
    return MotorControl_AutoTargetConfigured(sensor, previous, &config);
}
uint16_t MotorControl_AutoTargetConfigured(const SensorData_t *sensor, uint16_t previous, const SystemConfig_t *config)
{
    if (!sensor || !SystemConfig_Validate(config)) return 0;
    /* MQ135 的电压趋势作为需求量；风速不足时适度增加目标，温度辅助排风。
       不把未标定电压标成 ppm。压力只显示，不作为自动运行的必要条件。 */
    float demand = (sensor->gas_voltage - config->gas_low_v) / (config->gas_high_v - config->gas_low_v);
    if (sensor->wind_speed < CONTROL_AUTO_MIN_WIND_MPS) demand += CONTROL_AUTO_WIND_BOOST;
    if (sensor->temperature > CONTROL_AUTO_TEMP_BASE_C)
        demand += (sensor->temperature - CONTROL_AUTO_TEMP_BASE_C) / CONTROL_AUTO_TEMP_SCALE_C;
    if (!isfinite(demand)) return 0;
    if (demand < 0.0f) demand = 0.0f;
    if (demand > 1.0f) demand = 1.0f;
    uint16_t desired = CONTROL_TARGET_RPM_MIN + (uint16_t)(demand * (config->max_rpm - CONTROL_TARGET_RPM_MIN));
    if (previous < CONTROL_TARGET_RPM_MIN) previous = CONTROL_TARGET_RPM_MIN;
    if (previous > config->max_rpm) previous = config->max_rpm;
    if (desired > previous && desired - previous > CONTROL_AUTO_SLEW_RPM) desired = previous + CONTROL_AUTO_SLEW_RPM;
    if (desired < previous && previous - desired > CONTROL_AUTO_SLEW_RPM) desired = previous - CONTROL_AUTO_SLEW_RPM;
    return desired;
}
uint16_t MotorControl_Step(MotorControl_t *control, const MotorControl_Input_t *input)
{
    uint32_t dt_ms = control->initialized ? (uint32_t)(input->now_ms - control->last_step_ms) : 50U;
    control->initialized = true;
    control->last_step_ms = input->now_ms;
    bool was_running = control->running;
    uint16_t previous_duty = control->duty;
    if (input->speed_valid && input->rpm > CONTROL_OVERSPEED_RPM)
        MotorControl_LatchFault(control, CONTROL_FAULT_OVERSPEED);
    if (input->sensor && (input->sensor->valid_flags & SENSOR_VALID_SHT30) &&
        input->sensor->temperature_raw >= CONTROL_MAX_TEMPERATURE_C)
        MotorControl_LatchFault(control, CONTROL_FAULT_OVER_TEMP);

    if (!input->target_rpm || control->faults) {
        if (was_running) control->stopped_since_ms = input->now_ms;
        control->running = false;
        control->duty = 0;
        control->feedback_acquired = false;
        control->low_speed_pending = false;
        PID_Reset(&control->pid);
        /* 故障只能在停机、冷却、传感器恢复且转速低时人工复位。
           无反馈时至少等待测速超时+启动宽限，避免惯性旋转立即复位。 */
        bool stopped = input->speed_valid ? input->rpm < CONTROL_RESET_RPM :
            (uint32_t)(input->now_ms - control->stopped_since_ms) >= CONTROL_START_GRACE_MS;
        if (input->reset_fault && !input->target_rpm && stopped &&
            MotorControl_SensorsReady(input->sensor, input->now_ms, input->automatic) &&
            input->sensor->temperature_raw < CONTROL_RESET_TEMPERATURE_C)
            control->faults = 0;
        return 0;
    }
    if (!MotorControl_SensorsReady(input->sensor, input->now_ms, input->automatic))
        MotorControl_LatchFault(control, CONTROL_FAULT_SENSOR);
    if ((uint32_t)(input->now_ms - input->command_timestamp_ms) > CONTROL_COMMAND_STALE_MS)
        MotorControl_LatchFault(control, CONTROL_FAULT_COMMAND);
    if (input->target_rpm < CONTROL_TARGET_RPM_MIN || input->target_rpm > CONTROL_TARGET_RPM_MAX)
        MotorControl_LatchFault(control, CONTROL_FAULT_COMMAND);
    if (dt_ms == 0 || dt_ms > CONTROL_MAX_STEP_MS)
        MotorControl_LatchFault(control, CONTROL_FAULT_TIMING);
    if (control->faults) { control->stopped_since_ms = input->now_ms; return 0; }

    if (!control->running) {
        control->start_ms = input->now_ms;
        control->running = true;
        control->feedback_acquired = false;
        control->low_speed_pending = false;
        PID_Reset(&control->pid);
    }
    if (input->speed_valid) control->feedback_acquired = true;
    if (!input->speed_valid) {
        if (control->feedback_acquired || (uint32_t)(input->now_ms - control->start_ms) >= CONTROL_START_GRACE_MS)
            MotorControl_LatchFault(control, CONTROL_FAULT_FEEDBACK);
        else control->duty = CONTROL_START_DUTY; /* 仅启动阶段允许限时开环取得两次脉冲。 */
    } else if ((uint32_t)(input->now_ms - control->start_ms) >= CONTROL_START_GRACE_MS && input->rpm < CONTROL_STALL_RPM) {
        if (!control->low_speed_pending) { control->low_speed_pending = true; control->low_speed_since_ms = input->now_ms; }
        if ((uint32_t)(input->now_ms - control->low_speed_since_ms) >= CONTROL_STALL_TIMEOUT_MS)
            MotorControl_LatchFault(control, CONTROL_FAULT_STALL);
    } else control->low_speed_pending = false;

    if (!control->faults && input->speed_valid) {
        PID_SetSetpoint(&control->pid, (float)input->target_rpm);
        control->duty = (uint16_t)PID_Update(&control->pid, (float)input->rpm, (float)dt_ms / 1000.0f);
    }
    /* 启动也按斜率爬升；急停/故障已在上方清零，不通过此慢降路径。 */
    if (!control->faults)
        control->duty = MotorControl_ApplyDutySlew(previous_duty, control->duty, dt_ms).applied_duty;
    if (control->faults) control->stopped_since_ms = input->now_ms;
    return control->duty;
}
