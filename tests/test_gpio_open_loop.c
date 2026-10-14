/* 链接正式 GPIO、LED、Button 与开环时序模块，模拟接线和 HAL 时钟。 */
#include "gpio.h"
#include "bsp_gpio.h"
#include "button.h"
#include "led.h"
#include "motor_open_loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GPIO_TypeDef test_gpioa, test_gpiob, test_gpioc;
uint32_t test_primask;
static unsigned checks, event_counts[BUTTON_MAX][BUTTON_EVENT_REPEAT + 1];
static uint32_t now_ms;
#define CHECK(x) do { ++checks; if (!(x)) { \
    printf("FAIL at line %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)

uint32_t HAL_GetTick(void) { return now_ms; }
void HAL_Delay(uint32_t delay) { now_ms += delay; }
void HAL_NVIC_SetPriority(uint32_t irq, uint32_t preempt, uint32_t sub)
{ (void)irq; (void)preempt; (void)sub; }
void HAL_NVIC_EnableIRQ(uint32_t irq) { (void)irq; }
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState level)
{
    if (level == GPIO_PIN_SET) { port->ODR |= pin; port->IDR |= pin; }
    else { port->ODR &= ~pin; port->IDR &= ~pin; }
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{ return (port->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin)
{ port->ODR ^= pin; port->IDR ^= pin; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    unsigned i;
    for (i = 0; i < 16U; ++i) {
        uint32_t pin = 1U << i;
        if (!(config->Pin & pin)) continue;
        port->mode[i] = config->Mode;
        port->pull[i] = config->Pull;
        if (config->Mode != GPIO_MODE_OUTPUT_PP) {
            if (config->Pull == GPIO_PULLUP) port->IDR |= pin;
            else if (config->Pull == GPIO_PULLDOWN) port->IDR &= ~pin;
        }
    }
}
static void on_event(Button_ID_t id, Button_Event_t event)
{ ++event_counts[id][event]; }
static void scan_after(uint32_t delta)
{ now_ms += delta; Button_Process(); }
static void set_pressed(unsigned id, bool pressed)
{
    static const uint16_t pins[] = {GPIO_PIN_0, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4};
    GPIO_TypeDef *port = id == BUTTON_ENCODER ? GPIOA : GPIOC;
    bool high = id == BUTTON_ENCODER ? !pressed : pressed;
    if (high) port->IDR |= pins[id]; else port->IDR &= ~pins[id];
}
static void reset_buttons(void)
{
    unsigned i;
    for (i = 0; i < BUTTON_MAX; ++i) set_pressed(i, false);
    memset(event_counts, 0, sizeof(event_counts));
    now_ms = 0U;
    Button_Init();
    Button_RegisterCallback(on_event);
}

int main(void)
{
    unsigned i;
    Motor_OpenLoop_t sweep = {0};
    uint16_t duty = 123U;
    MX_GPIO_Init();
    CHECK(test_gpioa.mode[0] == GPIO_MODE_IT_FALLING && test_gpioa.pull[0] == GPIO_PULLUP);
    CHECK(test_gpioc.mode[2] == GPIO_MODE_IT_RISING && test_gpioc.pull[2] == GPIO_PULLDOWN);
    CHECK(test_gpioc.mode[3] == GPIO_MODE_IT_RISING && test_gpioc.pull[3] == GPIO_PULLDOWN);
    CHECK(test_gpioc.mode[4] == GPIO_MODE_INPUT && test_gpioc.pull[4] == GPIO_PULLDOWN);
    CHECK(test_gpioc.mode[7] == GPIO_MODE_OUTPUT_PP && test_gpioc.mode[9] == GPIO_MODE_OUTPUT_PP);
    CHECK((test_gpiob.ODR & (LED_R_Pin | LED_G_Pin)) == (LED_R_Pin | LED_G_Pin));
    CHECK(test_gpioc.ODR & LED_BLUE_Pin);
    CHECK(!(test_gpioc.ODR & (MOTOR_EN_Pin | MOTOR_DIR_Pin)));
    Bsp_GPIO_Init();
    for (i = 0; i < BSP_LED_MAX; ++i) {
        CHECK(!Bsp_LED_Get((Bsp_LedId_t)i));
        Bsp_LED_Set((Bsp_LedId_t)i, true); CHECK(Bsp_LED_Get((Bsp_LedId_t)i));
        Bsp_LED_Toggle((Bsp_LedId_t)i); CHECK(!Bsp_LED_Get((Bsp_LedId_t)i));
    }
    Bsp_LED_Set((Bsp_LedId_t)-1, true); Bsp_LED_Toggle(BSP_LED_MAX);
    CHECK(!Bsp_LED_Get(BSP_LED_MAX));
    Bsp_Motor_Enable(true); CHECK(test_gpioc.ODR & MOTOR_EN_Pin);
    Bsp_Motor_SetDirection(false); CHECK(test_gpioc.ODR & MOTOR_DIR_Pin);
    Bsp_Motor_Enable(false); CHECK(!(test_gpioc.ODR & MOTOR_EN_Pin));
    CHECK(test_gpioc.ODR & MOTOR_DIR_Pin); /* 使能操作不改写方向。 */
    Bsp_Motor_SetDirection(true); CHECK(!(test_gpioc.ODR & MOTOR_DIR_Pin));
    LED_Init();
    LED_SetColor(LED_COLOR_YELLOW);
    CHECK(LED_GetState(LED_RED) && LED_GetState(LED_GREEN) && !LED_GetState(LED_BLUE));
    CHECK(!(test_gpiob.ODR & (LED_R_Pin | LED_G_Pin)));
    LED_AllOff(); LED_Toggle(LED_BLUE); CHECK(LED_GetState(LED_BLUE));
    LED_On((LED_ID_t)-1); LED_Off(LED_MAX); CHECK(!LED_GetState(LED_MAX));
    /* 每个按键都验证电平、按下消抖、释放抖动和单击次数。 */
    for (i = 0; i < BUTTON_MAX; ++i) {
        reset_buttons(); scan_after(100U);
        CHECK(!Bsp_Key_Read((Bsp_KeyId_t)i) && !Button_IsPressed((Button_ID_t)i));
        set_pressed(i, true); CHECK(Bsp_Key_Read((Bsp_KeyId_t)i));
        scan_after(0U); scan_after(10U);
        CHECK(!event_counts[i][BUTTON_EVENT_PRESS]);
        set_pressed(i, false); scan_after(10U); /* 按下抖动取消。 */
        CHECK(!event_counts[i][BUTTON_EVENT_CLICK]);
        set_pressed(i, true); scan_after(0U); scan_after(20U);
        CHECK(event_counts[i][BUTTON_EVENT_PRESS] == 1U && Button_IsPressed((Button_ID_t)i));
        set_pressed(i, false); scan_after(0U); scan_after(10U);
        CHECK(!event_counts[i][BUTTON_EVENT_CLICK]);
        set_pressed(i, true); scan_after(0U); /* 释放抖动不能发 CLICK。 */
        CHECK(!event_counts[i][BUTTON_EVENT_CLICK]);
        set_pressed(i, false); scan_after(0U); scan_after(20U); scan_after(50U);
        CHECK(event_counts[i][BUTTON_EVENT_CLICK] == 1U);
        CHECK(event_counts[i][BUTTON_EVENT_RELEASE] == 1U && !Button_IsPressed((Button_ID_t)i));
        /* 上电按住：抑制按键，待稳定释放后才重新接收输入。 */
        reset_buttons(); set_pressed(i, true); Button_Init();
        scan_after(2000U); CHECK(!event_counts[i][BUTTON_EVENT_PRESS]);
        set_pressed(i, false); scan_after(0U); scan_after(20U);
        CHECK(!event_counts[i][BUTTON_EVENT_CLICK]);
        set_pressed(i, true); scan_after(0U); scan_after(20U);
        CHECK(event_counts[i][BUTTON_EVENT_PRESS] == 1U);
        scan_after(1000U); CHECK(event_counts[i][BUTTON_EVENT_LONG_PRESS] == 1U);
        scan_after(200U); CHECK(event_counts[i][BUTTON_EVENT_REPEAT] == 1U);
        set_pressed(i, false); scan_after(0U); scan_after(20U);
        CHECK(!event_counts[i][BUTTON_EVENT_CLICK] && event_counts[i][BUTTON_EVENT_RELEASE] == 1U);
    }
    CHECK(!Bsp_Key_Read((Bsp_KeyId_t)-1)); CHECK(!Bsp_Key_Read(BSP_KEY_MAX));
    CHECK(!Button_IsPressed((Button_ID_t)-1));
    /* 毫秒回绕不破坏按键消抖。 */
    reset_buttons(); now_ms = UINT32_MAX - 10U;
    set_pressed(BUTTON_MODE, true); scan_after(0U); scan_after(20U);
    CHECK(event_counts[BUTTON_MODE][BUTTON_EVENT_PRESS] == 1U);
    /* 扫测只跑一轮：从 0% 起，每档完整停留，到 100% 结束后自动停机。 */
    CHECK(!Motor_OpenLoop_Poll(&sweep, 0U, &duty));
    CHECK(!Motor_OpenLoop_Start(&sweep, 0U, 0U));
    CHECK(!Motor_OpenLoop_Start(&sweep, 0U, 999U));
    CHECK(!Motor_OpenLoop_Start(&sweep, 0U, 60001U));
    CHECK(Motor_OpenLoop_Start(&sweep, 0U, 1000U));
    CHECK(!Motor_OpenLoop_Poll(&sweep, 999U, &duty));
    CHECK(Motor_OpenLoop_Poll(&sweep, 1000U, &duty) && duty == 300U);
    CHECK(Motor_OpenLoop_Poll(&sweep, 2000U, &duty) && duty == 600U);
    CHECK(Motor_OpenLoop_Poll(&sweep, 3000U, &duty) && duty == 1000U);
    CHECK(Motor_OpenLoop_Poll(&sweep, 4000U, &duty) && duty == 0U && !sweep.active);
    CHECK(!Motor_OpenLoop_Poll(&sweep, 5000U, &duty));
    CHECK(Motor_OpenLoop_Start(&sweep, 100U, 1000U));
    CHECK(Motor_OpenLoop_Poll(&sweep, 10100U, &duty) && duty == 300U);
    CHECK(!Motor_OpenLoop_Poll(&sweep, 10101U, &duty)); /* 不追赶延迟档位。 */
    Motor_OpenLoop_Stop(&sweep); CHECK(!Motor_OpenLoop_Poll(&sweep, 50000U, &duty));
    CHECK(Motor_OpenLoop_Start(&sweep, UINT32_MAX - 500U, 1000U));
    CHECK(!Motor_OpenLoop_Poll(&sweep, 498U, &duty));
    CHECK(Motor_OpenLoop_Poll(&sweep, 499U, &duty) && duty == 300U);
    Motor_OpenLoop_Stop(NULL);
    CHECK(!Motor_OpenLoop_Start(NULL, 0U, 1000U));
    CHECK(!Motor_OpenLoop_Poll(NULL, 0U, &duty));
    CHECK(!Motor_OpenLoop_Poll(&sweep, 1500U, NULL));
    printf("PASS: %u GPIO/button/open-loop checks\n", checks);
    return 0;
}
