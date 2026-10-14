#include "bsp_watchdog.h"
#include "main.h"
static IWDG_HandleTypeDef watchdog;
bool Bsp_Watchdog_Start(void)
{
    watchdog.Instance = IWDG;
    watchdog.Init.Prescaler = IWDG_PRESCALER_128;
    watchdog.Init.Reload = 1999; /* 32 kHz LSI 时约 8 s；为单 Bank 扇区擦除留余量。 */
    return HAL_IWDG_Init(&watchdog) == HAL_OK;
}
bool Bsp_Watchdog_Refresh(void) { return HAL_IWDG_Refresh(&watchdog) == HAL_OK; }
