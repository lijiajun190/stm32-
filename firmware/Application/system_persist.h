#ifndef SYSTEM_PERSIST_H
#define SYSTEM_PERSIST_H
#include "system_config.h"
#include "persistent_store.h"
#include "fault_record.h"
typedef struct {
    PersistentStore_t journal;
    SystemConfig_t saved_config;
    FaultRecord_t latest_fault;
    bool config_saved, fault_valid;
    uint32_t config_sequence, fault_sequence;
} SystemPersist_t;
bool SystemPersist_Init(SystemPersist_t *persist, const StoreBackend_t *backend, SystemConfig_t *config);
bool SystemPersist_SaveConfig(SystemPersist_t *persist, const SystemConfig_t *config);
bool SystemPersist_SaveFault(SystemPersist_t *persist, const FaultRecord_t *record);
/* 应用提供串口/后续工具可调用的线程安全接口，提交不等于已写入 Flash。 */
bool Tasks_GetConfig(SystemConfig_t *config);
bool Tasks_SetConfig(const SystemConfig_t *config);
bool Tasks_IsConfigSaved(void);
bool Tasks_GetLastFault(FaultRecord_t *fault);
#endif
