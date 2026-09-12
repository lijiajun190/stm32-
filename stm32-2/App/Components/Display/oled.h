/**
 ******************************************************************************
 * @file    oled.h
 * @brief   SSD1306 OLED驱动头文件（硬件I2C）
 * @author  油烟机项目组
 * @date    2026-09-12
 ******************************************************************************
 * @note    基于硬件I2C2实现，支持128x64分辨率OLED
 *          I2C地址：0x78（7位地址0x3C）
 *          I2C速度：400kHz快速模式
 ******************************************************************************
 */

#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* 配置参数 -----------------------------------------------------------------*/
#define OLED_I2C_ADDRESS    0x78        // SSD1306的I2C地址（8位格式）
#define OLED_I2C_TIMEOUT    100         // I2C超时时间（ms）

#define OLED_WIDTH          128         // 屏幕宽度（像素）
#define OLED_HEIGHT         64          // 屏幕高度（像素）
#define OLED_PAGE_NUM       8           // 页数（64/8=8）

/* 错误码定义 ---------------------------------------------------------------*/
typedef enum {
    OLED_OK = 0,                        // 成功
    OLED_ERROR_TIMEOUT,                 // I2C超时
    OLED_ERROR_BUSY,                    // I2C忙
    OLED_ERROR_PARAM,                   // 参数错误
    OLED_ERROR_NOT_INIT                 // 未初始化
} OLED_Status_t;

/* 显示模式 -----------------------------------------------------------------*/
typedef enum {
    OLED_COLOR_NORMAL = 0,              // 正常模式（黑底白字）
    OLED_COLOR_REVERSED                 // 反色模式（白底黑字）
} OLED_ColorMode_t;

/* 字体大小 -----------------------------------------------------------------*/
typedef enum {
    OLED_FONT_6X8 = 0,                  // 6x8字体
    OLED_FONT_8X16                      // 8x16字体
} OLED_FontSize_t;

/* API函数 ------------------------------------------------------------------*/

/**
 * @brief  初始化OLED
 * @param  hi2c: I2C句柄指针（使用hi2c2）
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  清空屏幕
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_Clear(void);

/**
 * @brief  全屏填充
 * @param  data: 填充数据（0x00=全黑，0xFF=全白）
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_Fill(uint8_t data);

/**
 * @brief  刷新显示缓冲区到OLED
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_Refresh(void);

/**
 * @brief  显示字符串
 * @param  x: X坐标（0-127）
 * @param  y: Y坐标，页地址（0-7）
 * @param  str: 字符串指针
 * @param  font: 字体大小
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_ShowString(uint8_t x, uint8_t y, const char *str, OLED_FontSize_t font);

/**
 * @brief  显示字符
 * @param  x: X坐标（0-127）
 * @param  y: Y坐标，页地址（0-7）
 * @param  ch: 字符
 * @param  font: 字体大小
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_ShowChar(uint8_t x, uint8_t y, char ch, OLED_FontSize_t font);

/**
 * @brief  显示数字
 * @param  x: X坐标（0-127）
 * @param  y: Y坐标，页地址（0-7）
 * @param  num: 数字
 * @param  len: 显示位数
 * @param  font: 字体大小
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, OLED_FontSize_t font);

/**
 * @brief  显示浮点数
 * @param  x: X坐标（0-127）
 * @param  y: Y坐标，页地址（0-7）
 * @param  num: 浮点数
 * @param  int_len: 整数部分位数
 * @param  frac_len: 小数部分位数
 * @param  font: 字体大小
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t frac_len, OLED_FontSize_t font);

/**
 * @brief  画点
 * @param  x: X坐标（0-127）
 * @param  y: Y坐标（0-63）
 * @param  on: true=点亮，false=熄灭
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_DrawPixel(uint8_t x, uint8_t y, bool on);

/**
 * @brief  画线
 * @param  x1: 起点X坐标
 * @param  y1: 起点Y坐标
 * @param  x2: 终点X坐标
 * @param  y2: 终点Y坐标
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/**
 * @brief  画矩形
 * @param  x: 左上角X坐标
 * @param  y: 左上角Y坐标
 * @param  w: 宽度
 * @param  h: 高度
 * @param  fill: true=填充，false=空心
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill);

/**
 * @brief  设置显示开关
 * @param  on: true=开启，false=关闭
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_DisplayOn(bool on);

/**
 * @brief  设置显示对比度
 * @param  contrast: 对比度值（0-255）
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_SetContrast(uint8_t contrast);

/**
 * @brief  设置颜色模式
 * @param  mode: 颜色模式
 * @retval OLED_Status_t 状态码
 */
OLED_Status_t OLED_SetColorMode(OLED_ColorMode_t mode);

/**
 * @brief  获取初始化状态
 * @retval true=已初始化，false=未初始化
 */
bool OLED_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
