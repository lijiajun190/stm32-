/**
 ******************************************************************************
 * @file    oled_test.c
 * @brief   OLED驱动测试示例
 * @author  油烟机项目组
 * @date    2026-09-12
 ******************************************************************************
 * @note    在main.c中调用OLED_Test()函数进行测试
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "oled.h"
#include "i2c.h"
#include <stdio.h>

/* 外部变量 -----------------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c2;  // 在i2c.c中定义

/**
 * @brief  OLED基础功能测试
 * @note   在main.c的USER CODE BEGIN 2区域调用此函数
 */
void OLED_Test(void)
{
    OLED_Status_t status;

    printf("OLED Test Start...\r\n");

    // 1. 初始化OLED
    status = OLED_Init(&hi2c2);
    if (status == OLED_OK) {
        printf("OLED Init: OK\r\n");
    } else {
        printf("OLED Init: FAILED (Error: %d)\r\n", status);
        return;
    }

    HAL_Delay(500);

    // 2. 测试清屏
    printf("Test 1: Clear screen\r\n");
    OLED_Clear();
    OLED_Refresh();
    HAL_Delay(1000);

    // 3. 测试显示字符串
    printf("Test 2: Show string\r\n");
    OLED_ShowString(0, 0, "STM32F411", OLED_FONT_6X8);
    OLED_ShowString(0, 1, "OLED Test", OLED_FONT_6X8);
    OLED_Refresh();
    HAL_Delay(2000);

    // 4. 测试显示数字
    printf("Test 3: Show numbers\r\n");
    OLED_Clear();
    OLED_ShowString(0, 0, "Num:", OLED_FONT_6X8);
    OLED_ShowNum(30, 0, 12345, 5, OLED_FONT_6X8);
    OLED_Refresh();
    HAL_Delay(2000);

    // 5. 测试显示浮点数
    printf("Test 4: Show float\r\n");
    OLED_Clear();
    OLED_ShowString(0, 0, "Temp:", OLED_FONT_6X8);
    OLED_ShowFloat(36, 0, 25.6, 2, 1, OLED_FONT_6X8);
    OLED_ShowString(60, 0, "C", OLED_FONT_6X8);
    OLED_Refresh();
    HAL_Delay(2000);

    // 6. 测试画线
    printf("Test 5: Draw lines\r\n");
    OLED_Clear();
    OLED_DrawLine(0, 0, 127, 63);
    OLED_DrawLine(127, 0, 0, 63);
    OLED_Refresh();
    HAL_Delay(2000);

    // 7. 测试画矩形
    printf("Test 6: Draw rectangles\r\n");
    OLED_Clear();
    OLED_DrawRect(10, 10, 50, 30, false);  // 空心矩形
    OLED_DrawRect(70, 10, 50, 30, true);   // 填充矩形
    OLED_Refresh();
    HAL_Delay(2000);

    // 8. 测试综合显示
    printf("Test 7: System info display\r\n");
    OLED_Clear();
    OLED_ShowString(0, 0, "System Ready", OLED_FONT_6X8);
    OLED_ShowString(0, 2, "Mode: Manual", OLED_FONT_6X8);
    OLED_ShowString(0, 3, "RPM: 1200", OLED_FONT_6X8);
    OLED_ShowString(0, 5, "T:25C H:60%", OLED_FONT_6X8);
    OLED_Refresh();
    HAL_Delay(3000);

    printf("OLED Test Complete!\r\n");
}

/**
 * @brief  OLED动态刷新测试（计数器）
 * @note   可以在FreeRTOS任务中循环调用
 */
void OLED_DynamicTest(void)
{
    static uint32_t counter = 0;

    OLED_Clear();
    OLED_ShowString(0, 0, "Counter:", OLED_FONT_6X8);
    OLED_ShowNum(54, 0, counter++, 5, OLED_FONT_6X8);

    OLED_ShowString(0, 2, "System OK", OLED_FONT_6X8);
    OLED_Refresh();
}

/**
 * @brief  传感器数据显示示例
 * @param  smoke: 油烟浓度
 * @param  temp: 温度
 * @param  hum: 湿度
 * @param  rpm: 转速
 */
void OLED_ShowSensorData(float smoke, float temp, float hum, uint16_t rpm)
{
    OLED_Clear();

    // 第0行：标题
    OLED_ShowString(20, 0, "System Status", OLED_FONT_6X8);

    // 第2行：油烟浓度
    OLED_ShowString(0, 2, "Smoke:", OLED_FONT_6X8);
    OLED_ShowFloat(42, 2, smoke, 3, 1, OLED_FONT_6X8);

    // 第3行：温湿度
    OLED_ShowString(0, 3, "T:", OLED_FONT_6X8);
    OLED_ShowFloat(18, 3, temp, 2, 1, OLED_FONT_6X8);
    OLED_ShowString(48, 3, "H:", OLED_FONT_6X8);
    OLED_ShowFloat(66, 3, hum, 2, 1, OLED_FONT_6X8);

    // 第5行：转速
    OLED_ShowString(0, 5, "RPM:", OLED_FONT_6X8);
    OLED_ShowNum(30, 5, rpm, 4, OLED_FONT_6X8);

    OLED_Refresh();
}

/**
 * @brief  模式显示示例
 * @param  mode: 模式名称字符串
 * @param  target_rpm: 目标转速
 * @param  actual_rpm: 实际转速
 */
void OLED_ShowModeInfo(const char *mode, uint16_t target_rpm, uint16_t actual_rpm)
{
    OLED_Clear();

    // 第0行：模式
    OLED_ShowString(0, 0, "Mode:", OLED_FONT_6X8);
    OLED_ShowString(36, 0, mode, OLED_FONT_6X8);

    // 第2行：目标转速
    OLED_ShowString(0, 2, "Target:", OLED_FONT_6X8);
    OLED_ShowNum(48, 2, target_rpm, 4, OLED_FONT_6X8);

    // 第3行：实际转速
    OLED_ShowString(0, 3, "Actual:", OLED_FONT_6X8);
    OLED_ShowNum(48, 3, actual_rpm, 4, OLED_FONT_6X8);

    // 计算误差
    int16_t error = actual_rpm - target_rpm;
    OLED_ShowString(0, 5, "Error:", OLED_FONT_6X8);
    if (error >= 0) {
        OLED_ShowString(42, 5, "+", OLED_FONT_6X8);
        OLED_ShowNum(48, 5, error, 4, OLED_FONT_6X8);
    } else {
        OLED_ShowString(42, 5, "-", OLED_FONT_6X8);
        OLED_ShowNum(48, 5, -error, 4, OLED_FONT_6X8);
    }

    OLED_Refresh();
}

/**
 * @brief  错误信息显示
 * @param  error_msg: 错误信息字符串
 */
void OLED_ShowError(const char *error_msg)
{
    OLED_Clear();

    // 第1行：ERROR标题
    OLED_ShowString(30, 1, "!! ERROR !!", OLED_FONT_6X8);

    // 第3行：错误信息
    OLED_ShowString(0, 3, error_msg, OLED_FONT_6X8);

    // 第6行：提示
    OLED_ShowString(10, 6, "Check System", OLED_FONT_6X8);

    OLED_Refresh();
}
