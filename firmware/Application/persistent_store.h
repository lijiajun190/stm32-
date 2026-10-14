/** 固定槽日志，Flash 后端可替换为主机内存故障注入模型。 */
#ifndef PERSISTENT_STORE_H
#define PERSISTENT_STORE_H
#include <stdint.h>
#include <stdbool.h>
#define STORE_SLOT_BYTES 256U
#define STORE_PAYLOAD_BYTES 224U
#define STORE_TYPE_CONFIG 1U
#define STORE_TYPE_FAULT 2U
typedef struct {
    void *context;
    uint32_t bank_bytes;
    bool (*read)(void *, unsigned bank, uint32_t offset, void *data, uint32_t length);
    bool (*program)(void *, unsigned bank, uint32_t offset, uint32_t word);
    bool (*erase)(void *, unsigned bank);
} StoreBackend_t;
typedef struct {
    StoreBackend_t backend;
    uint32_t next_sequence;
    unsigned active_bank;
    uint32_t next_offset[2];
    bool initialized;
} PersistentStore_t;
bool PersistentStore_Init(PersistentStore_t *store, const StoreBackend_t *backend);
bool PersistentStore_ReadLatest(PersistentStore_t *store, uint32_t type, uint32_t version,
                                void *payload, uint32_t length, uint32_t *sequence);
bool PersistentStore_Append(PersistentStore_t *store, uint32_t type, uint32_t version,
                            const void *payload, uint32_t length);
#endif
