/* 测试直接链接正式 BSP 和 motor.c，HAL 替身提供可控制的时钟及捕获边沿。 */
#include "tim.h"
#include "bsp_timer.h"
#include "bsp_pwm.h"
#include "bsp_gpio.h"
#include "motor.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

TIM_TypeDef test_tim1, test_tim3, test_tim4;
TIM_HandleTypeDef htim1, htim3, htim4;
uint32_t test_primask;
static uint32_t now_ms, pclk1 = 50000000U, apb_divider = 2U;
static HAL_StatusTypeDef capture_start_status = HAL_OK, pwm_start_status = HAL_OK;
static unsigned checks, pwm_starts;
static int enabled;
#define CHECK(x) do { ++checks; if (!(x)) { \
    printf("FAIL at line %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)

uint32_t HAL_GetTick(void) { return now_ms; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return pclk1; }
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *c, uint32_t *l)
{ c->APB1CLKDivider = apb_divider; *l = 0U; }
uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *h, uint32_t c)
{ (void)c; return h->Instance->CCR1; }
HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *h, uint32_t c)
{ (void)h; (void)c; return capture_start_status; }
HAL_StatusTypeDef HAL_TIM_IC_Stop_IT(TIM_HandleTypeDef *h, uint32_t c)
{ (void)h; (void)c; return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *h, uint32_t c)
{ (void)h; (void)c; return HAL_OK; }
HAL_StatusTypeDef HAL_TIM_Encoder_Stop(TIM_HandleTypeDef *h, uint32_t c)
{ (void)h; (void)c; return HAL_OK; }
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *h, uint32_t c)
{ CHECK(h == &htim1 && c == TIM_CHANNEL_3); ++pwm_starts; return pwm_start_status; }
HAL_StatusTypeDef HAL_TIMEx_PWMN_Stop(TIM_HandleTypeDef *h, uint32_t c)
{ CHECK(h == &htim1 && c == TIM_CHANNEL_3); return HAL_OK; }
void Bsp_Motor_Enable(bool value) { enabled = value; }
void Bsp_Motor_SetDirection(bool forward) { CHECK(forward); }

static void pulse(uint32_t value, uint32_t tick)
{
    now_ms = tick;
    test_tim3.CNT = test_tim3.CCR1 = value;
    htim3.Channel = HAL_TIM_ACTIVE_CHANNEL_1;
    HAL_TIM_IC_CaptureCallback(&htim3);
}
static void overflow(void)
{
    test_tim3.SR &= ~TIM_FLAG_UPDATE;
    Bsp_Timer_SpeedCapture_Overflow();
}
static void restart(void)
{
    capture_start_status = HAL_OK;
    CHECK(Bsp_Timer_SpeedCapture_Start() == HAL_OK);
    CHECK(!Bsp_Timer_SpeedCapture_IsValid());
}

int main(void)
{
    Bsp_SpeedInfo_t info;
    unsigned i;
    htim1.Instance = TIM1; htim3.Instance = TIM3; htim4.Instance = TIM4;
    htim3.Init.Prescaler = 99U;
    test_tim1.ARR = 999U; test_tim3.ARR = test_tim4.ARR = 65535U;
    Bsp_Timer_Init();
    restart();
    /* 首脉冲无周期；第二次脉冲得到 20 ms 周期、50 Hz、3000/PPR RPM。 */
    pulse(100U, 0U);
    CHECK(!Bsp_Timer_SpeedCapture_IsValid());
    pulse(20100U, 20U);
    Bsp_Timer_SpeedCapture_GetInfo(&info);
    CHECK(info.valid && info.period_ticks == 20000U && info.timer_tick_hz == 1000000U);
    CHECK(info.rpm == 3000U / BSP_SPEED_PULSES_PER_REV && info.frequency_hz == 50.0f);
    CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 20000U);
    CHECK(Bsp_Timer_SpeedCapture_GetRpm() == info.rpm);
    CHECK(Bsp_Timer_SpeedCapture_GetFrequency() == 50.0f);
    /* 非整数 Hz 的 RPM 必须直接按周期计算，不能先截断频率。 */
    restart(); pulse(0U, 0U); pulse(33333U, 33U);
    CHECK(Bsp_Timer_SpeedCapture_GetRpm() == 60000000ULL / (33333ULL * BSP_SPEED_PULSES_PER_REV));
    restart(); pulse(65000U, 0U); overflow(); pulse(1000U, 1U);
    CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 1536U);
    /* 低速时跨三次溢出，周期是 200000 ticks，不是截断后的 3392。 */
    restart(); pulse(100U, 0U); overflow(); overflow(); overflow(); pulse(3492U, 200U);
    CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 200000U);
    CHECK(Bsp_Timer_SpeedCapture_GetRpm() == 300U / BSP_SPEED_PULSES_PER_REV);
    /* UIF 与 CC1 同时挂起：捕获发生在溢出之后。 */
    restart(); pulse(65000U, 0U);
    test_tim3.SR = TIM_FLAG_UPDATE; pulse(1000U, 1U); test_tim3.CNT = 1100U;
    CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 1536U); overflow();
    pulse(2000U, 2U); CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 1000U);
    /* 捕获发生在溢出前，但 IRQ 在溢出后处理。 */
    restart(); pulse(64000U, 0U);
    test_tim3.SR = TIM_FLAG_UPDATE; test_tim3.CCR1 = 65000U; test_tim3.CNT = 10U;
    now_ms = 1U; HAL_TIM_IC_CaptureCallback(&htim3);
    CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 1000U); overflow();
    pulse(1000U, 2U); CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 1536U);
    /* 超时边界归零，恢复需要两次新脉冲；读取接口不破坏测量基准。 */
    restart(); pulse(100U, 0U); pulse(20100U, 20U);
    now_ms = 20U + BSP_SPEED_SIGNAL_TIMEOUT_MS - 1U; CHECK(Bsp_Timer_SpeedCapture_IsValid());
    ++now_ms; Bsp_Timer_SpeedCapture_GetInfo(&info);
    CHECK(!info.valid && !info.rpm && !info.period_ticks && info.frequency_hz == 0.0f);
    pulse(30000U, now_ms); CHECK(!Bsp_Timer_SpeedCapture_IsValid());
    pulse(50000U, now_ms + 20U); CHECK(Bsp_Timer_SpeedCapture_IsValid());
    /* 漏边沿及零周期无效，并可在下一完整周期恢复。 */
    test_tim3.SR = TIM_FLAG_CC1OF; pulse(51000U, now_ms + 1U);
    CHECK(!Bsp_Timer_SpeedCapture_IsValid());
    CHECK(!(test_tim3.SR & TIM_FLAG_CC1OF));
    pulse(52000U, now_ms + 1U); CHECK(Bsp_Timer_SpeedCapture_IsValid());
    pulse(52000U, now_ms); CHECK(!Bsp_Timer_SpeedCapture_IsValid());
    /* HAL 毫秒计数和扩展 TIM3 计数都可跨 32 位回绕。 */
    restart(); pulse(0U, UINT32_MAX - 10U); pulse(20000U, 9U);
    CHECK(Bsp_Timer_SpeedCapture_IsValid());
    restart(); for (i = 0; i < 65535U; ++i) overflow();
    pulse(65000U, 0U); overflow(); pulse(1000U, 1U);
    CHECK(Bsp_Timer_SpeedCapture_GetPeriod() == 1536U);
    /* Getter 保留调用方 PRIMASK；停止/启动失败均不产生有效读数。 */
    test_primask = 1U; Bsp_Timer_SpeedCapture_GetInfo(&info); CHECK(test_primask == 1U);
    test_primask = 0U; Bsp_Timer_SpeedCapture_Stop(); CHECK(!Bsp_Timer_SpeedCapture_IsValid());
    capture_start_status = HAL_ERROR; CHECK(Bsp_Timer_SpeedCapture_Start() == HAL_ERROR);
    CHECK(!(test_tim3.DIER & TIM_IT_UPDATE)); CHECK(!Bsp_Timer_SpeedCapture_IsValid());
    pclk1 = 100000000U; apb_divider = RCC_HCLK_DIV1; restart();
    Bsp_Timer_SpeedCapture_GetInfo(&info); CHECK(info.timer_tick_hz == 1000000U);
    /* PWM 边界以及驱动使能时序。 */
    Motor_Init(); CHECK(!enabled && Motor_GetDuty() == 0U && test_tim1.CCR3 == 0U);
    Motor_SetDuty(1000U); CHECK(!enabled && Motor_GetDuty() == 0U);
    Motor_Enable(true); Motor_SetDuty(250U);
    CHECK(enabled && test_tim1.CCR3 == 250U && Motor_GetDuty() == 250U);
    i = pwm_starts; Motor_SetDuty(500U); CHECK(pwm_starts == i);
    Motor_SetDuty(300U); CHECK(enabled && test_tim1.CCR3 == 300U && Motor_GetDuty() == 300U);
    Motor_SetDuty(600U); CHECK(enabled && test_tim1.CCR3 == 600U && Motor_GetDuty() == 600U);
    Motor_SetDuty(2000U); CHECK(Motor_GetDuty() == 1000U && test_tim1.CCR3 == 1000U);
    Motor_SetDuty(0U); CHECK(!enabled && Motor_GetDuty() == 0U && test_tim1.CCR3 == 0U);
    Motor_Enable(true); pwm_start_status = HAL_ERROR; Motor_SetDuty(250U);
    CHECK(!enabled && Motor_GetInfo().state == MOTOR_STATE_ERROR);
    Motor_UpdateSpeed(123456U, true); CHECK(Motor_GetInfo().rpm == 123456U);
    Motor_UpdateSpeed(123456U, false); CHECK(!Motor_GetInfo().rpm && !Motor_GetInfo().speed_valid);
    printf("PASS: %u checks (ppr=%lu)\n", checks, (unsigned long)BSP_SPEED_PULSES_PER_REV);
    return 0;
}
