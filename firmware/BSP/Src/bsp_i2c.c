/**
 * @file    bsp_i2c.c
 * @brief   I2C 板级支持包实现
 */

#include "bsp_i2c.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"

/* 获取 I2C 句柄 */
static I2C_HandleTypeDef* get_i2c_handle(Bsp_I2C_Bus_t bus)
{
    switch (bus) {
        case BSP_I2C_BUS_1: return &hi2c1;
        case BSP_I2C_BUS_2: return &hi2c2;
        default: return NULL;
    }
}

/**
 * @brief  I2C 初始化
 */
void Bsp_I2C_Init(void)
{
    // I2C 由 CubeMX 初始化
}

void Bsp_I2C_Delay(uint32_t milliseconds)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(pdMS_TO_TICKS(milliseconds));
    else HAL_Delay(milliseconds);
}

/**
 * @brief  I2C 写数据
 */
bool Bsp_I2C_Write(Bsp_I2C_Bus_t bus, uint8_t dev_addr, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_HandleTypeDef *hi2c = get_i2c_handle(bus);
    if (hi2c == NULL) return false;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(hi2c, dev_addr, (uint8_t*)data, length, timeout);
    return (status == HAL_OK);
}

/**
 * @brief  I2C 读数据
 */
bool Bsp_I2C_Read(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_HandleTypeDef *hi2c = get_i2c_handle(bus);
    if (hi2c == NULL) return false;

    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(hi2c, dev_addr, data, length, timeout);
    return (status == HAL_OK);
}

/**
 * @brief  I2C 写寄存器
 */
bool Bsp_I2C_WriteReg(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_HandleTypeDef *hi2c = get_i2c_handle(bus);
    if (hi2c == NULL) return false;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)data, length, timeout);
    return (status == HAL_OK);
}

/**
 * @brief  I2C 读寄存器
 */
bool Bsp_I2C_ReadReg(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_HandleTypeDef *hi2c = get_i2c_handle(bus);
    if (hi2c == NULL) return false;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, data, length, timeout);
    return (status == HAL_OK);
}

/**
 * @brief  检测设备是否就绪
 */
bool Bsp_I2C_IsDeviceReady(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint32_t timeout)
{
    I2C_HandleTypeDef *hi2c = get_i2c_handle(bus);
    if (hi2c == NULL) return false;

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(hi2c, dev_addr, 3, timeout);
    return (status == HAL_OK);
}
