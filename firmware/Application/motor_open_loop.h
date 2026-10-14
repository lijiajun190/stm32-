/** @file motor_open_loop.h
 *  @brief 开环扫测时序；仅电机任务访问实例，不依赖 HAL/FreeRTOS。
 */
#ifndef MOTOR_OPEN_LOOP_H
#define MOTOR_OPEN_LOOP_H
#include <stdbool.h>
#include <stdint.h>

/* 每档至少 1 s，便于采集串口结果；最长 60 s，时间差可安全使用无符号减法。 */
#define MOTOR_OPEN_LOOP_MIN_DWELL_MS 1000U
#define MOTOR_OPEN_LOOP_MAX_DWELL_MS 60000U
typedef struct {
    bool active;
    uint8_t step;
    uint32_t dwell_ms;
    uint32_t step_start_ms;
} Motor_OpenLoop_t;

bool Motor_OpenLoop_Start(Motor_OpenLoop_t *test, uint32_t now_ms, uint32_t dwell_ms);
void Motor_OpenLoop_Stop(Motor_OpenLoop_t *test);
/* 到期后返回下一档；仅运行一次 0/30/60/100/0%，最后自动结束。 */
bool Motor_OpenLoop_Poll(Motor_OpenLoop_t *test, uint32_t now_ms, uint16_t *duty);
#endif
