/* 主机测试使用的 HAL 替身，只模拟本阶段用到的定时器接口。 */
#ifndef TEST_STM32_HAL_H
#define TEST_STM32_HAL_H
#include <stdint.h>
#include <stddef.h>

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
/* GPIO 模拟保留每个引脚的模式/上下拉，可测试 CubeMX 初始化代码本身。 */
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
typedef struct {
    uint32_t IDR, ODR;
    uint32_t mode[16], pull[16];
} GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
extern GPIO_TypeDef test_gpioa, test_gpiob, test_gpioc;
#define GPIOA (&test_gpioa)
#define GPIOB (&test_gpiob)
#define GPIOC (&test_gpioc)
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_8 (1U << 8)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_14 (1U << 14)
#define GPIO_PIN_15 (1U << 15)
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_IT_RISING 2U
#define GPIO_MODE_IT_FALLING 3U
#define GPIO_PULLUP 1U
#define GPIO_PULLDOWN 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 2U
#define EXTI0_IRQn 6U
#define EXTI2_IRQn 8U
#define EXTI3_IRQn 9U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOH_CLK_ENABLE() ((void)0)
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, uint16_t);
void HAL_GPIO_TogglePin(GPIO_TypeDef *, uint16_t);
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void HAL_NVIC_SetPriority(uint32_t, uint32_t, uint32_t);
void HAL_NVIC_EnableIRQ(uint32_t);
void HAL_Delay(uint32_t);
typedef struct {
    uint32_t CNT, ARR, SR, DIER, CCR1, CCR3;
} TIM_TypeDef;
typedef struct { uint32_t Prescaler; } TIM_InitTypeDef;
typedef struct {
    TIM_TypeDef *Instance;
    TIM_InitTypeDef Init;
    uint32_t Channel;
} TIM_HandleTypeDef;
typedef struct { uint32_t APB1CLKDivider; } RCC_ClkInitTypeDef;
extern TIM_TypeDef test_tim1, test_tim3, test_tim4;
#define TIM1 (&test_tim1)
#define TIM3 (&test_tim3)
#define TIM4 (&test_tim4)
#define TIM_CHANNEL_1 0U
#define TIM_CHANNEL_3 8U
#define TIM_CHANNEL_ALL 0xFFFFU
#define HAL_TIM_ACTIVE_CHANNEL_1 1U
#define TIM_FLAG_UPDATE 1U
#define TIM_FLAG_CC1 2U
#define TIM_FLAG_CC1OF 512U
#define TIM_IT_UPDATE 1U
#define RCC_HCLK_DIV1 1U
#define RESET 0U
#define __HAL_TIM_GET_COUNTER(h) ((h)->Instance->CNT)
#define __HAL_TIM_SET_COUNTER(h,v) ((h)->Instance->CNT = (v))
#define __HAL_TIM_GET_AUTORELOAD(h) ((h)->Instance->ARR)
#define __HAL_TIM_GET_FLAG(h,f) ((h)->Instance->SR & (f))
#define __HAL_TIM_CLEAR_FLAG(h,f) ((h)->Instance->SR &= ~(f))
#define __HAL_TIM_DISABLE_IT(h,i) ((h)->Instance->DIER &= ~(i))
#define __HAL_TIM_ENABLE_IT(h,i) ((h)->Instance->DIER |= (i))
#define __HAL_TIM_SET_COMPARE(h,c,v) ((h)->Instance->CCR3 = (v))
extern uint32_t test_primask;
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
static inline void __disable_irq(void) { test_primask = 1U; }
static inline void __set_PRIMASK(uint32_t value) { test_primask = value; }
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *, uint32_t *);
uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIM_IC_Stop_IT(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIM_Encoder_Stop(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_TIMEx_PWMN_Stop(TIM_HandleTypeDef *, uint32_t);
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *);
#endif
