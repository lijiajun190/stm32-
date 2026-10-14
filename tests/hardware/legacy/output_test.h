/**
 ******************************************************************************
 * @file           : output_test.h
 * @brief          : 蜂鸣器和LED硬件验证测试头文件
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 */

#ifndef __OUTPUT_TEST_H
#define __OUTPUT_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 测试蜂鸣器
 * @note 蜂鸣器响3次，每次200ms
 */
void Test_Buzzer(void);

/**
 * @brief 测试单个LED
 * @note 依次测试红、绿、蓝LED，每个闪烁3次
 */
void Test_LED_Single(void);

/**
 * @brief 测试LED混合颜色
 * @note 依次显示：红、黄、绿、青、蓝、品红、白
 */
void Test_LED_Colors(void);

/**
 * @brief LED流水灯效果
 * @note 红->绿->蓝循环10次
 */
void Test_LED_Running(void);

/**
 * @brief LED和蜂鸣器组合测试
 * @note LED亮起时蜂鸣器响，测试协同工作
 */
void Test_Combined(void);

/**
 * @brief 完整输出设备测试套件
 * @note 运行所有测试项目，适合用于硬件验证
 * @usage 在main函数中调用：Test_Output_All();
 */
void Test_Output_All(void);

#ifdef __cplusplus
}
#endif

#endif /* __OUTPUT_TEST_H */
