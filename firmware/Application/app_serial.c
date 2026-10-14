/** 协议由 Logger 单一上下文处理；控制/配置通过原有队列提交。 */
#include "app_serial.h"
#include "serial_protocol.h"
#include "system_persist.h"
#include "bsp_uart.h"
#include "boot_layout.h"
#include <string.h>
static ProtocolParser_t parser;
static ProtocolFrame_t request, reply;
static Tasks_MonitorSnapshot_t monitor;
static void put_float(uint8_t *p, float value)
{ uint32_t bits; memcpy(&bits, &value, 4); Protocol_Put32(p, bits); }
static float get_float(const uint8_t *p)
{ uint32_t bits = Protocol_Get32(p); float value; memcpy(&value, &bits, 4); return value; }
static void config_encode(uint8_t *p, const SystemConfig_t *c)
{
    Protocol_Put32(p, c->manual_rpm); Protocol_Put32(p + 4, c->max_rpm);
    put_float(p + 8, c->kp); put_float(p + 12, c->ki); put_float(p + 16, c->kd);
    put_float(p + 20, c->gas_low_v); put_float(p + 24, c->gas_high_v);
    Protocol_Put32(p + 28, c->shutdown_delay_ms);
}
static void config_decode(const uint8_t *p, SystemConfig_t *c)
{
    memset(c, 0, sizeof(*c)); c->manual_rpm = Protocol_Get32(p); c->max_rpm = Protocol_Get32(p + 4);
    c->kp = get_float(p + 8); c->ki = get_float(p + 12); c->kd = get_float(p + 16);
    c->gas_low_v = get_float(p + 20); c->gas_high_v = get_float(p + 24);
    c->shutdown_delay_ms = Protocol_Get32(p + 28);
}
static bool handle(const System_Status_t *status, const Motor_Telemetry_t *motor, const SensorData_t *sensor)
{
    bool reset = false;
    reply.command = request.command | 0x80U; reply.sequence = request.sequence;
    reply.length = 1; reply.payload[0] = REPLY_OK;
    uint8_t *p = reply.payload + 1;
    SystemConfig_t config;
    if (request.command != CMD_SET_CONFIG && request.command != CMD_MONITOR && request.length) {
        reply.payload[0] = REPLY_BAD_ARGUMENT; return false;
    }
    switch (request.command) {
        case CMD_INFO:
            Protocol_Put32(p, 1U); /* role: application */
#ifdef APP_UPGRADE_LAYOUT
            Protocol_Put32(p + 4, 1U);
#else
            Protocol_Put32(p + 4, 0U);
#endif
            Protocol_Put32(p + 8, BOOT_APP_BASE); Protocol_Put32(p + 12, BOOT_IMAGE_MAX);
            Protocol_Put32(p + 16, 252U); reply.length += 20; break;
        case CMD_STATUS: {
            uint32_t values[] = {status->state, status->target_rpm, motor->speed.rpm, motor->faults,
                motor->motor.duty, status->run_requested, motor->motor.enabled, sensor->valid_flags,
                sensor->error_flags, HAL_GetTick(), Tasks_IsConfigSaved(), status->shutdown_remaining_ms,
                status->boost_remaining_ms, Bsp_UART_RxDropped()};
            for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) Protocol_Put32(p + i * 4U, values[i]);
            put_float(p + 56, sensor->temperature); put_float(p + 60, sensor->humidity);
            put_float(p + 64, sensor->gas_voltage); put_float(p + 68, sensor->wind_speed); put_float(p + 72, sensor->pressure);
            reply.length += 76; break;
        }
        case CMD_GET_CONFIG:
            if (Tasks_GetConfig(&config)) { config_encode(p, &config); reply.length += 32; }
            else reply.payload[0] = REPLY_BUSY;
            break;
        case CMD_SET_CONFIG:
            if (request.length != 32) { reply.payload[0] = REPLY_BAD_ARGUMENT; break; }
            config_decode(request.payload, &config);
            if (!SystemConfig_Validate(&config)) reply.payload[0] = REPLY_BAD_ARGUMENT;
            else if (!Tasks_SetConfig(&config)) reply.payload[0] = REPLY_BUSY;
            break;
        case CMD_STOP:
            xEventGroupSetBits(g_system_events, SYSTEM_FLAG_STOP_REQUEST); break;
        case CMD_MONITOR: {
            if (request.length != 1 || request.payload[0] >= HEARTBEAT_COUNT) { reply.payload[0] = REPLY_BAD_ARGUMENT; break; }
            unsigned id = request.payload[0]; Tasks_GetMonitorSnapshot(&monitor);
            Runtime_Task_t *t = &monitor.tasks[id];
            uint32_t values[] = { id, t->samples, t->samples ? t->period_min_us : 0, t->period_max_us,
                t->jitter_max_us, t->execution_max_us, t->overruns, (uint32_t)t->cpu_us, (uint32_t)(t->cpu_us >> 32),
                monitor.stack_free_bytes[id], monitor.heap_free_bytes, monitor.heap_min_bytes,
                (uint32_t)monitor.cpu_total_us, (uint32_t)(monitor.cpu_total_us >> 32),
                t->samples ? (uint32_t)(t->jitter_sum_us / t->samples) : 0 };
            for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) Protocol_Put32(p + i * 4U, values[i]);
            reply.length += sizeof(values); break;
        }
        case CMD_BOOT:
#ifdef APP_UPGRADE_LAYOUT
            if (status->run_requested || motor->motor.enabled || motor->speed.rpm >= CONTROL_RESET_RPM ||
                !Tasks_IsConfigSaved() || (uint32_t)(HAL_GetTick() - motor->timestamp_ms) > CONTROL_COMMAND_STALE_MS)
                reply.payload[0] = REPLY_BUSY;
            else {
                xEventGroupSetBits(g_system_events, SYSTEM_FLAG_BOOT_REQUEST);
                reset = true; /* 从回复到复位期间，SysMgr 和电机任务也保持禁止启动。 */
            }
#else
            reply.payload[0] = REPLY_UNSUPPORTED;
#endif
            break;
        default: reply.payload[0] = REPLY_UNSUPPORTED; break;
    }
    return reset;
}
void AppSerial_Poll(const System_Status_t *status, const Motor_Telemetry_t *motor, const SensorData_t *sensor)
{
    uint8_t bytes[64]; uint16_t count;
    unsigned budget = 1024U;
    while (budget && (count = Bsp_UART_Receive(bytes, sizeof(bytes))) != 0U) {
        budget -= count;
        for (uint16_t i = 0; i < count; ++i) {
            if (!Protocol_Feed(&parser, bytes[i], HAL_GetTick(), &request)) continue;
            bool reset = handle(status, motor, sensor);
            uint8_t encoded[PROTOCOL_FRAME_MAX];
            uint16_t length = Protocol_Encode(&reply, encoded, sizeof(encoded));
            if (Bsp_UART_SendBlocking(encoded, length, 100U) && reset) {
#ifdef APP_UPGRADE_LAYOUT
                /* 只在确认回复已发出且电机停止后设置备份域请求。 */
                __disable_irq(); Motor_Enable(false); __HAL_RCC_PWR_CLK_ENABLE(); HAL_PWR_EnableBkUpAccess();
                __HAL_RCC_RTC_ENABLE(); RTC->BKP1R = BOOT_REQUEST_MAGIC; __DSB(); NVIC_SystemReset();
#endif
            }
        }
    }
}
