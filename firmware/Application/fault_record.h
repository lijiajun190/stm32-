#ifndef FAULT_RECORD_H
#define FAULT_RECORD_H
#include <stdint.h>
typedef enum {
    FAULT_REASON_MOTOR = 1, FAULT_REASON_SENSOR, FAULT_REASON_HEARTBEAT,
    FAULT_REASON_HARDFAULT, FAULT_REASON_MEMFAULT, FAULT_REASON_BUSFAULT,
    FAULT_REASON_USAGEFAULT, FAULT_REASON_STACK, FAULT_REASON_MALLOC,
    FAULT_REASON_STARTUP, FAULT_REASON_WATCHDOG_RESET, FAULT_REASON_NMI
} FaultReason_t;
typedef struct {
    uint32_t reason, detail, uptime_ms, reset_flags;
    uint32_t mode, rpm, target_rpm, duty, sensor_valid, sensor_errors;
    int32_t temperature_mC;
    uint32_t frame_valid, registers[8], exc_return, cfsr, hfsr, dfsr, mmfar, bfar, afsr;
    uint32_t task_id;
} FaultRecord_t;
/* 协议中的 registers 顺序为 R0,R1,R2,R3,R12,LR,PC,xPSR。 */
#endif
