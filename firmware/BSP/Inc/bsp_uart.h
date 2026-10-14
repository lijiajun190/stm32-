/**
 * @file    bsp_uart.h
 * @brief   UART 板级支持包头文件
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* 函数声明 */
void Bsp_UART_Init(void);
bool Bsp_UART_Send(const uint8_t *data, uint16_t length);
bool Bsp_UART_SendBlocking(const uint8_t *data, uint16_t length, uint32_t timeout);
uint16_t Bsp_UART_Receive(uint8_t *buffer, uint16_t max_length);
uint32_t Bsp_UART_RxDropped(void);

/* 调试输出宏 */
#define BSP_DEBUG_PRINTF(fmt, ...) printf(fmt, ##__VA_ARGS__)

#endif /* BSP_UART_H */
