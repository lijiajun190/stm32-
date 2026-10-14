/**
 * @file    bsp_uart.c
 * @brief   UART 板级支持包实现
 */

#include "bsp_uart.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* 单生产者 IRQ、单消费者 Logger；索引访问在短临界区内完成。 */
#define UART_RX_BYTES 1024U
static uint8_t rx_storage[UART_RX_BYTES], rx_byte, tx_storage[512];
static volatile uint16_t rx_head, rx_tail;
static volatile uint32_t rx_dropped;
static volatile bool rx_armed;

/**
 * @brief  UART 初始化
 */
void Bsp_UART_Init(void)
{
    rx_head = rx_tail = 0; rx_dropped = 0;
    rx_armed = HAL_UART_Receive_IT(&huart1, &rx_byte, 1) == HAL_OK;
}

/**
 * @brief  通过 DMA 发送数据（非阻塞）
 * @param  data: 数据指针
 * @param  length: 数据长度
 * @return true=成功, false=失败
 */
bool Bsp_UART_Send(const uint8_t *data, uint16_t length)
{
    if (!data || !length || length > sizeof(tx_storage)) return false;
    uint32_t saved = __get_PRIMASK(); __disable_irq();
    if (huart1.gState != HAL_UART_STATE_READY) { __set_PRIMASK(saved); return false; }
    /* DMA 发送期间保留驱动私有副本，不借用调用方的栈缓冲。 */
    memcpy(tx_storage, data, length);
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&huart1, tx_storage, length);
    __set_PRIMASK(saved);
    return (status == HAL_OK);
}

/**
 * @brief  阻塞发送数据
 * @param  data: 数据指针
 * @param  length: 数据长度
 * @param  timeout: 超时时间(ms)
 * @return true=成功, false=失败
 */
bool Bsp_UART_SendBlocking(const uint8_t *data, uint16_t length, uint32_t timeout)
{
    if (!data || !length) return false;
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, (uint8_t*)data, length, timeout);
    return (status == HAL_OK);
}

/**
 * @brief  接收数据
 * @param  buffer: 接收缓冲区
 * @param  max_length: 最大长度
 * @return 实际接收到的字节数
 */
uint16_t Bsp_UART_Receive(uint8_t *buffer, uint16_t max_length)
{
    if (!buffer) return 0;
    uint16_t count = 0;
    uint32_t saved = __get_PRIMASK(); __disable_irq();
    while (count < max_length && rx_tail != rx_head) {
        buffer[count++] = rx_storage[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % UART_RX_BYTES);
    }
    if (!rx_armed) rx_armed = HAL_UART_Receive_IT(&huart1, &rx_byte, 1) == HAL_OK;
    __set_PRIMASK(saved);
    return count;
}

uint32_t Bsp_UART_RxDropped(void) { return rx_dropped; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    uint16_t next = (uint16_t)((rx_head + 1U) % UART_RX_BYTES);
    if (next == rx_tail) ++rx_dropped;
    else { rx_storage[rx_head] = rx_byte; rx_head = next; }
    rx_armed = HAL_UART_Receive_IT(uart, &rx_byte, 1) == HAL_OK;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    ++rx_dropped;
    (void)HAL_UART_AbortReceive(uart);
    rx_armed = HAL_UART_Receive_IT(uart, &rx_byte, 1) == HAL_OK;
}

/**
 * @brief  重定向 printf 到 UART
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    uint8_t byte = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, &byte, 1, 20U);
    return ch;
}
