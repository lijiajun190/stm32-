/**
 * @file    filter.c
 * @brief   滤波算法实现
 */

#include "filter.h"
#include <string.h>

/* ========== 均值滤波 ========== */

void MovingAvg_Init(MovingAvg_Filter_t *filter, float *buffer, uint16_t size)
{
    filter->buffer = buffer;
    filter->size = size;
    filter->index = 0;
    filter->sum = 0;
    filter->full = false;
    memset(buffer, 0, size * sizeof(float));
}

float MovingAvg_Update(MovingAvg_Filter_t *filter, float input)
{
    // 减去最旧的值
    filter->sum -= filter->buffer[filter->index];

    // 添加新值
    filter->buffer[filter->index] = input;
    filter->sum += input;

    // 更新索引
    filter->index++;
    if (filter->index >= filter->size) {
        filter->index = 0;
        filter->full = true;
    }

    // 计算平均值
    uint16_t count = filter->full ? filter->size : filter->index;
    return filter->sum / count;
}

void MovingAvg_Reset(MovingAvg_Filter_t *filter)
{
    filter->index = 0;
    filter->sum = 0;
    filter->full = false;
    memset(filter->buffer, 0, filter->size * sizeof(float));
}

/* ========== 一阶低通滤波 ========== */

void LowPass_Init(LowPass_Filter_t *filter, float alpha)
{
    filter->alpha = alpha;
    filter->output = 0;
    filter->initialized = false;
}

float LowPass_Update(LowPass_Filter_t *filter, float input)
{
    if (!filter->initialized) {
        filter->output = input;
        filter->initialized = true;
    } else {
        filter->output = filter->alpha * input + (1.0f - filter->alpha) * filter->output;
    }
    return filter->output;
}

void LowPass_Reset(LowPass_Filter_t *filter)
{
    filter->output = 0;
    filter->initialized = false;
}

/* ========== 中值滤波 ========== */

static float get_median(float *array, uint16_t size)
{
    // 冒泡排序（简单实现）
    float temp[size];
    memcpy(temp, array, size * sizeof(float));

    for (uint16_t i = 0; i < size - 1; i++) {
        for (uint16_t j = 0; j < size - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                float t = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = t;
            }
        }
    }

    return temp[size / 2];
}

void Median_Init(Median_Filter_t *filter, float *buffer, uint16_t size)
{
    filter->buffer = buffer;
    filter->size = size;
    filter->index = 0;
    memset(buffer, 0, size * sizeof(float));
}

float Median_Update(Median_Filter_t *filter, float input)
{
    filter->buffer[filter->index] = input;
    filter->index++;
    if (filter->index >= filter->size) {
        filter->index = 0;
    }

    return get_median(filter->buffer, filter->size);
}

void Median_Reset(Median_Filter_t *filter)
{
    filter->index = 0;
    memset(filter->buffer, 0, filter->size * sizeof(float));
}
