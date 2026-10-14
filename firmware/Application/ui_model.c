#include "ui_model.h"
#include "control_config.h"
#include "motor_control.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *field_names[] = { "MANUAL RPM", "MAX RPM", "STOP DELAY S",
    "PID KP", "PID KI", "PID KD", "GAS LOW V", "GAS HIGH V" };
#define UI_FIELD_COUNT 8U

/* 所有编辑先落在候选配置中，非法边界不会污染可提交的草稿。 */
static bool adjust_field(UI_Model_t *ui, int32_t delta)
{
    SystemConfig_t next = ui->draft;
    int64_t change = delta;
    if (change > 100) change = 100;
    if (change < -100) change = -100;
    int64_t value;
    switch (ui->field) {
        case 0:
            value = (int64_t)next.manual_rpm + change * CONTROL_RPM_STEP;
            if (value < CONTROL_TARGET_RPM_MIN) value = CONTROL_TARGET_RPM_MIN;
            if (value > next.max_rpm) value = next.max_rpm;
            next.manual_rpm = (uint32_t)value;
            break;
        case 1:
            value = (int64_t)next.max_rpm + change * CONTROL_RPM_STEP;
            if (value < CONTROL_TARGET_RPM_MIN) value = CONTROL_TARGET_RPM_MIN;
            if (value > CONTROL_TARGET_RPM_MAX) value = CONTROL_TARGET_RPM_MAX;
            next.max_rpm = (uint32_t)value;
            if (next.manual_rpm > next.max_rpm) next.manual_rpm = next.max_rpm;
            break;
        case 2:
            value = (int64_t)next.shutdown_delay_ms + change * 10000;
            if (value < 0) value = 0;
            if (value > 600000) value = 600000;
            next.shutdown_delay_ms = (uint32_t)value;
            break;
        case 3: next.kp += (float)change * 0.05f; break;
        case 4: next.ki += (float)change * 0.05f; break;
        case 5: next.kd += (float)change * 0.01f; break;
        case 6: next.gas_low_v += (float)change * 0.05f; break;
        case 7: next.gas_high_v += (float)change * 0.05f; break;
        default: return false;
    }
    if (!SystemConfig_Validate(&next)) return false;
    if (memcmp(&next, &ui->draft, sizeof(next)) == 0) return false;
    ui->draft = next;
    ui->dirty = true;
    return true;
}

void UI_Model_Init(UI_Model_t *ui, const SystemConfig_t *config)
{
    memset(ui, 0, sizeof(*ui));
    if (SystemConfig_Validate(config)) ui->draft = *config;
    else SystemConfig_Defaults(&ui->draft);
}

UI_Result_t UI_Model_Action(UI_Model_t *ui, UI_Action_t action, int32_t delta,
                           const SystemConfig_t *current, const System_Status_t *status)
{
    UI_Result_t result = {0};
    result.input.type = UI_INPUT_ACTIVITY;
    if (action == UI_ACTION_STOP || action == UI_ACTION_RESET) {
        result.input.type = action == UI_ACTION_RESET ? UI_INPUT_RESET : UI_INPUT_STOP;
        return result;
    }
    if (status && status->state == SYSTEM_STATE_SLEEP) {
        ui->page = UI_PAGE_MAIN; /* 首次操作只唤醒，放弃未提交的草稿。 */
        ui->dirty = false;
        return result;
    }
    if (status && status->state == SYSTEM_STATE_ERROR) {
        ui->page = UI_PAGE_FAULTS;
        if (action == UI_ACTION_POWER) ui->fault_index = (uint8_t)((ui->fault_index + 1U) % 8U);
        else if (action != UI_ACTION_PAGE) result.feedback = UI_FEEDBACK_WARNING;
        return result;
    }
    if (action == UI_ACTION_PAGE) {
        ui->page = (UI_Page_t)((ui->page + 1U) % UI_PAGE_COUNT);
        ui->dirty = false;
        if (ui->page == UI_PAGE_SETTINGS) {
            if (SystemConfig_Validate(current)) ui->draft = *current;
            ui->field = 0;
            result.input.type = UI_INPUT_STOP; /* 编辑参数前强制停机。 */
        }
        return result;
    }
    if (ui->page == UI_PAGE_SETTINGS) {
        if (action == UI_ACTION_MODE) {
            result.save_config = ui->dirty && SystemConfig_Validate(&ui->draft);
            result.feedback = UI_FEEDBACK_MODE;
            ui->page = UI_PAGE_MAIN;
        } else if (action == UI_ACTION_POWER) {
            ui->field = (uint8_t)((ui->field + 1U) % UI_FIELD_COUNT);
        } else {
            int32_t steps = action == UI_ACTION_UP ? 1 : action == UI_ACTION_DOWN ? -1 : delta;
            if (!adjust_field(ui, steps)) result.feedback = UI_FEEDBACK_WARNING;
        }
        return result;
    }
    if (ui->page == UI_PAGE_FAULTS && action == UI_ACTION_POWER) {
        ui->fault_index = (uint8_t)((ui->fault_index + 1U) % 8U);
        return result;
    }
    if (action == UI_ACTION_MODE) {
        result.input.type = UI_INPUT_MODE;
        result.feedback = UI_FEEDBACK_MODE;
    } else if (action == UI_ACTION_POWER) {
        result.input.type = UI_INPUT_TOGGLE_RUN;
        if (status && status->state != SYSTEM_STATE_MANUAL && status->state != SYSTEM_STATE_AUTO &&
            status->state != SYSTEM_STATE_BOOST) result.feedback = UI_FEEDBACK_WARNING;
    } else {
        result.input.type = UI_INPUT_ADJUST;
        result.input.delta = action == UI_ACTION_UP ? 1 : action == UI_ACTION_DOWN ? -1 : delta;
    }
    return result;
}

static void sensor_value(char *line, const char *label, float value, uint32_t flag,
                         const UI_View_t *view, const char *unit, uint32_t scale)
{
    if ((view->sensor.valid_flags & flag) != flag || !isfinite(value) ||
        (uint32_t)(view->now_ms - view->sensor.timestamp_ms) > CONTROL_SENSOR_STALE_MS) {
        (void)snprintf(line, UI_LINE_SIZE, "%s:--", label);
        return;
    }
    /* 限定工程范围后转换，异常输入不会使浮点到整数转换溢出。 */
    if (value < -1000000.0f || value > 1000000.0f) {
        (void)snprintf(line, UI_LINE_SIZE, "%s:--", label);
        return;
    }
    int32_t fixed = (int32_t)value;
    uint32_t magnitude = fixed < 0 ? (uint32_t)(-fixed) : (uint32_t)fixed;
    char formatted[64];
    if (scale == 1U) (void)snprintf(formatted, sizeof(formatted), "%s:%ld%s", label, (long)fixed, unit);
    else (void)snprintf(formatted, sizeof(formatted), "%s:%s%lu.%0*lu%s", label,
        fixed < 0 ? "-" : "", (unsigned long)(magnitude / scale), scale == 100U ? 2 : 3,
        (unsigned long)(magnitude % scale), unit);
    (void)snprintf(line, UI_LINE_SIZE, "%.21s", formatted);
}

static void render_fault(uint32_t faults, uint8_t index, char lines[UI_LINE_COUNT][UI_LINE_SIZE])
{
    static const char *causes[] = { "MOTOR STALL", "OVERSPEED", "SPEED SIGNAL LOST", "SENSOR NOT READY",
        "OVER TEMPERATURE", "PWM OUTPUT ERROR", "COMMAND INVALID", "CONTROL TIMING" };
    static const char *checks[] = { "CHECK FAN / LOAD", "CHECK SPEED / PWM", "CHECK PA6 / SENSOR", "CHECK WARMUP / WIRES",
        "WAIT FOR COOLING", "CHECK DRIVER / PWM", "CHECK TARGET / QUEUE", "CHECK TASK / LOAD" };
    uint8_t active[8], count = 0;
    for (uint8_t i = 0; i < 8; ++i) if (faults & (1UL << i)) active[count++] = i;
    if (!count) { (void)snprintf(lines[2], UI_LINE_SIZE, "NO ACTIVE FAULT"); return; }
    index = (uint8_t)(index % count);
    (void)snprintf(lines[1], UI_LINE_SIZE, "FAULT %u/%u 0x%02lX", (unsigned)index + 1U,
                   (unsigned)count, (unsigned long)(faults & 0xFFU));
    (void)snprintf(lines[3], UI_LINE_SIZE, "%s", causes[active[index]]);
    (void)snprintf(lines[5], UI_LINE_SIZE, "%s", checks[active[index]]);
    (void)snprintf(lines[7], UI_LINE_SIZE, "RESET WHEN SAFE");
}

void UI_Model_Render(const UI_Model_t *ui, const UI_View_t *v,
                     char lines[UI_LINE_COUNT][UI_LINE_SIZE])
{
    memset(lines, 0, UI_LINE_COUNT * UI_LINE_SIZE);
    if (v->status.state == SYSTEM_STATE_SLEEP) return;
    UI_Page_t page = v->motor_faults ? UI_PAGE_FAULTS : ui->page;
    switch (page) {
        case UI_PAGE_MAIN:
            (void)snprintf(lines[0], UI_LINE_SIZE, "MODE:%s", StateMachine_GetStateName(v->status.state));
            (void)snprintf(lines[1], UI_LINE_SIZE, "SET:%u RPM", (unsigned)(v->status.run_requested ? v->status.target_rpm : v->status.manual_rpm));
            (void)snprintf(lines[2], UI_LINE_SIZE, "RPM:%lu %s", (unsigned long)v->rpm, v->speed_valid ? "OK" : "--");
            (void)snprintf(lines[3], UI_LINE_SIZE, "PWM:%u EN:%u", (unsigned)v->duty, (unsigned)v->motor_enabled);
            if (v->status.state == SYSTEM_STATE_CLEANING)
                (void)snprintf(lines[5], UI_LINE_SIZE, "%s", v->rpm >= CONTROL_RESET_RPM || v->motor_enabled ? "WAIT FOR FAN TO STOP" : "POWER OFF TO CLEAN");
            else if (v->status.shutdown_pending)
                (void)snprintf(lines[5], UI_LINE_SIZE, "STOP IN:%luS", (unsigned long)((v->status.shutdown_remaining_ms + 999U) / 1000U));
            else if (v->status.state == SYSTEM_STATE_BOOST && v->status.run_requested)
                (void)snprintf(lines[5], UI_LINE_SIZE, "BOOST:%luS", (unsigned long)((v->status.boost_remaining_ms + 999U) / 1000U));
            else (void)snprintf(lines[5], UI_LINE_SIZE, "%s", v->status.run_requested ? "RUNNING" : "STOPPED");
            (void)snprintf(lines[7], UI_LINE_SIZE, "CONFIG:%s", v->config_saved ? "SAVED" : "PENDING");
            break;
        case UI_PAGE_SENSORS:
            (void)snprintf(lines[0], UI_LINE_SIZE, "SENSORS");
            sensor_value(lines[2], "GAS", v->sensor.gas_voltage * 1000.0f, SENSOR_VALID_GAS, v, "V", 1000U);
            sensor_value(lines[4], "WIND", v->sensor.wind_speed * 1000.0f, SENSOR_VALID_WIND, v, "M/S", 1000U);
            (void)snprintf(lines[6], UI_LINE_SIZE, "MQ:%s", v->sensor.mq_preheating ? "WARMUP" : v->sensor.mq_uncalibrated ? "UNCALIBRATED" : "CALIBRATED");
            (void)snprintf(lines[7], UI_LINE_SIZE, "VALID:%02lX ERR:%02lX", (unsigned long)(v->sensor.valid_flags & 0xFFU), (unsigned long)(v->sensor.error_flags & 0xFFU));
            break;
        case UI_PAGE_ENVIRONMENT:
            (void)snprintf(lines[0], UI_LINE_SIZE, "ENVIRONMENT");
            sensor_value(lines[2], "TEMP", v->sensor.temperature * 1000.0f, SENSOR_VALID_SHT30, v, "C", 1000U);
            sensor_value(lines[4], "RH", v->sensor.humidity * 100.0f, SENSOR_VALID_SHT30, v, "%", 100U);
            sensor_value(lines[6], "PRESS", v->sensor.pressure, SENSOR_VALID_BMP280, v, "PA", 1U);
            break;
        case UI_PAGE_SETTINGS: {
            (void)snprintf(lines[0], UI_LINE_SIZE, "SETTINGS %u/%u", (unsigned)ui->field + 1U, UI_FIELD_COUNT);
            (void)snprintf(lines[2], UI_LINE_SIZE, "%s", field_names[ui->field % UI_FIELD_COUNT]);
            uint32_t value = 0, fraction = 0;
            switch (ui->field) {
                case 0: value = ui->draft.manual_rpm; break;
                case 1: value = ui->draft.max_rpm; break;
                case 2: value = ui->draft.shutdown_delay_ms / 1000U; break;
                case 3: fraction = (uint32_t)(ui->draft.kp * 1000.0f + 0.5f); break;
                case 4: fraction = (uint32_t)(ui->draft.ki * 1000.0f + 0.5f); break;
                case 5: fraction = (uint32_t)(ui->draft.kd * 1000.0f + 0.5f); break;
                case 6: fraction = (uint32_t)(ui->draft.gas_low_v * 1000.0f + 0.5f); break;
                case 7: fraction = (uint32_t)(ui->draft.gas_high_v * 1000.0f + 0.5f); break;
                default: break;
            }
            if (ui->field >= 3U)
                (void)snprintf(lines[4], UI_LINE_SIZE, "%lu.%03lu", (unsigned long)(fraction / 1000U), (unsigned long)(fraction % 1000U));
            else (void)snprintf(lines[4], UI_LINE_SIZE, "%lu", (unsigned long)value);
            (void)snprintf(lines[6], UI_LINE_SIZE, "%s", ui->dirty ? "DRAFT" : "CURRENT");
            break;
        }
        case UI_PAGE_FAULTS:
            (void)snprintf(lines[0], UI_LINE_SIZE, "FAULTS");
            if (v->motor_faults) render_fault(v->motor_faults, ui->fault_index, lines);
            else if (v->last_fault_valid) {
                if (v->last_fault.reason == FAULT_REASON_MOTOR) render_fault(v->last_fault.detail, ui->fault_index, lines);
                else {
                    static const char *reasons[] = { "UNKNOWN", "MOTOR", "SENSOR ERROR", "TASK HEARTBEAT LOST",
                        "HARDFAULT", "MEMFAULT", "BUSFAULT", "USAGEFAULT", "STACK OVERFLOW",
                        "ALLOCATION FAILED", "STARTUP FAILED", "WATCHDOG RESET", "NMI" };
                    uint32_t reason = v->last_fault.reason;
                    if (reason >= sizeof(reasons) / sizeof(reasons[0])) reason = 0;
                    (void)snprintf(lines[2], UI_LINE_SIZE, "%s", reasons[reason]);
                    (void)snprintf(lines[4], UI_LINE_SIZE, "DETAIL:%08lX", (unsigned long)v->last_fault.detail);
                    if (reason == FAULT_REASON_SENSOR) {
                        const char *device = v->last_fault.detail & SENSOR_ERROR_ADC ? "CHECK ADC / DMA" :
                            v->last_fault.detail & SENSOR_ERROR_SHT30 ? "CHECK SHT30 / I2C1" :
                            v->last_fault.detail & SENSOR_ERROR_BMP280 ? "CHECK BMP280 / I2C1" : "CHECK SENSOR RANGE";
                        (void)snprintf(lines[6], UI_LINE_SIZE, "%s", device);
                    }
                }
                (void)snprintf(lines[0], UI_LINE_SIZE, "LAST FAULT");
            } else (void)snprintf(lines[2], UI_LINE_SIZE, "NO FAULT RECORD");
            break;
        default: break;
    }
}
