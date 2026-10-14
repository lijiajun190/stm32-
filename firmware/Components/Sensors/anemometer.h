/**
 * @file    anemometer.h
 * @brief   风速传感器驱动（基于 ADC）
 */

#ifndef ANEMOMETER_H
#define ANEMOMETER_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* 传感器数据 */
typedef struct {
    float voltage;         // 电压值 (V)
    float wind_speed;      // 风速 (m/s)
    bool valid;           // 数据有效标志
    uint32_t timestamp;   // 时间戳
} Anemometer_Data_t;

/* 函数声明 */
void Anemometer_Init(void);
void Anemometer_Update(void);
void Anemometer_UpdateVoltage(float voltage);
Anemometer_Data_t Anemometer_GetData(void);
void Anemometer_Calibrate(float v_min, float v_max, float speed_min, float speed_max);

#endif /* ANEMOMETER_H */
