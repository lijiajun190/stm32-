/**
 * @file    bmp280.h
 * @brief   BMP280 气压传感器驱动
 */

#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* BMP280 I2C 地址 */
#define BMP280_I2C_ADDR    0xEC  // 7位地址 0x76 << 1

/* 传感器数据 */
typedef struct {
    float pressure;        // 气压 (Pa)
    float temperature;     // 温度 (°C)
    bool valid;           // 数据有效标志
    uint32_t timestamp;   // 时间戳
} BMP280_Data_t;

/* 函数声明 */
bool BMP280_Init(void);
bool BMP280_Read(BMP280_Data_t *data);
bool BMP280_SoftReset(void);
bool BMP280_IsPresent(void);
bool BMP280_IsInitialized(void);

#endif /* BMP280_H */
