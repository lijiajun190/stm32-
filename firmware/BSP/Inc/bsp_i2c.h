/**
 * @file    bsp_i2c.h
 * @brief   I2C 板级支持包头文件
 */

#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* I2C 总线定义 */
typedef enum {
    BSP_I2C_BUS_1 = 0,  // I2C1: PB8/PB9 - 传感器总线
    BSP_I2C_BUS_2,      // I2C2: PB10/PB3 - OLED 总线
    BSP_I2C_BUS_MAX
} Bsp_I2C_Bus_t;

/* 函数声明 */
void Bsp_I2C_Init(void);
void Bsp_I2C_Delay(uint32_t milliseconds);
bool Bsp_I2C_Write(Bsp_I2C_Bus_t bus, uint8_t dev_addr, const uint8_t *data, uint16_t length, uint32_t timeout);
bool Bsp_I2C_Read(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint8_t *data, uint16_t length, uint32_t timeout);
bool Bsp_I2C_WriteReg(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t length, uint32_t timeout);
bool Bsp_I2C_ReadReg(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t length, uint32_t timeout);
bool Bsp_I2C_IsDeviceReady(Bsp_I2C_Bus_t bus, uint8_t dev_addr, uint32_t timeout);

#endif /* BSP_I2C_H */
