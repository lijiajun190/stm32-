#include "sensor_filter.h"
#include "control_config.h"
#include <math.h>

static float update(LowPass_Filter_t *filter, float value, bool valid)
{
    if (!valid || !isfinite(value)) { LowPass_Reset(filter); return 0.0f; }
    return LowPass_Update(filter, value);
}
void SensorFilter_Init(SensorFilter_t *filter)
{
    unsigned i;
    for (i = 0; i < 4; ++i) LowPass_Init(&filter->voltage[i], CONTROL_SENSOR_ALPHA);
    LowPass_Init(&filter->wind, CONTROL_SENSOR_ALPHA);
    LowPass_Init(&filter->temperature, CONTROL_SENSOR_ALPHA);
    LowPass_Init(&filter->humidity, CONTROL_SENSOR_ALPHA);
    LowPass_Init(&filter->pressure, CONTROL_SENSOR_ALPHA);
}
void SensorFilter_Apply(SensorFilter_t *filter, SensorData_t *data)
{
    unsigned i;
    /* 范围判断在滤波前完成，异常值不得被滤波掩盖。 */
    for (i = 0; i < 4; ++i)
        data->adc_voltage[i] = update(&filter->voltage[i], data->adc_voltage[i],
                                     (data->valid_flags & SENSOR_VALID_ADC) != 0);
    if (!(data->valid_flags & SENSOR_VALID_GAS)) LowPass_Reset(&filter->voltage[0]);
    data->gas_voltage = (data->valid_flags & SENSOR_VALID_GAS) ? data->adc_voltage[0] : 0.0f;
    data->wind_speed = update(&filter->wind, data->wind_speed, (data->valid_flags & SENSOR_VALID_WIND) != 0);
    data->temperature = update(&filter->temperature, data->temperature, (data->valid_flags & SENSOR_VALID_SHT30) != 0);
    data->humidity = update(&filter->humidity, data->humidity, (data->valid_flags & SENSOR_VALID_SHT30) != 0);
    data->pressure = update(&filter->pressure, data->pressure, (data->valid_flags & SENSOR_VALID_BMP280) != 0);
}
