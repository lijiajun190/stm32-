/** 与 RTOS 无关的心跳判定。使用递增时钟，支持 uint32 回绕。 */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { HEARTBEAT_MOTOR, HEARTBEAT_SENSOR, HEARTBEAT_UI, HEARTBEAT_SYSTEM,
               HEARTBEAT_LOGGER, HEARTBEAT_STORAGE, HEARTBEAT_IDLE, HEARTBEAT_WATCHDOG,
               HEARTBEAT_COUNT } Heartbeat_ID_t;
#define HEARTBEAT_ALL ((1UL << HEARTBEAT_COUNT) - 1UL)
#define HEARTBEAT_START_GRACE_MS 5000U
typedef struct {
    uint32_t timestamp[HEARTBEAT_COUNT];
    uint32_t seen_mask, start_ms;
} Heartbeat_t;
void Heartbeat_Init(Heartbeat_t *heartbeat, uint32_t now_ms);
void Heartbeat_Report(Heartbeat_t *heartbeat, Heartbeat_ID_t id, uint32_t now_ms);
uint32_t Heartbeat_Missing(const Heartbeat_t *heartbeat, uint32_t now_ms);
bool Heartbeat_Ready(const Heartbeat_t *heartbeat, uint32_t now_ms);
#endif
