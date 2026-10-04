/**
 * @file    state_machine.c
 * @brief   系统状态机实现
 */

#include "state_machine.h"
#include "control_config.h"
#include "motor_control.h"

static System_State_t current_state = SYSTEM_STATE_INIT;
static System_Status_t status;
static bool mode_changed;
static SystemConfig_t configuration;
static uint32_t clock_ms, last_activity_ms, run_start_ms, shutdown_start_ms;
static uint16_t shutdown_rpm;

static void stop_running(void)
{
    status.run_requested = false;
    status.target_rpm = 0;
    status.shutdown_pending = false;
    status.shutdown_remaining_ms = status.boost_remaining_ms = 0;
}

/* 状态名称字符串 */
static const char* state_names[] = {
    "INIT",
    "STANDBY",
    "MANUAL",
    "AUTO",
    "BOOST",
    "CLEANING",
    "ERROR",
    "SLEEP"
};

/**
 * @brief  初始化状态机
 */
void StateMachine_Init(void)
{
    current_state = SYSTEM_STATE_INIT;
    status = (System_Status_t){0};
    status.manual_rpm = CONTROL_DEFAULT_RPM;
    mode_changed = false;
    clock_ms = last_activity_ms = run_start_ms = shutdown_start_ms = 0;
    SystemConfig_Defaults(&configuration);
}

/**
 * @brief  状态转换
 */
static void transition_to(System_State_t new_state)
{
    if (current_state != new_state) {
        current_state = new_state;
        status.state = new_state;
        last_activity_ms = clock_ms;
    }
}

/**
 * @brief  处理事件
 */
void StateMachine_ProcessEvent(System_Event_t event)
{
    if (event == EVENT_STOP) {
        stop_running();
        mode_changed = true;
        return;
    }
    if (event == EVENT_SENSOR_ERROR || event == EVENT_MOTOR_STALL ||
        event == EVENT_OVER_TEMPERATURE || event == EVENT_MOTOR_OVERSPEED) {
        transition_to(SYSTEM_STATE_ERROR);
        stop_running();
        return;
    }
    if (event == EVENT_KEY_MODE && current_state != SYSTEM_STATE_ERROR) {
        stop_running();
        status.automatic_required = false;
        mode_changed = true;
    }
    switch (current_state) {
        case SYSTEM_STATE_INIT:
            if (event == EVENT_POWER_ON) {
                transition_to(SYSTEM_STATE_STANDBY);
            }
            break;

        case SYSTEM_STATE_STANDBY:
            if (event == EVENT_KEY_MODE) {
                transition_to(SYSTEM_STATE_MANUAL);
            }
            break;

        case SYSTEM_STATE_MANUAL:
            if (event == EVENT_KEY_MODE) {
                transition_to(SYSTEM_STATE_AUTO);
            }
            break;

        case SYSTEM_STATE_AUTO:
            if (event == EVENT_KEY_MODE) {
                transition_to(SYSTEM_STATE_BOOST);
            }
            break;

        case SYSTEM_STATE_BOOST:
            if (event == EVENT_KEY_MODE) transition_to(SYSTEM_STATE_CLEANING);
            break;
        case SYSTEM_STATE_CLEANING:
            if (event == EVENT_KEY_MODE) transition_to(SYSTEM_STATE_SLEEP);
            break;
        case SYSTEM_STATE_SLEEP:
            if (event == EVENT_KEY_MODE) transition_to(SYSTEM_STATE_STANDBY);
            break;

        case SYSTEM_STATE_ERROR:
            // 错误状态需要复位或排除故障后才能退出
            break;

        default:
            break;
    }
}

/**
 * @brief  获取当前状态
 */
System_State_t StateMachine_GetState(void)
{
    return current_state;
}

/**
 * @brief  获取状态名称
 */
const char* StateMachine_GetStateName(System_State_t state)
{
    if ((unsigned)state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "UNKNOWN";
}

void StateMachine_HandleInput(const UI_Event_t *input)
{
    StateMachine_HandleInputAt(input, clock_ms);
}

void StateMachine_HandleInputAt(const UI_Event_t *input, uint32_t now_ms)
{
    if (!input || input->type == UI_INPUT_NONE) return;
    clock_ms = last_activity_ms = now_ms;
    if (input->type == UI_INPUT_STOP) { StateMachine_ProcessEvent(EVENT_STOP); return; }
    if (current_state == SYSTEM_STATE_ERROR) {
        if (input->type == UI_INPUT_RESET) status.reset_pending = true;
        return;
    }
    /* 唤醒动作只回待机，不把同一次按键解释为启动或模式选择。 */
    if (current_state == SYSTEM_STATE_SLEEP) {
        stop_running();
        transition_to(SYSTEM_STATE_STANDBY);
        mode_changed = true;
        return;
    }
    if (input->type == UI_INPUT_RESET) { StateMachine_ProcessEvent(EVENT_STOP); return; }
    if (input->type == UI_INPUT_MODE) {
        mode_changed = true;
        status.run_requested = false; /* 切换模式必先停机，启动必须再次显式确认。 */
        status.target_rpm = 0;
        StateMachine_ProcessEvent(EVENT_KEY_MODE);
    } else if (input->type == UI_INPUT_TOGGLE_RUN &&
               (current_state == SYSTEM_STATE_MANUAL || current_state == SYSTEM_STATE_AUTO ||
                current_state == SYSTEM_STATE_BOOST)) {
        if (mode_changed) return; /* 同一批输入不能覆盖模式切换产生的停机。 */
        if (status.shutdown_pending) {
            status.shutdown_pending = false; /* 再按一次取消余烟排风倒计时。 */
            status.shutdown_remaining_ms = 0;
        } else if (status.run_requested && configuration.shutdown_delay_ms) {
            status.shutdown_pending = true;
            shutdown_start_ms = now_ms;
            status.shutdown_remaining_ms = configuration.shutdown_delay_ms;
            shutdown_rpm = status.target_rpm;
            if (shutdown_rpm < CONTROL_TARGET_RPM_MIN) shutdown_rpm = CONTROL_TARGET_RPM_MIN;
        } else {
            status.run_requested = !status.run_requested;
            if (!status.run_requested) status.target_rpm = 0;
            run_start_ms = now_ms;
            if (status.run_requested) status.automatic_required = current_state == SYSTEM_STATE_AUTO;
        }
    } else if (input->type == UI_INPUT_ADJUST && current_state == SYSTEM_STATE_MANUAL) {
        /* 编码器累计计数的每四个边沿对应一档；计算提升为 64 位防溢出。 */
        int64_t next = (int64_t)status.manual_rpm + (int64_t)input->delta * CONTROL_RPM_STEP;
        if (next < CONTROL_TARGET_RPM_MIN) next = CONTROL_TARGET_RPM_MIN;
        if (next > configuration.max_rpm) next = configuration.max_rpm;
        status.manual_rpm = (uint16_t)next;
    }
    if (!status.run_requested) status.target_rpm = 0;
}

void StateMachine_Update(const SensorData_t *sensor, uint32_t now_ms, uint32_t motor_faults)
{
    clock_ms = now_ms;
    mode_changed = false; /* 管理任务一轮输入结束，下一轮才允许重新启动。 */
    status.faults = motor_faults;
    if (motor_faults) {
        transition_to(SYSTEM_STATE_ERROR);
        stop_running();
        return;
    }
    if (current_state == SYSTEM_STATE_ERROR) {
        /* 电机任务确认安全条件并清除锁存后，才回待机；不自动重新启动。 */
        if (status.reset_pending && motor_faults == 0U) {
            status.reset_pending = false;
            status.automatic_required = false;
            transition_to(SYSTEM_STATE_STANDBY);
        }
        return;
    }
    if (current_state == SYSTEM_STATE_STANDBY &&
        (uint32_t)(now_ms - last_activity_ms) >= CONTROL_STANDBY_SLEEP_MS)
        transition_to(SYSTEM_STATE_SLEEP);
    if (!status.run_requested) { stop_running(); return; }
    if (current_state == SYSTEM_STATE_BOOST) {
        uint32_t elapsed = now_ms - run_start_ms;
        if (elapsed >= CONTROL_BOOST_DURATION_MS) {
            stop_running();
            transition_to(SYSTEM_STATE_STANDBY);
            return;
        }
        status.boost_remaining_ms = CONTROL_BOOST_DURATION_MS - elapsed;
    }
    if (status.shutdown_pending) {
        uint32_t elapsed = now_ms - shutdown_start_ms;
        if (elapsed >= configuration.shutdown_delay_ms) {
            stop_running();
            transition_to(SYSTEM_STATE_STANDBY);
        } else {
            status.shutdown_remaining_ms = configuration.shutdown_delay_ms - elapsed;
            status.target_rpm = shutdown_rpm;
        }
        return;
    }
    if (current_state == SYSTEM_STATE_MANUAL) status.target_rpm = status.manual_rpm;
    else if (current_state == SYSTEM_STATE_BOOST) status.target_rpm = (uint16_t)configuration.max_rpm;
    else if (current_state == SYSTEM_STATE_AUTO) {
        if (MotorControl_SensorsReady(sensor, now_ms, true))
            status.target_rpm = MotorControl_AutoTargetConfigured(sensor, status.target_rpm, &configuration);
        /* 无效时保留请求，让电机任务识别并锁存 SENSOR 故障，绝不升速。 */
        else if (!status.target_rpm) status.target_rpm = CONTROL_TARGET_RPM_MIN;
    }
}

System_Status_t StateMachine_GetStatus(void) { return status; }

void StateMachine_ApplyConfig(const SystemConfig_t *config)
{
    if (!SystemConfig_Validate(config)) return;
    configuration = *config;
    status.manual_rpm = (uint16_t)config->manual_rpm;
    /* 应用配置不恢复运行意图；新的 PID 参数下一次显式启动才生效。 */
    stop_running();
    mode_changed = true;
}
