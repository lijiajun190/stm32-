/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区实现
 */

#include "ring_buffer.h"
#include <string.h>

/**
 * @brief  初始化环形缓冲区
 */
void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t size)
{
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

/**
 * @brief  写入一个字节
 */
bool RingBuffer_Put(RingBuffer_t *rb, uint8_t data)
{
    if (RingBuffer_IsFull(rb)) {
        return false;
    }

    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rb->size;
    rb->count++;
    return true;
}

/**
 * @brief  读取一个字节
 */
bool RingBuffer_Get(RingBuffer_t *rb, uint8_t *data)
{
    if (RingBuffer_IsEmpty(rb)) {
        return false;
    }

    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    rb->count--;
    return true;
}

/**
 * @brief  查看一个字节（不移除）
 */
bool RingBuffer_Peek(RingBuffer_t *rb, uint8_t *data)
{
    if (RingBuffer_IsEmpty(rb)) {
        return false;
    }

    *data = rb->buffer[rb->tail];
    return true;
}

/**
 * @brief  获取已用空间
 */
uint16_t RingBuffer_GetCount(RingBuffer_t *rb)
{
    return rb->count;
}

/**
 * @brief  获取剩余空间
 */
uint16_t RingBuffer_GetFree(RingBuffer_t *rb)
{
    return rb->size - rb->count;
}

/**
 * @brief  检查是否满
 */
bool RingBuffer_IsFull(RingBuffer_t *rb)
{
    return (rb->count >= rb->size);
}

/**
 * @brief  检查是否空
 */
bool RingBuffer_IsEmpty(RingBuffer_t *rb)
{
    return (rb->count == 0);
}

/**
 * @brief  清空缓冲区
 */
void RingBuffer_Clear(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

/**
 * @brief  写入多个字节
 */
uint16_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint16_t length)
{
    uint16_t written = 0;
    for (uint16_t i = 0; i < length; i++) {
        if (RingBuffer_Put(rb, data[i])) {
            written++;
        } else {
            break;
        }
    }
    return written;
}

/**
 * @brief  读取多个字节
 */
uint16_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, uint16_t length)
{
    uint16_t read = 0;
    for (uint16_t i = 0; i < length; i++) {
        if (RingBuffer_Get(rb, &data[i])) {
            read++;
        } else {
            break;
        }
    }
    return read;
}
