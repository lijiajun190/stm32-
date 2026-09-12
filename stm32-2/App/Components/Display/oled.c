/**
 ******************************************************************************
 * @file    oled.c
 * @brief   SSD1306 OLED驱动实现（硬件I2C）
 * @author  油烟机项目组
 * @date    2026-09-12
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "oled.h"
#include <string.h>
#include <stdio.h>

/* 私有宏定义 ---------------------------------------------------------------*/
#define OLED_CMD        0x00    // 命令模式
#define OLED_DATA       0x40    // 数据模式

/* SSD1306命令定义 ----------------------------------------------------------*/
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_DISPLAY_ALL_ON      0xA5
#define SSD1306_CMD_DISPLAY_NORMAL      0xA6
#define SSD1306_CMD_DISPLAY_INVERSE     0xA7
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_MEM_ADDR_MODE   0x20
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22

/* 私有变量 -----------------------------------------------------------------*/
static I2C_HandleTypeDef *g_hi2c = NULL;
static uint8_t g_oled_gram[OLED_PAGE_NUM][OLED_WIDTH];  // 显示缓冲区
static bool g_oled_initialized = false;

/* 字体数据 -----------------------------------------------------------------*/
// 6x8 ASCII字体（简化版，只包含常用字符）
static const uint8_t OLED_Font_6x8[][6] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00, 0x00}, // '!' (33)
    {0x00, 0x07, 0x00, 0x07, 0x00, 0x00}, // '"' (34)
    {0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00}, // '#' (35)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00}, // '$' (36)
    {0x23, 0x13, 0x08, 0x64, 0x62, 0x00}, // '%' (37)
    {0x36, 0x49, 0x56, 0x20, 0x50, 0x00}, // '&' (38)
    {0x00, 0x08, 0x07, 0x03, 0x00, 0x00}, // ''' (39)
    {0x00, 0x1C, 0x22, 0x41, 0x00, 0x00}, // '(' (40)
    {0x00, 0x41, 0x22, 0x1C, 0x00, 0x00}, // ')' (41)
    {0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00}, // '*' (42)
    {0x08, 0x08, 0x3E, 0x08, 0x08, 0x00}, // '+' (43)
    {0x00, 0x80, 0x70, 0x30, 0x00, 0x00}, // ',' (44)
    {0x08, 0x08, 0x08, 0x08, 0x08, 0x00}, // '-' (45)
    {0x00, 0x00, 0x60, 0x60, 0x00, 0x00}, // '.' (46)
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x00}, // '/' (47)
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00}, // '0' (48)
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00}, // '1' (49)
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00}, // '2' (50)
    {0x21, 0x41, 0x45, 0x4B, 0x31, 0x00}, // '3' (51)
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00}, // '4' (52)
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00}, // '5' (53)
    {0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00}, // '6' (54)
    {0x01, 0x71, 0x09, 0x05, 0x03, 0x00}, // '7' (55)
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00}, // '8' (56)
    {0x06, 0x49, 0x49, 0x29, 0x1E, 0x00}, // '9' (57)
    {0x00, 0x00, 0x14, 0x00, 0x00, 0x00}, // ':' (58)
    {0x00, 0x40, 0x34, 0x00, 0x00, 0x00}, // ';' (59)
    {0x00, 0x08, 0x14, 0x22, 0x41, 0x00}, // '<' (60)
    {0x14, 0x14, 0x14, 0x14, 0x14, 0x00}, // '=' (61)
    {0x00, 0x41, 0x22, 0x14, 0x08, 0x00}, // '>' (62)
    {0x02, 0x01, 0x51, 0x09, 0x06, 0x00}, // '?' (63)
    {0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00}, // '@' (64)
    {0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00}, // 'A' (65)
    {0x7F, 0x49, 0x49, 0x49, 0x36, 0x00}, // 'B' (66)
    {0x3E, 0x41, 0x41, 0x41, 0x22, 0x00}, // 'C' (67)
    {0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00}, // 'D' (68)
    {0x7F, 0x49, 0x49, 0x49, 0x41, 0x00}, // 'E' (69)
    {0x7F, 0x09, 0x09, 0x09, 0x01, 0x00}, // 'F' (70)
    {0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00}, // 'G' (71)
    {0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00}, // 'H' (72)
    {0x00, 0x41, 0x7F, 0x41, 0x00, 0x00}, // 'I' (73)
    {0x20, 0x40, 0x41, 0x3F, 0x01, 0x00}, // 'J' (74)
    {0x7F, 0x08, 0x14, 0x22, 0x41, 0x00}, // 'K' (75)
    {0x7F, 0x40, 0x40, 0x40, 0x40, 0x00}, // 'L' (76)
    {0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x00}, // 'M' (77)
    {0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00}, // 'N' (78)
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00}, // 'O' (79)
    {0x7F, 0x09, 0x09, 0x09, 0x06, 0x00}, // 'P' (80)
    {0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00}, // 'Q' (81)
    {0x7F, 0x09, 0x19, 0x29, 0x46, 0x00}, // 'R' (82)
    {0x46, 0x49, 0x49, 0x49, 0x31, 0x00}, // 'S' (83)
    {0x01, 0x01, 0x7F, 0x01, 0x01, 0x00}, // 'T' (84)
    {0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00}, // 'U' (85)
    {0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00}, // 'V' (86)
    {0x3F, 0x40, 0x30, 0x40, 0x3F, 0x00}, // 'W' (87)
    {0x63, 0x14, 0x08, 0x14, 0x63, 0x00}, // 'X' (88)
    {0x07, 0x08, 0x70, 0x08, 0x07, 0x00}, // 'Y' (89)
    {0x61, 0x51, 0x49, 0x45, 0x43, 0x00}, // 'Z' (90)
};

/* 私有函数声明 -------------------------------------------------------------*/
static OLED_Status_t OLED_WriteCmd(uint8_t cmd);
static OLED_Status_t OLED_WriteData(uint8_t *data, uint16_t len);
static HAL_StatusTypeDef OLED_I2C_Write(uint8_t reg, uint8_t *data, uint16_t len);

/* 私有函数实现 -------------------------------------------------------------*/

/**
 * @brief  I2C写操作封装
 */
static HAL_StatusTypeDef OLED_I2C_Write(uint8_t reg, uint8_t *data, uint16_t len)
{
    if (g_hi2c == NULL) {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status;
    uint8_t buf[128];

    // 第一个字节是控制字节（OLED_CMD或OLED_DATA）
    buf[0] = reg;
    memcpy(buf + 1, data, len);

    status = HAL_I2C_Master_Transmit(g_hi2c, OLED_I2C_ADDRESS, buf, len + 1, OLED_I2C_TIMEOUT);

    return status;
}

/**
 * @brief  写命令到OLED
 */
static OLED_Status_t OLED_WriteCmd(uint8_t cmd)
{
    HAL_StatusTypeDef status = OLED_I2C_Write(OLED_CMD, &cmd, 1);

    if (status == HAL_OK) {
        return OLED_OK;
    } else if (status == HAL_TIMEOUT) {
        return OLED_ERROR_TIMEOUT;
    } else if (status == HAL_BUSY) {
        return OLED_ERROR_BUSY;
    }
    return OLED_ERROR_TIMEOUT;
}

/**
 * @brief  写数据到OLED
 */
static OLED_Status_t OLED_WriteData(uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status = OLED_I2C_Write(OLED_DATA, data, len);

    if (status == HAL_OK) {
        return OLED_OK;
    } else if (status == HAL_TIMEOUT) {
        return OLED_ERROR_TIMEOUT;
    } else if (status == HAL_BUSY) {
        return OLED_ERROR_BUSY;
    }
    return OLED_ERROR_TIMEOUT;
}

/* 公共API实现 --------------------------------------------------------------*/

/**
 * @brief  初始化OLED
 */
OLED_Status_t OLED_Init(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) {
        return OLED_ERROR_PARAM;
    }

    g_hi2c = hi2c;

    // 等待OLED上电稳定
    HAL_Delay(100);

    // 初始化序列
    OLED_WriteCmd(0xAE); // 关闭显示

    OLED_WriteCmd(0x00); // 设置列地址低4位
    OLED_WriteCmd(0x10); // 设置列地址高4位
    OLED_WriteCmd(0x40); // 设置起始行地址（0-63）

    OLED_WriteCmd(0xB0); // 设置页地址（0-7）

    OLED_WriteCmd(0x81); // 对比度设置
    OLED_WriteCmd(0xFF); // 对比度值（0-255）

    OLED_WriteCmd(0xA1); // 段重映射（A0=正常，A1=左右翻转）
    OLED_WriteCmd(0xA6); // 正常显示（A6=正常，A7=反色）

    OLED_WriteCmd(0xA8); // 设置复用率
    OLED_WriteCmd(0x3F); // 1/64 duty

    OLED_WriteCmd(0xC8); // COM扫描方向（C0=正常，C8=上下翻转）

    OLED_WriteCmd(0xD3); // 设置显示偏移
    OLED_WriteCmd(0x00); // 无偏移

    OLED_WriteCmd(0xD5); // 设置时钟分频
    OLED_WriteCmd(0x80); // 默认分频比

    OLED_WriteCmd(0xD8); // 设置区域颜色模式
    OLED_WriteCmd(0x05); // 单色模式

    OLED_WriteCmd(0xD9); // 设置预充电周期
    OLED_WriteCmd(0xF1); // 预充电15个时钟，放电1个时钟

    OLED_WriteCmd(0xDA); // 设置COM引脚配置
    OLED_WriteCmd(0x12); // 交替配置

    OLED_WriteCmd(0xDB); // 设置VCOMH电压
    OLED_WriteCmd(0x30); // 0.83*Vcc

    OLED_WriteCmd(0x8D); // 电荷泵设置
    OLED_WriteCmd(0x14); // 使能电荷泵

    OLED_WriteCmd(0xAF); // 开启显示

    // 清空显示缓冲区
    OLED_Clear();

    g_oled_initialized = true;

    return OLED_OK;
}

/**
 * @brief  清空屏幕
 */
OLED_Status_t OLED_Clear(void)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    memset(g_oled_gram, 0x00, sizeof(g_oled_gram));
    return OLED_Refresh();
}

/**
 * @brief  全屏填充
 */
OLED_Status_t OLED_Fill(uint8_t data)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    memset(g_oled_gram, data, sizeof(g_oled_gram));
    return OLED_Refresh();
}

/**
 * @brief  刷新显示缓冲区到OLED
 */
OLED_Status_t OLED_Refresh(void)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    OLED_Status_t status;

    for (uint8_t page = 0; page < OLED_PAGE_NUM; page++) {
        // 设置页地址
        status = OLED_WriteCmd(0xB0 + page);
        if (status != OLED_OK) {
            return status;
        }

        // 设置列起始地址
        status = OLED_WriteCmd(0x00); // 列地址低4位
        if (status != OLED_OK) {
            return status;
        }

        status = OLED_WriteCmd(0x10); // 列地址高4位
        if (status != OLED_OK) {
            return status;
        }

        // 写入一页数据
        status = OLED_WriteData(g_oled_gram[page], OLED_WIDTH);
        if (status != OLED_OK) {
            return status;
        }
    }

    return OLED_OK;
}

/**
 * @brief  显示字符
 */
OLED_Status_t OLED_ShowChar(uint8_t x, uint8_t y, char ch, OLED_FontSize_t font)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    if (x >= OLED_WIDTH || y >= OLED_PAGE_NUM) {
        return OLED_ERROR_PARAM;
    }

    uint8_t index = ch - ' ';
    if (index > 90) { // 超出字符范围
        index = 0;
    }

    // 6x8字体
    if (font == OLED_FONT_6X8) {
        if (x + 6 > OLED_WIDTH) {
            return OLED_ERROR_PARAM;
        }

        for (uint8_t i = 0; i < 6; i++) {
            g_oled_gram[y][x + i] = OLED_Font_6x8[index][i];
        }
    }

    return OLED_OK;
}

/**
 * @brief  显示字符串
 */
OLED_Status_t OLED_ShowString(uint8_t x, uint8_t y, const char *str, OLED_FontSize_t font)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    if (str == NULL) {
        return OLED_ERROR_PARAM;
    }

    uint8_t x_pos = x;
    OLED_Status_t status;

    while (*str != '\0') {
        if (x_pos >= OLED_WIDTH) {
            break;
        }

        status = OLED_ShowChar(x_pos, y, *str, font);
        if (status != OLED_OK) {
            return status;
        }

        x_pos += 6; // 6x8字体宽度
        str++;
    }

    return OLED_OK;
}

/**
 * @brief  显示数字
 */
OLED_Status_t OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, OLED_FontSize_t font)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    char buf[12];
    snprintf(buf, sizeof(buf), "%0*lu", len, num);

    return OLED_ShowString(x, y, buf, font);
}

/**
 * @brief  显示浮点数
 */
OLED_Status_t OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t frac_len, OLED_FontSize_t font)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%*.*f", int_len + frac_len + 1, frac_len, num);

    return OLED_ShowString(x, y, buf, font);
}

/**
 * @brief  画点
 */
OLED_Status_t OLED_DrawPixel(uint8_t x, uint8_t y, bool on)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return OLED_ERROR_PARAM;
    }

    uint8_t page = y / 8;
    uint8_t bit = y % 8;

    if (on) {
        g_oled_gram[page][x] |= (1 << bit);
    } else {
        g_oled_gram[page][x] &= ~(1 << bit);
    }

    return OLED_OK;
}

/**
 * @brief  画线（Bresenham算法）
 */
OLED_Status_t OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    int16_t dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int16_t dy = y2 > y1 ? y2 - y1 : y1 - y2;
    int16_t sx = x1 < x2 ? 1 : -1;
    int16_t sy = y1 < y2 ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        OLED_DrawPixel(x1, y1, true);

        if (x1 == x2 && y1 == y2) {
            break;
        }

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }

    return OLED_OK;
}

/**
 * @brief  画矩形
 */
OLED_Status_t OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill)
{
    if (fill) {
        for (uint8_t i = 0; i < h; i++) {
            for (uint8_t j = 0; j < w; j++) {
                OLED_DrawPixel(x + j, y + i, true);
            }
        }
    } else {
        OLED_DrawLine(x, y, x + w - 1, y);
        OLED_DrawLine(x + w - 1, y, x + w - 1, y + h - 1);
        OLED_DrawLine(x + w - 1, y + h - 1, x, y + h - 1);
        OLED_DrawLine(x, y + h - 1, x, y);
    }

    return OLED_OK;
}

/**
 * @brief  设置显示开关
 */
OLED_Status_t OLED_DisplayOn(bool on)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    return OLED_WriteCmd(on ? 0xAF : 0xAE);
}

/**
 * @brief  设置显示对比度
 */
OLED_Status_t OLED_SetContrast(uint8_t contrast)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    OLED_WriteCmd(0x81);
    return OLED_WriteCmd(contrast);
}

/**
 * @brief  设置颜色模式
 */
OLED_Status_t OLED_SetColorMode(OLED_ColorMode_t mode)
{
    if (!g_oled_initialized) {
        return OLED_ERROR_NOT_INIT;
    }

    return OLED_WriteCmd(mode == OLED_COLOR_NORMAL ? 0xA6 : 0xA7);
}

/**
 * @brief  获取初始化状态
 */
bool OLED_IsInitialized(void)
{
    return g_oled_initialized;
}
