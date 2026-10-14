/**
 * @file    app_main.h
 * @brief   应用主程序头文件
 */

#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>
#include <stdbool.h>

/* 版本信息 */
#define APP_VERSION_MAJOR    0
#define APP_VERSION_MINOR    1
#define APP_VERSION_PATCH    0

/* 函数声明 */
/* 必需外设初始化失败返回 false；OLED/I2C 传感器允许降级启动。 */
bool App_Init(void);
void App_Start(void);

#endif /* APP_MAIN_H */
