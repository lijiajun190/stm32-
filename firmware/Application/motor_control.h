/** 不依赖 HAL/RTOS 的控制和故障锁存，任务只提供快照并执行返回的占空比。 */
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H
#include "pid.h"
#include "sensor_data.h"
#include "system_config.h"
#define CONTROL_FAULT_STALL        (1UL << 0)
#define CONTROL_FAULT_OVERSPEED    (1UL << 1)
#define CONTROL_FAULT_FEEDBACK     (1UL << 2)
#define CONTROL_FAULT_SENSOR       (1UL << 3)
#define CONTROL_FAULT_OVER_TEMP    (1UL << 4)
#define CONTROL_FAULT_OUTPUT       (1UL << 5)
#define CONTROL_FAULT_COMMAND      (1UL << 6)
#define CONTROL_FAULT_TIMING       (1UL << 7)
typedef struct {
    uint32_t now_ms, rpm;
    bool speed_valid;
    const SensorData_t *sensor;
    bool automatic;
    uint16_t target_rpm; /* 0 停机，非零表示显式启动请求。 */
    uint32_t command_timestamp_ms;
    bool reset_fault;
} MotorControl_Input_t;
typedef struct {
    PID_Controller_t pid;
    uint32_t faults, start_ms, low_speed_since_ms, last_step_ms, stopped_since_ms;
    bool running, feedback_acquired, low_speed_pending, initialized;
    uint16_t duty;
} MotorControl_t;
typedef struct {
    uint16_t requested_duty;
    uint16_t applied_duty;
    bool limited;
} MotorControl_Output_t;
void MotorControl_Init(MotorControl_t *control);
uint16_t MotorControl_Step(MotorControl_t *control, const MotorControl_Input_t *input);
/* 将 PID 请求限制为每周期最大变化量；停机路径仍由 Step 返回 0。 */
MotorControl_Output_t MotorControl_ApplyDutySlew(uint16_t previous, uint16_t requested, uint32_t elapsed_ms);
void MotorControl_LatchFault(MotorControl_t *control, uint32_t faults);
bool MotorControl_SensorsReady(const SensorData_t *sensor, uint32_t now_ms, bool automatic);
uint16_t MotorControl_AutoTarget(const SensorData_t *sensor, uint16_t previous);
uint16_t MotorControl_AutoTargetConfigured(const SensorData_t *sensor, uint16_t previous, const SystemConfig_t *config);
#endif
