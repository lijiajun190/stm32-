/**
 * @file    bsp_adc.c
 * @brief   ADC 板级支持包实现
 */

#include "bsp_adc.h"
#include "adc.h"

/* ADC DMA 缓冲区 */
/* 连续扫描 DMA 由硬件持续刷新，读取时必须按易变数据处理。 */
static volatile uint16_t adc_dma_buffer[BSP_ADC_CH_MAX];
static bool adc_running;

/**
 * @brief  ADC 初始化
 */
void Bsp_ADC_Init(void)
{
    // ADC 由 CubeMX 初始化
}

/**
 * @brief  获取 ADC 原始值
 * @param  channel: 通道号 (0~3)
 * @return ADC 原始值 (0~4095)
 */
uint16_t Bsp_ADC_GetValue(uint8_t channel)
{
    if (channel >= BSP_ADC_CH_MAX) return 0;
    return adc_dma_buffer[channel];
}

/**
 * @brief  获取 ADC 电压值
 * @param  channel: 通道号 (0~3)
 * @return 电压值 (V)
 */
float Bsp_ADC_GetVoltage(uint8_t channel)
{
    uint16_t raw = Bsp_ADC_GetValue(channel);
    return (raw * 3.3f) / 4095.0f;
}

bool Bsp_ADC_GetSnapshot(uint16_t raw[BSP_ADC_CH_MAX])
{
    if (!raw || !adc_running || HAL_ADC_GetError(&hadc1) != HAL_ADC_ERROR_NONE ||
        !hadc1.DMA_Handle || !(hadc1.DMA_Handle->Instance->CR & DMA_SxCR_EN)) return false;
    for (uint8_t i = 0; i < BSP_ADC_CH_MAX; ++i) raw[i] = adc_dma_buffer[i];
    return true;
}

/**
 * @brief  启动 ADC DMA 采集
 */
HAL_StatusTypeDef Bsp_ADC_StartDMA(void)
{
    HAL_StatusTypeDef status = HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, BSP_ADC_CH_MAX);
    adc_running = status == HAL_OK;
    /* 连续四通道扫描约数万次/秒，本层由任务轮询，不需要每两样本一次 DMA 中断。 */
    if (adc_running) __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT | DMA_IT_TC);
    return status;
}

/**
 * @brief  停止 ADC DMA 采集
 */
void Bsp_ADC_StopDMA(void)
{
    adc_running = false;
    HAL_ADC_Stop_DMA(&hadc1);
}
