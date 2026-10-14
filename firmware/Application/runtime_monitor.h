/** 周期统计与调度 CPU 记账；调用方在短临界区内操作此结构。 */
#ifndef RUNTIME_MONITOR_H
#define RUNTIME_MONITOR_H
#include "heartbeat.h"
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint32_t samples, period_min_us, period_max_us, jitter_max_us;
    uint32_t execution_max_us, overruns, last_start_us, body_start_us;
    uint64_t jitter_sum_us, execution_sum_us, cpu_us;
    bool started;
} Runtime_Task_t;
typedef struct {
    Runtime_Task_t tasks[HEARTBEAT_COUNT];
    void *handles[HEARTBEAT_COUNT];
    uint32_t switched_us;
    int active;
} RuntimeMonitor_t;
void RuntimeMonitor_Init(RuntimeMonitor_t *monitor);
void RuntimeMonitor_Register(RuntimeMonitor_t *monitor, unsigned id, void *handle);
void RuntimeMonitor_Switch(RuntimeMonitor_t *monitor, void *handle, uint32_t now_us);
void RuntimeMonitor_Account(RuntimeMonitor_t *monitor, uint32_t now_us);
void RuntimeMonitor_Begin(RuntimeMonitor_t *monitor, unsigned id, uint32_t now_us, uint32_t period_us);
void RuntimeMonitor_End(RuntimeMonitor_t *monitor, unsigned id, uint32_t now_us, uint32_t period_us);
/* FreeRTOS hook 的声明放在此处，配置头只声明，无须依赖应用头。 */
void Tasks_MonitorSwitch(void *handle);
#endif
