/**
 * @file    app_main.c
 * @brief   应用主程序实现
 */

#include "app_main.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "bsp_i2c.h"
#include "bsp_pwm.h"
#include "bsp_timer.h"
#include "oled.h"
#include "button.h"
#include "encoder.h"
#include "motor.h"
#include "state_machine.h"
#include "i2c.h"
#include "tim.h"
#include "mq135.h"
#include "anemometer.h"
#include "sht30.h"
#include "bmp280.h"
#include "led.h"
#include "buzzer.h"
#include <stdio.h>

/**
 * @brief  应用初始化
 */
bool App_Init(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  STM32 Smart Range Hood Controller\r\n");
    printf("  Version: %d.%d.%d\r\n",
           APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    printf("========================================\r\n");

    /* 初始化 BSP 层；CubeMX 外设句柄已在 main.c 中完成初始化。 */
    printf("[BSP] Initializing...\r\n");
    Bsp_GPIO_Init();
    LED_Init();
    Buzzer_Init();
    Bsp_UART_Init();
    Bsp_ADC_Init();
    Bsp_I2C_Init();
    Bsp_PWM_Init();
    Bsp_Timer_Init();
    /* 上电始终禁止电机；用户选择模式并确认启动后才运行。 */
    Motor_Init();
    printf("[BSP] Init complete\r\n");

    /* OLED 初始化失败只记录状态，不阻塞主程序，便于无显示屏调试。 */
    OLED_Status_t oled_status = OLED_Init(&hi2c2);
    printf("[OLED] Init %s (status=%d)\r\n",
           oled_status == OLED_OK ? "success" : "failed", oled_status);
    if (oled_status == OLED_OK) {
        OLED_Clear();
        OLED_ShowString(0, 0, "Smart Range Hood", OLED_FONT_6X8);
        OLED_ShowString(0, 2, "Initializing...", OLED_FONT_6X8);
        printf("[OLED] Boot display status=%d\r\n", OLED_Refresh());
    }

    /* 初始化输入、执行器和系统状态机。 */
    Button_Init();
    Encoder_Status_t encoder_status = Encoder_Init(&htim4);
    printf("[INPUT] Buttons ready, encoder status=%d\r\n", encoder_status);
    StateMachine_Init();

    /* 启动前探测传感器；SensorTask 负责周期读取、滤波和断线恢复。 */
    MQ135_Init();
    Anemometer_Init();
    bool sht30_ready = SHT30_Init();
    bool bmp280_ready = BMP280_Init();
    printf("[SENSOR] MQ135 preheating, anemometer ready, SHT30=%u BMP280=%u\r\n",
           (unsigned)sht30_ready, (unsigned)bmp280_ready);

    /* TIM3 捕获中断在调度器启动前开启，首个脉冲到来后即可测速。 */
    HAL_StatusTypeDef speed_status = Bsp_Timer_SpeedCapture_Start();
    Bsp_SpeedInfo_t speed;
    Bsp_Timer_SpeedCapture_GetInfo(&speed);
    printf("[SPEED] Capture status=%d tick=%luHz ppr=%lu timeout=%lums\r\n",
           speed_status, (unsigned long)speed.timer_tick_hz,
           (unsigned long)BSP_SPEED_PULSES_PER_REV,
           (unsigned long)BSP_SPEED_SIGNAL_TIMEOUT_MS);

    // 启动 ADC DMA
    HAL_StatusTypeDef adc_status = Bsp_ADC_StartDMA();
    printf("[ADC] DMA status=%d\r\n", adc_status);

    if (encoder_status != ENCODER_OK || speed_status != HAL_OK || adc_status != HAL_OK) {
        Motor_Enable(false);
        Bsp_Timer_SpeedCapture_Stop();
        Bsp_ADC_StopDMA();
        printf("[APP] Required peripheral initialization failed\r\n");
        return false;
    }

    StateMachine_ProcessEvent(EVENT_POWER_ON);
    if (oled_status == OLED_OK) {
        OLED_Clear();
        OLED_ShowString(0, 0, "Smart Range Hood", OLED_FONT_6X8);
        OLED_ShowString(0, 2, "System Ready", OLED_FONT_6X8);
        OLED_ShowString(0, 4, "Motor Disabled", OLED_FONT_6X8);
        printf("[OLED] Ready display status=%d\r\n", OLED_Refresh());
    }

    printf("[APP] Init complete\r\n");
    return true;
}

/**
 * @brief  启动应用
 */
void App_Start(void)
{
    printf("[APP] Starting FreeRTOS scheduler...\r\n");
    // FreeRTOS 调度器由 main.c 启动
}
