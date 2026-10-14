/**
 * @file    sht30.h
 * @brief   SHT30 温湿度传感器驱动
 */

#ifndef SHT30_H
#define SHT30_H

#include <stdint.h>
#include <stdbool.h>

/* SHT30 I2C 地址 */
#define SHT30_I2C_ADDR    0x88  // 7位地址 0x44 << 1

/* 传感器数据 */
typedef struct {
    float temperature;      // 温度 (°C)
    float humidity;         // 湿度 (%)
    bool valid;            // 数据有效标志
    uint32_t timestamp;    // 时间戳
} SHT30_Data_t;

/* 函数声明 */
bool SHT30_Init(void);
bool SHT30_Read(SHT30_Data_t *data);
/* 非阻塞测量两阶段，Begin 后至少等待 20 ms 再 Finish；任务可释放 I2C 锁。 */
bool SHT30_BeginRead(void);
bool SHT30_FinishRead(SHT30_Data_t *data);
bool SHT30_SoftReset(void);
bool SHT30_IsPresent(void);

#endif /* SHT30_H */
