/**
 * @file    crc.c
 * @brief   CRC 校验算法实现
 */

#include "crc.h"

/**
 * @brief  计算 CRC8
 */
uint8_t CRC8_Calculate(const uint8_t *data, uint16_t length)
{
    uint8_t crc = 0x00;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief  计算 CRC16 (MODBUS)
 */
uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief  计算 CRC32
 */
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length)
{
    return ~CRC32_Update(0xFFFFFFFFU, data, length);
}

uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t length)
{

    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x00000001) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief  校验 CRC8
 */
bool CRC8_Verify(const uint8_t *data, uint16_t length, uint8_t crc)
{
    return (CRC8_Calculate(data, length) == crc);
}

/**
 * @brief  校验 CRC16
 */
bool CRC16_Verify(const uint8_t *data, uint16_t length, uint16_t crc)
{
    return (CRC16_Calculate(data, length) == crc);
}

/**
 * @brief  校验 CRC32
 */
bool CRC32_Verify(const uint8_t *data, uint32_t length, uint32_t crc)
{
    return (CRC32_Calculate(data, length) == crc);
}
