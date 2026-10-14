/** 可持久化的配置值；状态/启动请求不保存，上电始终待机。 */
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H
#include <stdint.h>
#include <stdbool.h>
#include "pid.h"
typedef struct {
    uint32_t manual_rpm;
    uint32_t max_rpm;
    float kp, ki, kd;
    float gas_low_v, gas_high_v;
    uint32_t shutdown_delay_ms; /* 普通启停键的余烟排风时间，0 为立即停机。 */
} SystemConfig_t;
void SystemConfig_Defaults(SystemConfig_t *config);
bool SystemConfig_Validate(const SystemConfig_t *config);
/* 只有启动/系统管理上下文应用值，各任务用快照传递，不访问该私有副本。 */
#endif
