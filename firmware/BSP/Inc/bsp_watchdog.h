#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H
#include <stdbool.h>
/* 只有 WatchdogTask 启动/喂狗；初始化自动启用独立 LSI。 */
bool Bsp_Watchdog_Start(void);
bool Bsp_Watchdog_Refresh(void);
#endif
