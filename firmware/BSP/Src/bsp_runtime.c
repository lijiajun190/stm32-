#include "bsp_runtime.h"
#include "main.h"
void Bsp_Runtime_Init(void)
{
    RCC_ClkInitTypeDef clock; uint32_t latency;
    HAL_RCC_GetClockConfig(&clock, &latency);
    uint32_t frequency = HAL_RCC_GetPCLK1Freq();
    if (clock.APB1CLKDivider != RCC_HCLK_DIV1) frequency *= 2U;
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = 0; TIM2->PSC = frequency / 1000000U - 1U;
    TIM2->ARR = 0xFFFFFFFFU; TIM2->EGR = TIM_EGR_UG; TIM2->CNT = 0;
    TIM2->CR1 = TIM_CR1_CEN;
}
uint32_t Bsp_Runtime_Micros(void) { return TIM2->CNT; }
