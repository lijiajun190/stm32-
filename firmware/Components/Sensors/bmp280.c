/**
 * @file    bmp280.c
 * @brief   BMP280 气压传感器驱动实现
 */

#include "bmp280.h"
#include "bsp_i2c.h"

/* BMP280 寄存器地址 */
#define BMP280_REG_TEMP_XLSB    0xFC
#define BMP280_REG_TEMP_LSB     0xFB
#define BMP280_REG_TEMP_MSB     0xFA
#define BMP280_REG_PRESS_XLSB   0xF9
#define BMP280_REG_PRESS_LSB    0xF8
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_STATUS       0xF3
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_ID           0xD0

/* 校准参数 */
static struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} calib;

static bool initialized;

/**
 * @brief  读取校准参数
 */
static bool read_calibration(void)
{
    uint8_t data[24];

    if (!Bsp_I2C_ReadReg(BSP_I2C_BUS_1, BMP280_I2C_ADDR, 0x88, data, 24, 5)) {
        return false;
    }

    calib.dig_T1 = (data[1] << 8) | data[0];
    calib.dig_T2 = (data[3] << 8) | data[2];
    calib.dig_T3 = (data[5] << 8) | data[4];
    calib.dig_P1 = (data[7] << 8) | data[6];
    calib.dig_P2 = (data[9] << 8) | data[8];
    calib.dig_P3 = (data[11] << 8) | data[10];
    calib.dig_P4 = (data[13] << 8) | data[12];
    calib.dig_P5 = (data[15] << 8) | data[14];
    calib.dig_P6 = (data[17] << 8) | data[16];
    calib.dig_P7 = (data[19] << 8) | data[18];
    calib.dig_P8 = (data[21] << 8) | data[20];
    calib.dig_P9 = (data[23] << 8) | data[22];

    /* 无效的 NVM 参数不能用于补偿，避免除零或输出伪有效气压。 */
    return calib.dig_T1 != 0 && calib.dig_T1 != 0xFFFF &&
           calib.dig_P1 != 0 && calib.dig_P1 != 0xFFFF;
}

/**
 * @brief  初始化 BMP280
 */
bool BMP280_Init(void)
{
    initialized = false;
    uint8_t id;

    // 读取 ID
    if (!Bsp_I2C_ReadReg(BSP_I2C_BUS_1, BMP280_I2C_ADDR, BMP280_REG_ID, &id, 1, 5)) {
        return false;
    }

    if (id != 0x58) {  // BMP280 ID
        return false;
    }

    // 软复位
    if (!BMP280_SoftReset()) {
        return false;
    }
    Bsp_I2C_Delay(10);

    // 读取校准参数
    if (!read_calibration()) {
        return false;
    }

    // 配置：正常模式，温度 x2、气压 x16
    uint8_t ctrl_meas = 0x57;  // osrs_t=x2, osrs_p=x16, mode=normal
    initialized = Bsp_I2C_WriteReg(BSP_I2C_BUS_1, BMP280_I2C_ADDR,
                                  BMP280_REG_CTRL_MEAS, &ctrl_meas, 1, 5);
    return initialized;
}

/**
 * @brief  读取气压和温度
 */
bool BMP280_Read(BMP280_Data_t *data)
{
    uint8_t buffer[6];
    if (!data) return false;
    data->valid = false;
    data->temperature = 0.0f;
    data->pressure = 0.0f;
    data->timestamp = HAL_GetTick();
    if (!initialized) return false;

    // 读取原始数据
    if (!Bsp_I2C_ReadReg(BSP_I2C_BUS_1, BMP280_I2C_ADDR, BMP280_REG_PRESS_MSB, buffer, 6, 5)) {
        initialized = false;
        return false;
    }

    int32_t adc_P = (buffer[0] << 12) | (buffer[1] << 4) | (buffer[2] >> 4);
    int32_t adc_T = (buffer[3] << 12) | (buffer[4] << 4) | (buffer[5] >> 4);
    if (adc_P == 0x80000 || adc_T == 0x80000) return false; /* 跳过测量的标记 */

    /* Bosch BMP280 数据手册浮点补偿公式。double 避免原 32 位有符号
       乘法溢出和负数左移；采样 10 Hz 时计算成本远小于 I2C 等待。 */
    double var1 = ((double)adc_T / 16384.0 - (double)calib.dig_T1 / 1024.0) * calib.dig_T2;
    double var2 = ((double)adc_T / 131072.0 - (double)calib.dig_T1 / 8192.0);
    var2 = var2 * var2 * calib.dig_T3;
    double fine = var1 + var2;
    data->temperature = (float)(fine / 5120.0);
    var1 = fine / 2.0 - 64000.0;
    var2 = var1 * var1 * calib.dig_P6 / 32768.0;
    var2 += var1 * calib.dig_P5 * 2.0;
    var2 = var2 / 4.0 + (double)calib.dig_P4 * 65536.0;
    var1 = (calib.dig_P3 * var1 * var1 / 524288.0 + calib.dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * calib.dig_P1;
    if (var1 == 0.0) return false;
    double pressure = 1048576.0 - adc_P;
    pressure = (pressure - var2 / 4096.0) * 6250.0 / var1;
    var1 = calib.dig_P9 * pressure * pressure / 2147483648.0;
    var2 = pressure * calib.dig_P8 / 32768.0;
    pressure += (var1 + var2 + calib.dig_P7) / 16.0;
    data->pressure = (float)pressure;
    if (data->temperature < -40.0f || data->temperature > 85.0f ||
        data->pressure < 30000.0f || data->pressure > 110000.0f) return false;
    data->valid = true;
    data->timestamp = HAL_GetTick();

    return true;
}

/**
 * @brief  软复位
 */
bool BMP280_SoftReset(void)
{
    initialized = false;
    uint8_t cmd = 0xB6;
    return Bsp_I2C_WriteReg(BSP_I2C_BUS_1, BMP280_I2C_ADDR, BMP280_REG_RESET, &cmd, 1, 5);
}

bool BMP280_IsInitialized(void) { return initialized; }

/**
 * @brief  检测设备是否存在
 */
bool BMP280_IsPresent(void)
{
    return Bsp_I2C_IsDeviceReady(BSP_I2C_BUS_1, BMP280_I2C_ADDR, 100);
}
