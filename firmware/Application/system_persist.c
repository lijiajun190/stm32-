#include "system_persist.h"
#include <string.h>
#define CONFIG_FORMAT_VERSION 1U
#define FAULT_FORMAT_VERSION 1U
typedef char ConfigRecordSizeCheck[(sizeof(SystemConfig_t) <= STORE_PAYLOAD_BYTES) ? 1 : -1];
typedef char FaultRecordSizeCheck[(sizeof(FaultRecord_t) <= STORE_PAYLOAD_BYTES) ? 1 : -1];
bool SystemPersist_Init(SystemPersist_t *persist, const StoreBackend_t *backend, SystemConfig_t *config)
{
    if (!persist || !config) return false;
    memset(persist, 0, sizeof(*persist)); SystemConfig_Defaults(config);
    if (!PersistentStore_Init(&persist->journal, backend)) return false;
    SystemConfig_t loaded;
    if (PersistentStore_ReadLatest(&persist->journal, STORE_TYPE_CONFIG, CONFIG_FORMAT_VERSION,
                                  &loaded, sizeof(loaded), &persist->config_sequence) && SystemConfig_Validate(&loaded)) {
        *config = loaded; persist->saved_config = loaded; persist->config_saved = true;
    } else if (PersistentStore_Append(&persist->journal, STORE_TYPE_CONFIG, CONFIG_FORMAT_VERSION,
                                      config, sizeof(*config))) {
        persist->saved_config = *config; persist->config_saved = true;
        persist->config_sequence = persist->journal.next_sequence - 1;
    }
    persist->fault_valid = PersistentStore_ReadLatest(&persist->journal, STORE_TYPE_FAULT, FAULT_FORMAT_VERSION,
        &persist->latest_fault, sizeof(persist->latest_fault), &persist->fault_sequence);
    return true;
}
bool SystemPersist_SaveConfig(SystemPersist_t *persist, const SystemConfig_t *config)
{
    if (!persist || !SystemConfig_Validate(config)) return false;
    if (persist->config_saved && memcmp(config, &persist->saved_config, sizeof(*config)) == 0) return true;
    if (!PersistentStore_Append(&persist->journal, STORE_TYPE_CONFIG, CONFIG_FORMAT_VERSION, config, sizeof(*config))) return false;
    persist->saved_config = *config; persist->config_saved = true;
    persist->config_sequence = persist->journal.next_sequence - 1;
    return true;
}
bool SystemPersist_SaveFault(SystemPersist_t *persist, const FaultRecord_t *record)
{
    if (!persist || !record || record->reason < FAULT_REASON_MOTOR || record->reason > FAULT_REASON_NMI) return false;
    if (!PersistentStore_Append(&persist->journal, STORE_TYPE_FAULT, FAULT_FORMAT_VERSION, record, sizeof(*record))) return false;
    persist->latest_fault = *record; persist->fault_valid = true;
    persist->fault_sequence = persist->journal.next_sequence - 1;
    return true;
}
