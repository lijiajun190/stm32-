/**
 * @file    crc.h
 * @brief   CRC 校验算法
 */

#ifndef CRC_H
#define CRC_H

#include <stdint.h>
#include <stdbool.h>

/* 函数声明 */
uint8_t CRC8_Calculate(const uint8_t *data, uint16_t length);
uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length);
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length);
/* 分块 CRC：初值 0xFFFFFFFF，所有分块完成后取反。 */
uint32_t CRC32_Update(uint32_t state, const uint8_t *data, uint32_t length);
bool CRC8_Verify(const uint8_t *data, uint16_t length, uint8_t crc);
bool CRC16_Verify(const uint8_t *data, uint16_t length, uint16_t crc);
bool CRC32_Verify(const uint8_t *data, uint32_t length, uint32_t crc);

#endif /* CRC_H */
