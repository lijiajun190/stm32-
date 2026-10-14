/**
 * @file    mq135.h
 * @brief   MQ-135 烟雾传感器驱动
 */

#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>
#include <stdbool.h>

/* MQ-135 参数 */
#define MQ135_PREHEAT_TIME_MS    60000  // 预热时间 60秒

/* 传感器状态 */
typedef enum {
    MQ135_STATUS_NOT_INIT = 0,
    MQ135_STATUS_PREHEATING,
    MQ135_STATUS_READY,
    MQ135_STATUS_ERROR
} MQ135_Status_t;

/* 传感器数据 */
typedef struct {
    float voltage;           // 电压值 (V)
    float resistance;        // 传感器电阻 (Ω)
    float ppm;              // 浓度 (ppm)
    float ratio;            // Rs/R0，非气体浓度
    bool valid;
    bool calibrated;        // 仅电阻基准校准，不等于完成 ppm 曲线标定
    MQ135_Status_t status;  // 状态
    uint32_t timestamp;     // 时间戳
} MQ135_Data_t;

/* 函数声明 */
void MQ135_Init(void);
void MQ135_Update(void);
void MQ135_UpdateVoltage(float voltage);
bool MQ135_IsReady(void);
MQ135_Data_t MQ135_GetData(void);
void MQ135_Calibrate(float clean_air_ratio);

#endif /* MQ135_H */
