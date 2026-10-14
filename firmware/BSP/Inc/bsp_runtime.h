/** TIM2 专用 1 MHz 监测时钟，不占用测速、PWM、编码器定时器。 */
#ifndef BSP_RUNTIME_H
#define BSP_RUNTIME_H
#include <stdint.h>
void Bsp_Runtime_Init(void);
uint32_t Bsp_Runtime_Micros(void);
#endif
