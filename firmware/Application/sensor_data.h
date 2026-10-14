/** 统一的传感器快照，作为 SensorTask 到系统管理任务的队列契约。 */
#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t adc_raw[4];
    float adc_voltage[4];
    float gas_voltage;
    float gas_resistance;
    float gas_ratio; /* Rs/R0，仅完成清洁空气校准时有效；不是 ppm。 */
    float wind_speed;
    float temperature;
    float temperature_raw; /* 过温保护使用未滤波的最新温度。 */
    float humidity;
    float pressure;
    uint32_t timestamp_ms;
    uint32_t valid_flags;
    uint32_t error_flags;
    bool mq_preheating;
    bool mq_uncalibrated;
} SensorData_t;

#define SENSOR_VALID_GAS        (1UL << 0)
#define SENSOR_VALID_WIND       (1UL << 1)
#define SENSOR_VALID_SHT30      (1UL << 2)
#define SENSOR_VALID_BMP280     (1UL << 3)
#define SENSOR_VALID_ADC        (1UL << 4)
#define SENSOR_AUTO_REQUIRED    (SENSOR_VALID_GAS | SENSOR_VALID_WIND | SENSOR_VALID_SHT30)
#define SENSOR_ERROR_ADC        (1UL << 0)
#define SENSOR_ERROR_SHT30      (1UL << 1)
#define SENSOR_ERROR_BMP280     (1UL << 2)
#define SENSOR_ERROR_RANGE      (1UL << 3)
#endif
