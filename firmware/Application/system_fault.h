#ifndef SYSTEM_FAULT_H
#define SYSTEM_FAULT_H
#include "fault_record.h"
#include <stdbool.h>
void SystemFault_Boot(void);
void SystemFault_UpdateSnapshot(const FaultRecord_t *context);
void SystemFault_Stage(const FaultRecord_t *record);
bool SystemFault_GetPending(FaultRecord_t *record);
void SystemFault_ClearPending(void);
/* 不返回、不使用 HAL/RTOS/printf，直接关电机并写 .noinit 邮箱后复位。 */
__attribute__((noreturn)) void SystemFault_Fatal(FaultReason_t reason, uint32_t detail);
__attribute__((noreturn)) void SystemFault_FromException(uint32_t *frame, uint32_t exc_return, uint32_t reason);
void SystemFault_EmergencyStop(void);
#endif
