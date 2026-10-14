/**
 * @file    anemometer.c
 * @brief   风速传感器驱动实现
 */

#include "anemometer.h"
#include "bsp_adc.h"
#include <string.h>
#include <math.h>

static Anemometer_Data_t sensor_data;
static float v_min = 0.4f;   // 最小电压 (V)
static float v_max = 2.0f;   // 最大电压 (V)
static float speed_min = 0.0f;   // 最小风速 (m/s)
static float speed_max = 30.0f;  // 最大风速 (m/s)

/**
 * @brief  初始化风速传感器
 */
void Anemometer_Init(void)
{
    memset(&sensor_data, 0, sizeof(sensor_data));
}

/**
 * @brief  更新传感器数据
 */
void Anemometer_Update(void)
{
    Anemometer_UpdateVoltage(Bsp_ADC_GetVoltage(BSP_ADC_CH_WIND_SPEED));
}

void Anemometer_UpdateVoltage(float voltage)
{
    sensor_data.voltage = voltage;

    // 线性转换为风速
    if (isfinite(voltage) && sensor_data.voltage >= v_min && sensor_data.voltage <= v_max && v_max > v_min) {
        sensor_data.wind_speed = speed_min +
            (sensor_data.voltage - v_min) * (speed_max - speed_min) / (v_max - v_min);
        sensor_data.valid = true;
    } else {
        sensor_data.wind_speed = 0.0f;
        sensor_data.valid = false;
    }

    sensor_data.timestamp = HAL_GetTick();
}

/**
 * @brief  获取传感器数据
 */
Anemometer_Data_t Anemometer_GetData(void)
{
    return sensor_data;
}

/**
 * @brief  校准传感器
 */
void Anemometer_Calibrate(float v_min_new, float v_max_new, float speed_min_new, float speed_max_new)
{
    if (!isfinite(v_min_new) || !isfinite(v_max_new) || !isfinite(speed_min_new) ||
        !isfinite(speed_max_new) || v_max_new <= v_min_new || speed_min_new < 0.0f || speed_max_new < speed_min_new) return;
    v_min = v_min_new;
    v_max = v_max_new;
    speed_min = speed_min_new;
    speed_max = speed_max_new;
}
