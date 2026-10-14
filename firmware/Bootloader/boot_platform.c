/** Bootloader 无 RTOS，以 HSI 16 MHz 和轮询 USART1 运行。 */
#include "boot_platform.h"
#include "stm32f4xx_hal.h"
#include <string.h>
static UART_HandleTypeDef uart;
static IWDG_HandleTypeDef watchdog;
static uint32_t address(unsigned area) { return area == BOOT_AREA_APP ? BOOT_APP_BASE : BOOT_STAGE_BASE; }
static bool read_flash(void *ctx, unsigned area, uint32_t offset, void *data, uint32_t length)
{
    (void)ctx;
    if (area > BOOT_AREA_STAGE || offset > BOOT_SLOT_BYTES || length > BOOT_SLOT_BYTES - offset) return false;
    memcpy(data, (const void *)(address(area) + offset), length); return true;
}
static bool program_flash(void *ctx, unsigned area, uint32_t offset, uint32_t word)
{
    (void)ctx;
    if (area > BOOT_AREA_STAGE || offset % 4U || offset > BOOT_SLOT_BYTES - 4U) return false;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address(area) + offset, word);
    HAL_FLASH_Lock();
    return result == HAL_OK && *(volatile uint32_t *)(address(area) + offset) == word;
}
static bool erase_flash(void *ctx, unsigned area)
{
    (void)ctx;
    if (area > BOOT_AREA_STAGE) return false;
    FLASH_EraseInitTypeDef erase = {0}; uint32_t error;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS; erase.NbSectors = 1;
    erase.Sector = area == BOOT_AREA_APP ? FLASH_SECTOR_5 : FLASH_SECTOR_6;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    BootPlatform_Feed(); HAL_FLASH_Unlock();
    HAL_StatusTypeDef result = HAL_FLASHEx_Erase(&erase, &error);
    HAL_FLASH_Lock(); BootPlatform_Feed(); return result == HAL_OK;
}
static void feed_backend(void *ctx) { (void)ctx; BootPlatform_Feed(); }
BootBackend_t BootPlatform_Backend(void)
{
    BootBackend_t backend = {0}; backend.read = read_flash; backend.program = program_flash;
    backend.erase = erase_flash; backend.feed_watchdog = feed_backend; return backend;
}
void HAL_MspInit(void) { __HAL_RCC_SYSCFG_CLK_ENABLE(); __HAL_RCC_PWR_CLK_ENABLE(); }
void SysTick_Handler(void) { HAL_IncTick(); }
void BootPlatform_Init(void)
{
    HAL_Init();
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    /* 先写禁用电平，再配置输出，整个升级过程关闭电机和蜂鸣器。 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio = {0}; gpio.Pin = GPIO_PIN_7 | GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Pull = GPIO_NOPULL; HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_2; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLDOWN; HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_15; HAL_GPIO_Init(GPIOB, &gpio);
    __HAL_RCC_USART1_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP; gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
    uart.Instance = USART1; uart.Init.BaudRate = 115200; uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1; uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX; uart.Init.HwFlowCtl = UART_HWCONTROL_NONE; uart.Init.OverSampling = UART_OVERSAMPLING_16;
    (void)HAL_UART_Init(&uart);
    /* 禁用缓存，使升级写入后的 CRC 读取不使用旧数据。 */
    __HAL_FLASH_DATA_CACHE_DISABLE(); __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET(); __HAL_FLASH_INSTRUCTION_CACHE_RESET();
    watchdog.Instance = IWDG; watchdog.Init.Prescaler = IWDG_PRESCALER_128; watchdog.Init.Reload = 1999;
    (void)HAL_IWDG_Init(&watchdog);
}
bool BootPlatform_Requested(void)
{
    __HAL_RCC_PWR_CLK_ENABLE(); HAL_PWR_EnableBkUpAccess(); __HAL_RCC_RTC_ENABLE();
    bool requested = RTC->BKP1R == BOOT_REQUEST_MAGIC;
    RTC->BKP1R = 0;
    return requested || HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2) == GPIO_PIN_SET;
}
bool BootPlatform_ReadByte(uint8_t *byte)
{
    if (__HAL_UART_GET_FLAG(&uart, UART_FLAG_ORE)) __HAL_UART_CLEAR_OREFLAG(&uart);
    if (!__HAL_UART_GET_FLAG(&uart, UART_FLAG_RXNE)) return false;
    *byte = (uint8_t)uart.Instance->DR; return true;
}
bool BootPlatform_Send(const uint8_t *bytes, uint16_t length)
{ return HAL_UART_Transmit(&uart, (uint8_t *)bytes, length, 100U) == HAL_OK; }
void BootPlatform_Feed(void) { (void)HAL_IWDG_Refresh(&watchdog); }
void BootPlatform_Jump(void)
{
    uint32_t stack = *(volatile uint32_t *)BOOT_APP_BASE;
    uint32_t entry = *(volatile uint32_t *)(BOOT_APP_BASE + 4U);
    BootPlatform_Feed(); __disable_irq();
    SysTick->CTRL = 0; SysTick->LOAD = 0; SysTick->VAL = 0;
    for (unsigned i = 0; i < 8; ++i) { NVIC->ICER[i] = 0xFFFFFFFFU; NVIC->ICPR[i] = 0xFFFFFFFFU; }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    HAL_UART_DeInit(&uart); HAL_RCC_DeInit();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE(); __HAL_FLASH_DATA_CACHE_ENABLE();
    SCB->VTOR = BOOT_APP_BASE; __set_BASEPRI(0); __set_FAULTMASK(0); __DSB(); __ISB();
    /* MSP 修改之后不能再执行依赖当前 C 栈的代码。 */
    __asm volatile("msr msp, %0\nmsr control, %2\nisb\ncpsie i\nbx %1" : : "r"(stack), "r"(entry), "r"(0U) : "memory");
    for (;;) {}
}
