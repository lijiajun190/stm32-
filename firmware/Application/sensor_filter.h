#ifndef SENSOR_FILTER_H
#define SENSOR_FILTER_H
#include "sensor_data.h"
#include "filter.h"
typedef struct {
    LowPass_Filter_t voltage[4], wind, temperature, humidity, pressure;
} SensorFilter_t;
void SensorFilter_Init(SensorFilter_t *filter);
/* 只滤波有效采样；错误时清除历史，恢复的首样本不会混入旧数据。 */
void SensorFilter_Apply(SensorFilter_t *filter, SensorData_t *data);
#endif
