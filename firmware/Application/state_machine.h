/**
 * @file    state_machine.h
 * @brief   系统状态机
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_data.h"
#include "system_config.h"

/* 系统状态 */
typedef enum {
    SYSTEM_STATE_INIT = 0,
    SYSTEM_STATE_STANDBY,
    SYSTEM_STATE_MANUAL,
    SYSTEM_STATE_AUTO,
    SYSTEM_STATE_BOOST,
    SYSTEM_STATE_CLEANING,
    SYSTEM_STATE_ERROR,
    SYSTEM_STATE_SLEEP
} System_State_t;

/* 系统事件 */
typedef enum {
    EVENT_POWER_ON = 0,
    EVENT_KEY_MODE,
    EVENT_KEY_UP,
    EVENT_KEY_DOWN,
    EVENT_ENCODER_CHANGED,
    EVENT_SENSOR_ERROR,
    EVENT_MOTOR_STALL,
    EVENT_OVER_TEMPERATURE,
    EVENT_TIMEOUT,
    EVENT_STOP,
    EVENT_MOTOR_OVERSPEED,
    EVENT_MAX
} System_Event_t;

typedef enum { UI_INPUT_NONE, UI_INPUT_MODE, UI_INPUT_ADJUST, UI_INPUT_TOGGLE_RUN,
               UI_INPUT_STOP, UI_INPUT_RESET, UI_INPUT_ACTIVITY } UI_InputType_t;
typedef struct { UI_InputType_t type; int32_t delta; } UI_Event_t;
typedef struct {
    System_State_t state;
    uint16_t target_rpm, manual_rpm;
    uint32_t faults;
    bool run_requested, reset_pending;
    bool shutdown_pending, automatic_required;
    uint32_t shutdown_remaining_ms, boost_remaining_ms;
} System_Status_t;
/* 只有系统管理任务调用以下写接口，UI/日志读取队列快照。 */
void StateMachine_HandleInput(const UI_Event_t *input);
/* 正式任务传入事件处理时刻；原接口保留给已有测试和调用方。 */
void StateMachine_HandleInputAt(const UI_Event_t *input, uint32_t now_ms);
void StateMachine_Update(const SensorData_t *sensor, uint32_t now_ms, uint32_t motor_faults);
System_Status_t StateMachine_GetStatus(void);
void StateMachine_ApplyConfig(const SystemConfig_t *config);

/* 函数声明 */
void StateMachine_Init(void);
void StateMachine_ProcessEvent(System_Event_t event);
System_State_t StateMachine_GetState(void);
const char* StateMachine_GetStateName(System_State_t state);

#endif /* STATE_MACHINE_H */
