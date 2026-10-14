#include "runtime_monitor.h"
#include <string.h>
#include <limits.h>
void RuntimeMonitor_Init(RuntimeMonitor_t *monitor)
{
    memset(monitor, 0, sizeof(*monitor)); monitor->active = -1;
    for (unsigned i = 0; i < HEARTBEAT_COUNT; ++i) monitor->tasks[i].period_min_us = UINT_MAX;
}
void RuntimeMonitor_Register(RuntimeMonitor_t *monitor, unsigned id, void *handle)
{ if (id < HEARTBEAT_COUNT) monitor->handles[id] = handle; }
void RuntimeMonitor_Account(RuntimeMonitor_t *monitor, uint32_t now_us)
{
    if (monitor->active >= 0) monitor->tasks[monitor->active].cpu_us += (uint32_t)(now_us - monitor->switched_us);
    monitor->switched_us = now_us;
}
void RuntimeMonitor_Switch(RuntimeMonitor_t *monitor, void *handle, uint32_t now_us)
{
    RuntimeMonitor_Account(monitor, now_us); monitor->active = -1;
    for (unsigned i = 0; i < HEARTBEAT_COUNT; ++i)
        if (handle && monitor->handles[i] == handle) { monitor->active = (int)i; break; }
}
void RuntimeMonitor_Begin(RuntimeMonitor_t *monitor, unsigned id, uint32_t now_us, uint32_t period_us)
{
    if (id >= HEARTBEAT_COUNT) return;
    Runtime_Task_t *task = &monitor->tasks[id];
    if (task->started) {
        uint32_t elapsed = now_us - task->last_start_us;
        uint32_t jitter = elapsed > period_us ? elapsed - period_us : period_us - elapsed;
        if (elapsed < task->period_min_us) task->period_min_us = elapsed;
        if (elapsed > task->period_max_us) task->period_max_us = elapsed;
        if (jitter > task->jitter_max_us) task->jitter_max_us = jitter;
        task->jitter_sum_us += jitter;
        ++task->samples;
    }
    task->started = true; task->last_start_us = task->body_start_us = now_us;
}
void RuntimeMonitor_End(RuntimeMonitor_t *monitor, unsigned id, uint32_t now_us, uint32_t period_us)
{
    if (id >= HEARTBEAT_COUNT || !monitor->tasks[id].started) return;
    Runtime_Task_t *task = &monitor->tasks[id]; uint32_t elapsed = now_us - task->body_start_us;
    if (elapsed > task->execution_max_us) task->execution_max_us = elapsed;
    task->execution_sum_us += elapsed;
    if (elapsed > period_us) ++task->overruns;
}
