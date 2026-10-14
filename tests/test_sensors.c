/* 真实传感器驱动 + I2C/ADC 替身。BMP280 使用 Bosch 数据手册参考输入。 */
#include "mq135.h"
#include "anemometer.h"
#include "sht30.h"
#include "bmp280.h"
#include "bsp_adc.h"
#include "bsp_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned checks;
uint32_t test_primask;
static uint32_t tick;
static uint8_t sht_frame[6], bmp_frame[6], calibration[24];
static bool read_ok = true, write_ok = true;
static uint8_t chip_id = 0x58;
#define CHECK(x) do { ++checks; if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)
uint32_t HAL_GetTick(void) { return tick; }
void HAL_Delay(uint32_t ms) { tick += ms; }
void Bsp_I2C_Delay(uint32_t ms) { tick += ms; }
float Bsp_ADC_GetVoltage(uint8_t ch) { return ch == 0 ? 1.0f : 1.2f; }
bool Bsp_I2C_Write(Bsp_I2C_Bus_t bus, uint8_t addr, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    CHECK(bus == BSP_I2C_BUS_1 && addr == 0x88 && len == 2);
    if (data[0] == 0x24) CHECK(data[1] == 0 && timeout == 5);
    return write_ok;
}
bool Bsp_I2C_Read(Bsp_I2C_Bus_t bus, uint8_t addr, uint8_t *data, uint16_t len, uint32_t timeout)
{
    CHECK(bus == BSP_I2C_BUS_1 && addr == 0x88 && len == 6 && timeout == 5);
    memcpy(data, sht_frame, len); return read_ok;
}
bool Bsp_I2C_ReadReg(Bsp_I2C_Bus_t bus, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len, uint32_t timeout)
{
    CHECK(bus == BSP_I2C_BUS_1 && addr == 0xEC && timeout == 5);
    if (reg == 0xD0) { CHECK(len == 1); *data = chip_id; }
    else if (reg == 0x88) { CHECK(len == 24); memcpy(data, calibration, len); }
    else if (reg == 0xF7) { CHECK(len == 6); memcpy(data, bmp_frame, len); }
    else CHECK(0);
    return read_ok;
}
bool Bsp_I2C_WriteReg(Bsp_I2C_Bus_t bus, uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    CHECK(bus == BSP_I2C_BUS_1 && addr == 0xEC && len == 1 && timeout == 5);
    CHECK((reg == 0xE0 && data[0] == 0xB6) || (reg == 0xF4 && data[0] == 0x57));
    return write_ok;
}
bool Bsp_I2C_IsDeviceReady(Bsp_I2C_Bus_t bus, uint8_t addr, uint32_t timeout)
{ (void)bus; (void)addr; (void)timeout; return read_ok; }
static uint8_t crc(const uint8_t *data)
{
    /* 独立的逐字节 CRC 参考，用于构造温湿度响应。 */
    unsigned i, j; uint8_t value = 255;
    for (i = 0; i < 2; ++i) { value ^= data[i];
        for (j = 0; j < 8; ++j) value = (value & 128) ? (uint8_t)((value * 2) ^ 49) : (uint8_t)(value * 2); }
    return value;
}
static void encode_sht(uint16_t temp, uint16_t humidity)
{
    sht_frame[0] = temp >> 8; sht_frame[1] = temp & 255; sht_frame[2] = crc(sht_frame);
    sht_frame[3] = humidity >> 8; sht_frame[4] = humidity & 255; sht_frame[5] = crc(sht_frame + 3);
}
static void encode_bmp(uint32_t p, uint32_t t)
{
    bmp_frame[0] = p >> 12; bmp_frame[1] = p >> 4; bmp_frame[2] = (p & 15) << 4;
    bmp_frame[3] = t >> 12; bmp_frame[4] = t >> 4; bmp_frame[5] = (t & 15) << 4;
}
int main(void)
{
    MQ135_Data_t gas; Anemometer_Data_t wind; SHT30_Data_t sht = {0}; BMP280_Data_t bmp = {0};
    unsigned i;
    const int coefficients[12] = {27504,26435,-1000,36477,-10685,3024,2855,140,-7,15500,-14600,6000};
    MQ135_Init(); MQ135_UpdateVoltage(1); gas = MQ135_GetData();
    CHECK(gas.status == MQ135_STATUS_PREHEATING && !gas.valid && gas.voltage == 1);
    tick = 59999; MQ135_UpdateVoltage(1); CHECK(!MQ135_IsReady());
    tick = 60000; MQ135_UpdateVoltage(1); gas = MQ135_GetData();
    CHECK(gas.valid && gas.status == MQ135_STATUS_READY && !gas.calibrated && gas.ppm == 0);
    MQ135_Calibrate(0); CHECK(!MQ135_GetData().calibrated);
    MQ135_Calibrate(4); MQ135_UpdateVoltage(1); gas = MQ135_GetData();
    CHECK(gas.calibrated && fabs(gas.ratio - 4) < 0.001 && gas.ppm == 0);
    MQ135_UpdateVoltage(0); CHECK(!MQ135_GetData().valid && MQ135_GetData().resistance == 0);
    MQ135_UpdateVoltage(3.3f); CHECK(!MQ135_GetData().valid);
    MQ135_UpdateVoltage(1); CHECK(MQ135_GetData().valid);
    Anemometer_Init(); Anemometer_UpdateVoltage(0.4f); wind = Anemometer_GetData();
    CHECK(wind.valid && wind.wind_speed == 0);
    Anemometer_UpdateVoltage(2); CHECK(fabs(Anemometer_GetData().wind_speed - 30) < 0.001);
    Anemometer_UpdateVoltage(2.1f); CHECK(!Anemometer_GetData().valid);
    Anemometer_Calibrate(1,1,0,100); /* 非法校准不能破坏当前转换 */
    Anemometer_UpdateVoltage(1.2f); CHECK(fabs(Anemometer_GetData().wind_speed - 15) < 0.001);
    CHECK(SHT30_Init()); encode_sht(26214,32768);
    CHECK(SHT30_BeginRead()); tick += 20;
    CHECK(SHT30_FinishRead(&sht) && fabs(sht.temperature - 25) < 0.001 && fabs(sht.humidity - 50) < 0.01);
    sht_frame[2] ^= 1; CHECK(!SHT30_FinishRead(&sht) && !sht.valid);
    encode_sht(26214,32768); sht_frame[5] ^= 1; CHECK(!SHT30_FinishRead(&sht) && !sht.valid);
    encode_sht(0,0); CHECK(!SHT30_FinishRead(&sht) && !sht.valid);
    encode_sht(26214,32768); read_ok = false; CHECK(!SHT30_FinishRead(&sht) && !sht.valid);
    write_ok = false; CHECK(!SHT30_Read(&sht) && !sht.valid); write_ok = read_ok = true;
    CHECK(!SHT30_FinishRead(NULL));
    CHECK(!BMP280_Read(&bmp) && !bmp.valid);
    for (i=0; i<12; ++i) { calibration[2*i] = coefficients[i] & 255; calibration[2*i+1] = ((uint16_t)coefficients[i]) >> 8; }
    encode_bmp(415148,519888); CHECK(BMP280_Init() && BMP280_IsInitialized());
    CHECK(BMP280_Read(&bmp) && fabs(bmp.temperature - 25.082) < 0.01 && fabs(bmp.pressure - 100653.27) < 1);
    read_ok = false; CHECK(!BMP280_Read(&bmp) && !bmp.valid && !BMP280_IsInitialized());
    read_ok = true; CHECK(!BMP280_Read(&bmp)); CHECK(BMP280_Init());
    encode_bmp(0x80000,519888); CHECK(!BMP280_Read(&bmp) && !bmp.valid);
    encode_bmp(415148,0x80000); CHECK(!BMP280_Read(&bmp) && !bmp.valid);
    calibration[6] = calibration[7] = 0; CHECK(!BMP280_Init() && !BMP280_IsInitialized());
    chip_id = 0x60; CHECK(!BMP280_Init()); CHECK(!BMP280_Read(NULL));
    printf("PASS: %u sensor driver checks\n",checks); return 0;
}
