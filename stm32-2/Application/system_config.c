#include "system_config.h"
#include "control_config.h"
#include <math.h>
void SystemConfig_Defaults(SystemConfig_t *config)
{
    *config = (SystemConfig_t){0};
    config->manual_rpm = CONTROL_DEFAULT_RPM;
    config->max_rpm = CONTROL_TARGET_RPM_MAX;
    config->kp = CONTROL_PID_KP; config->ki = CONTROL_PID_KI; config->kd = CONTROL_PID_KD;
    config->gas_low_v = CONTROL_GAS_LOW_V; config->gas_high_v = CONTROL_GAS_HIGH_V;
    config->shutdown_delay_ms = CONTROL_SHUTDOWN_DELAY_MS;
}
bool SystemConfig_Validate(const SystemConfig_t *config)
{
    return config && config->max_rpm >= CONTROL_TARGET_RPM_MIN && config->max_rpm <= CONTROL_TARGET_RPM_MAX &&
        config->manual_rpm >= CONTROL_TARGET_RPM_MIN && config->manual_rpm <= config->max_rpm &&
        isfinite(config->kp) && config->kp >= 0 && config->kp <= 10 &&
        isfinite(config->ki) && config->ki >= 0 && config->ki <= 10 &&
        isfinite(config->kd) && config->kd >= 0 && config->kd <= 2 &&
        isfinite(config->gas_low_v) && isfinite(config->gas_high_v) &&
        config->gas_low_v >= 0.1f && config->gas_high_v <= 3.25f &&
        config->gas_high_v - config->gas_low_v >= 0.1f && config->shutdown_delay_ms <= 600000U;
}
