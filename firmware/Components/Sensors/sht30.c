/**
 * @file    sht30.c
 * @brief   SHT30 温湿度传感器驱动实现
 */

#include "sht30.h"
#include "bsp_i2c.h"

/* SHT30 命令 */
#define SHT30_CMD_MEAS_HIGH_REP    0x2C06  // 高重复性测量
#define SHT30_CMD_SOFT_RESET       0x30A2  // 软复位

/**
 * @brief  计算 CRC8
 */
static uint8_t calculate_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  初始化 SHT30
 */
bool SHT30_Init(void)
{
    return SHT30_IsPresent();
}

/**
 * @brief  读取温湿度数据
 */
bool SHT30_Read(SHT30_Data_t *data)
{
    if (!data) return false;
    data->valid = false;
    data->timestamp = HAL_GetTick();
    if (!SHT30_BeginRead()) return false;
    HAL_Delay(20); /* 兼容同步接口；SensorTask 使用两阶段接口和 vTaskDelay。 */
    return SHT30_FinishRead(data);
}

bool SHT30_BeginRead(void)
{
    uint8_t cmd[2] = {0x24, 0x00}; /* 高重复性，不使用 clock stretching。 */
    return Bsp_I2C_Write(BSP_I2C_BUS_1, SHT30_I2C_ADDR, cmd, 2, 5);
}

bool SHT30_FinishRead(SHT30_Data_t *data)
{
    uint8_t buffer[6];
    if (!data) return false;
    data->valid = false;
    data->temperature = 0.0f;
    data->humidity = 0.0f;
    data->timestamp = HAL_GetTick();
    if (!Bsp_I2C_Read(BSP_I2C_BUS_1, SHT30_I2C_ADDR, buffer, 6, 5)) {
        return false;
    }

    // 校验 CRC
    if (calculate_crc8(&buffer[0], 2) != buffer[2] ||
        calculate_crc8(&buffer[3], 2) != buffer[5]) {
        return false;
    }

    // 解析数据
    uint16_t temp_raw = (buffer[0] << 8) | buffer[1];
    uint16_t humi_raw = (buffer[3] << 8) | buffer[4];

    data->temperature = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
    data->humidity = 100.0f * (float)humi_raw / 65535.0f;
    if (data->temperature < -40.0f || data->temperature > 125.0f ||
        data->humidity < 0.0f || data->humidity > 100.0f) return false;
    data->valid = true;
    data->timestamp = HAL_GetTick();

    return true;
}

/**
 * @brief  软复位
 */
bool SHT30_SoftReset(void)
{
    uint8_t cmd[2] = {0x30, 0xA2};
    return Bsp_I2C_Write(BSP_I2C_BUS_1, SHT30_I2C_ADDR, cmd, 2, 100);
}

/**
 * @brief  检测设备是否存在
 */
bool SHT30_IsPresent(void)
{
    return Bsp_I2C_IsDeviceReady(BSP_I2C_BUS_1, SHT30_I2C_ADDR, 100);
}
