/**
 * @file    filter.h
 * @brief   滤波算法
 */

#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

/* 均值滤波器 */
typedef struct {
    float *buffer;
    uint16_t size;
    uint16_t index;
    float sum;
    bool full;
} MovingAvg_Filter_t;

/* 一阶低通滤波器 */
typedef struct {
    float alpha;        // 滤波系数 (0~1)
    float output;       // 输出值
    bool initialized;   // 初始化标志
} LowPass_Filter_t;

/* 中值滤波器 */
typedef struct {
    float *buffer;
    uint16_t size;
    uint16_t index;
} Median_Filter_t;

/* 函数声明 - 均值滤波 */
void MovingAvg_Init(MovingAvg_Filter_t *filter, float *buffer, uint16_t size);
float MovingAvg_Update(MovingAvg_Filter_t *filter, float input);
void MovingAvg_Reset(MovingAvg_Filter_t *filter);

/* 函数声明 - 低通滤波 */
void LowPass_Init(LowPass_Filter_t *filter, float alpha);
float LowPass_Update(LowPass_Filter_t *filter, float input);
void LowPass_Reset(LowPass_Filter_t *filter);

/* 函数声明 - 中值滤波 */
void Median_Init(Median_Filter_t *filter, float *buffer, uint16_t size);
float Median_Update(Median_Filter_t *filter, float input);
void Median_Reset(Median_Filter_t *filter);

#endif /* FILTER_H */
