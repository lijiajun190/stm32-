/**
 * @file    mq135.c
 * @brief   MQ-135 烟雾传感器驱动实现
 */

#include "mq135.h"
#include "bsp_adc.h"
#include <string.h>
#include <math.h>

static MQ135_Data_t sensor_data;
static uint32_t init_time_ms = 0;
static float r0 = 0.0f;  // 尚未标定的基准电阻

/**
 * @brief  初始化 MQ-135 传感器
 */
void MQ135_Init(void)
{
    memset(&sensor_data, 0, sizeof(sensor_data));
    sensor_data.status = MQ135_STATUS_PREHEATING;
    init_time_ms = HAL_GetTick();
    r0 = 0.0f;
}

/**
 * @brief  更新传感器数据
 */
void MQ135_Update(void)
{
    MQ135_UpdateVoltage(Bsp_ADC_GetVoltage(BSP_ADC_CH_MQ135));
}

void MQ135_UpdateVoltage(float voltage)
{
    sensor_data.timestamp = HAL_GetTick();
    sensor_data.voltage = voltage;
    sensor_data.valid = false;
    sensor_data.ppm = 0.0f;  /* 没有气种、分压比例及曲线标定，不提供虚假的 ppm。 */
    sensor_data.resistance = 0.0f;
    sensor_data.ratio = 0.0f;
    // 检查预热状态
    if (sensor_data.status == MQ135_STATUS_PREHEATING) {
        if ((HAL_GetTick() - init_time_ms) >= MQ135_PREHEAT_TIME_MS) {
            sensor_data.status = MQ135_STATUS_READY;
        }
        if (sensor_data.status == MQ135_STATUS_PREHEATING) return;
    }
    if (sensor_data.status == MQ135_STATUS_NOT_INIT) return;

    // 计算传感器电阻 Rs
    // 假设电路为: VCC - Rs - ADC - RL - GND
    // Rs = (VCC - Vout) * RL / Vout
    const float VCC = 5.0f;
    const float RL = 10000.0f;  // 负载电阻 10kΩ

    if (isfinite(voltage) && voltage > 0.1f && voltage < 3.25f) {
        sensor_data.resistance = (VCC - sensor_data.voltage) * RL / sensor_data.voltage;
        sensor_data.valid = true;
        sensor_data.status = MQ135_STATUS_READY;
        if (sensor_data.calibrated && r0 > 0.0f) sensor_data.ratio = sensor_data.resistance / r0;
    } else {
        sensor_data.status = MQ135_STATUS_ERROR;
    }

    sensor_data.timestamp = HAL_GetTick();
}

/**
 * @brief  检查传感器是否就绪
 */
bool MQ135_IsReady(void)
{
    return (sensor_data.status == MQ135_STATUS_READY);
}

/**
 * @brief  获取传感器数据
 */
MQ135_Data_t MQ135_GetData(void)
{
    return sensor_data;
}

/**
 * @brief  校准传感器
 * @param  clean_air_ratio: 清洁空气中的 Rs/R0 比值
 */
void MQ135_Calibrate(float clean_air_ratio)
{
    if (sensor_data.valid && isfinite(clean_air_ratio) && clean_air_ratio > 0.0f && sensor_data.resistance > 0) {
        r0 = sensor_data.resistance / clean_air_ratio;
        sensor_data.calibrated = isfinite(r0) && r0 > 0.0f;
    }
}
