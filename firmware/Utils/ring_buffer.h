/**
 * @file    ring_buffer.h
 * @brief   环形缓冲区
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* 环形缓冲区 */
typedef struct {
    uint8_t *buffer;
    uint16_t size;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} RingBuffer_t;

/* 函数声明 */
void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t size);
bool RingBuffer_Put(RingBuffer_t *rb, uint8_t data);
bool RingBuffer_Get(RingBuffer_t *rb, uint8_t *data);
bool RingBuffer_Peek(RingBuffer_t *rb, uint8_t *data);
uint16_t RingBuffer_GetCount(RingBuffer_t *rb);
uint16_t RingBuffer_GetFree(RingBuffer_t *rb);
bool RingBuffer_IsFull(RingBuffer_t *rb);
bool RingBuffer_IsEmpty(RingBuffer_t *rb);
void RingBuffer_Clear(RingBuffer_t *rb);
uint16_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint16_t length);
uint16_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, uint16_t length);

#endif /* RING_BUFFER_H */
