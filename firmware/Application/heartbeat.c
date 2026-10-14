#include "heartbeat.h"
#include <string.h>
static const uint32_t deadlines[HEARTBEAT_COUNT] = {250, 500, 500, 500, 2500, 500, 500, 1500};
void Heartbeat_Init(Heartbeat_t *heartbeat, uint32_t now_ms)
{
    memset(heartbeat, 0, sizeof(*heartbeat)); heartbeat->start_ms = now_ms;
}
void Heartbeat_Report(Heartbeat_t *heartbeat, Heartbeat_ID_t id, uint32_t now_ms)
{
    if ((unsigned)id >= HEARTBEAT_COUNT) return;
    heartbeat->timestamp[id] = now_ms; heartbeat->seen_mask |= 1UL << id;
}
uint32_t Heartbeat_Missing(const Heartbeat_t *heartbeat, uint32_t now_ms)
{
    uint32_t missing = 0;
    for (unsigned id = 0; id < HEARTBEAT_COUNT; ++id) {
        if (!(heartbeat->seen_mask & (1UL << id))) {
            if ((uint32_t)(now_ms - heartbeat->start_ms) >= HEARTBEAT_START_GRACE_MS) missing |= 1UL << id;
        } else if ((uint32_t)(now_ms - heartbeat->timestamp[id]) > deadlines[id]) missing |= 1UL << id;
    }
    return missing;
}
bool Heartbeat_Ready(const Heartbeat_t *heartbeat, uint32_t now_ms)
{
    return heartbeat->seen_mask == HEARTBEAT_ALL && Heartbeat_Missing(heartbeat, now_ms) == 0;
}
