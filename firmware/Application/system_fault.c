#include "system_fault.h"
#include "crc.h"
#include "main.h"
#include <string.h>
#include <stddef.h>
#define MAILBOX_MAGIC 0x43525348UL
typedef struct { FaultRecord_t record; uint32_t crc, commit; } FaultMailbox_t;
__attribute__((section(".noinit"))) static volatile FaultMailbox_t mailbox;
static volatile FaultRecord_t snapshot;
static uint32_t reset_flags;
/* 异常路径使用独立栈，即使 PSP 已溢出也不继续在损坏的任务栈上记录。 */
__attribute__((section(".noinit"), aligned(8))) uint8_t fault_exception_stack[1024];
void SystemFault_EmergencyStop(void)
{
    /* 先开放 GPIOC 时钟，即使初始化早期异常也确保 PC7 禁止功率输出。 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOC->BSRR = (uint32_t)MOTOR_EN_Pin << 16;
    GPIOC->MODER = (GPIOC->MODER & ~(3UL << 14)) | (1UL << 14);
    if (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        TIM1->CCER &= ~TIM_CCER_CC3NE;
        TIM1->CCR3 = 0;
    }
    __DSB();
}
bool SystemFault_GetPending(FaultRecord_t *record)
{
    if (!record || mailbox.commit != MAILBOX_MAGIC) return false;
    memcpy(record, (const void *)&mailbox.record, sizeof(*record));
    return mailbox.crc == CRC32_Calculate((const uint8_t *)record, sizeof(*record)) &&
        record->reason >= FAULT_REASON_MOTOR && record->reason <= FAULT_REASON_NMI;
}
void SystemFault_ClearPending(void) { mailbox.commit = 0; __DSB(); }
void SystemFault_Boot(void)
{
    reset_flags = RCC->CSR;
    /* 保留各类异常入口，避免全部升级为 HardFault 而丢失分类。 */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
    FaultRecord_t pending;
    /* POR/BOR 的 RAM 内容不可靠；暖复位的 .noinit 必须同时通过格式与 CRC。 */
    if (reset_flags & (RCC_CSR_PORRSTF | RCC_CSR_BORRSTF)) SystemFault_ClearPending();
    if (SystemFault_GetPending(&pending)) {
        /* 记录本次实际复位原因，而非异常发生前那一次启动的复位原因。 */
        SystemFault_Stage(&pending);
    } else if (reset_flags & RCC_CSR_IWDGRSTF) {
        memset(&pending, 0, sizeof(pending)); pending.reason = FAULT_REASON_WATCHDOG_RESET;
        pending.reset_flags = reset_flags; SystemFault_Stage(&pending);
    }
    RCC->CSR |= RCC_CSR_RMVF;
}
void SystemFault_UpdateSnapshot(const FaultRecord_t *context)
{
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    memcpy((void *)&snapshot, context, sizeof(snapshot));
    __set_PRIMASK(mask);
}
void SystemFault_Stage(const FaultRecord_t *record)
{
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    mailbox.commit = 0;
    memcpy((void *)&mailbox.record, record, sizeof(*record));
    mailbox.record.reset_flags = reset_flags;
    mailbox.crc = CRC32_Calculate((const uint8_t *)&mailbox.record, sizeof(mailbox.record));
    __DSB(); mailbox.commit = MAILBOX_MAGIC; __DSB();
    __set_PRIMASK(mask);
}
static void capture(FaultReason_t reason, uint32_t detail, const uint32_t *frame, uint32_t exc_return)
{
    FaultRecord_t record;
    memcpy(&record, (const void *)&snapshot, sizeof(record));
    record.reason = reason; record.detail = detail;
    record.cfsr = SCB->CFSR; record.hfsr = SCB->HFSR; record.dfsr = SCB->DFSR;
    record.mmfar = SCB->MMFAR; record.bfar = SCB->BFAR; record.afsr = SCB->AFSR;
    record.exc_return = exc_return;
    record.frame_valid = 0;
    memset(record.registers, 0, sizeof(record.registers));
    uintptr_t address = (uintptr_t)frame;
    /* FPU 扩展栈帧在核心寄存器之前多 18 words；边界检查后才访问。 */
    if (frame && !(exc_return & (1UL << 4))) address += 18 * 4;
    /* 压栈/退栈失败或 FPU 延迟压栈错误时，即使地址在 RAM 内也不能信任栈帧。 */
    const uint32_t stacking_errors = (1UL << 3) | (1UL << 4) | (1UL << 5) |
                                    (1UL << 11) | (1UL << 12) | (1UL << 13);
    if (frame && !(record.cfsr & stacking_errors) && address >= 0x20000000UL &&
        address <= 0x20020000UL - 8 * 4 && !(address & 3)) {
        memcpy(record.registers, (const void *)address, sizeof(record.registers)); record.frame_valid = 1;
    }
    SystemFault_Stage(&record);
}
__attribute__((used, noreturn)) void SystemFault_FatalOnSafeStack(FaultReason_t reason, uint32_t detail)
{
    __disable_irq(); SystemFault_EmergencyStop();
    /* Flash 初始化失败引发的启动复位不能覆盖尚未落盘的异常邮箱。 */
    FaultRecord_t retained;
    if (reason != FAULT_REASON_STARTUP || !SystemFault_GetPending(&retained))
        capture(reason, detail, NULL, 0);
    NVIC_SystemReset(); for (;;) {}
}
__attribute__((naked, noreturn)) void SystemFault_Fatal(FaultReason_t reason, uint32_t detail)
{
    __asm volatile("cpsid i\n"
                   "ldr r2, =fault_exception_stack+1024\n"
                   "msr msp, r2\n"
                   "mrs r2, control\n"
                   "bic r2, r2, #2\n"
                   "msr control, r2\n"
                   "isb\n"
                   "b SystemFault_FatalOnSafeStack\n");
}
void SystemFault_FromException(uint32_t *frame, uint32_t exc_return, uint32_t reason)
{
    __disable_irq(); SystemFault_EmergencyStop(); capture((FaultReason_t)reason, 0, frame, exc_return);
    NVIC_SystemReset(); for (;;) {}
}
