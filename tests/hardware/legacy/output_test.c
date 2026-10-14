/**
 ******************************************************************************
 * @file           : output_test.c
 * @brief          : 蜂鸣器和LED硬件验证测试
 * @author         : STM32 Project Team
 * @date           : 2024-09-14
 ******************************************************************************
 * @attention
 *
 * 用于阶段三独立硬件驱动开发的验证测试
 *
 * 硬件接线：
 * - 蜂鸣器：PC6
 * - LED红色：PB12
 * - LED绿色：PB14
 * - LED蓝色：PC13（板载）
 *
 ******************************************************************************
 */

#include "output_test.h"
#include "main.h"
#include <stdio.h>

/**
 * @brief 测试蜂鸣器
 */
void Test_Buzzer(void)
{
    printf("\n=== 蜂鸣器测试 ===\n");
    printf("测试开始：蜂鸣器将响3次...\n");

    for(int i = 0; i < 3; i++) {
        printf("第 %d 次蜂鸣\n", i + 1);
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        HAL_Delay(200);
    }

    printf("蜂鸣器测试完成！\n");
}

/**
 * @brief 测试单个LED
 */
void Test_LED_Single(void)
{
    printf("\n=== 单个LED测试 ===\n");

    // 红色LED测试
    printf("测试红色LED (PB12)...\n");
    for(int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
        HAL_Delay(300);
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
        HAL_Delay(300);
    }

    // 绿色LED测试
    printf("测试绿色LED (PB14)...\n");
    for(int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
        HAL_Delay(300);
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
        HAL_Delay(300);
    }

    // 蓝色LED测试（板载）
    printf("测试蓝色LED (PC13 - 板载)...\n");
    for(int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
        HAL_Delay(300);
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
        HAL_Delay(300);
    }

    printf("单个LED测试完成！\n");
}

/**
 * @brief 测试LED混合颜色
 */
void Test_LED_Colors(void)
{
    printf("\n=== LED混合颜色测试 ===\n");

    // 关闭所有LED
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
    HAL_Delay(500);

    // 红色
    printf("红色\n");
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);

    // 黄色（红+绿）
    printf("黄色 (红+绿)\n");
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);

    // 绿色
    printf("绿色\n");
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);

    // 青色（绿+蓝）
    printf("青色 (绿+蓝)\n");
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);

    // 蓝色
    printf("蓝色\n");
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);

    // 品红（红+蓝）
    printf("品红 (红+蓝)\n");
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);

    // 白色（全亮）
    printf("白色 (全亮)\n");
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    HAL_Delay(800);
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);

    printf("混合颜色测试完成！\n");
}

/**
 * @brief LED流水灯效果
 */
void Test_LED_Running(void)
{
    printf("\n=== LED流水灯测试 ===\n");
    printf("流水灯运行10次...\n");

    for(int cycle = 0; cycle < 10; cycle++) {
        // 红 -> 绿 -> 蓝
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
        HAL_Delay(150);
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);

        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
        HAL_Delay(150);
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);

        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
        HAL_Delay(150);
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
    }

    printf("流水灯测试完成！\n");
}

/**
 * @brief LED和蜂鸣器组合测试
 */
void Test_Combined(void)
{
    printf("\n=== 组合测试 ===\n");
    printf("LED亮起时蜂鸣器响...\n");

    // 红灯+短促蜂鸣
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    HAL_Delay(400);
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);

    // 绿灯+中等蜂鸣
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);

    // 蓝灯+长蜂鸣
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);

    printf("组合测试完成！\n");
}

/**
 * @brief 完整输出设备测试套件
 */
void Test_Output_All(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║   蜂鸣器和LED硬件验证测试 - 阶段三      ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");
    printf("硬件连接检查：\n");
    printf("  ✓ 蜂鸣器   : PC6\n");
    printf("  ✓ LED红色  : PB12\n");
    printf("  ✓ LED绿色  : PB14\n");
    printf("  ✓ LED蓝色  : PC13 (板载)\n");
    printf("\n");

    // 测试序列
    Test_Buzzer();
    HAL_Delay(1000);

    Test_LED_Single();
    HAL_Delay(1000);

    Test_LED_Colors();
    HAL_Delay(1000);

    Test_LED_Running();
    HAL_Delay(1000);

    Test_Combined();

    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║          所有测试完成！                  ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");
}
