#include "persistent_store.h"
#include "crc.h"
#include <string.h>
#include <stddef.h>
#define RECORD_MAGIC 0x52484431UL
#define RECORD_COMMIT 0x434F4D54UL
typedef struct {
    uint32_t magic, type, version, length, sequence;
    uint8_t payload[STORE_PAYLOAD_BYTES];
    uint32_t reserved, crc, commit;
} StoreRecord_t;
typedef char StoreRecordSizeCheck[(sizeof(StoreRecord_t) == STORE_SLOT_BYTES) ? 1 : -1];
static bool newer(uint32_t a, uint32_t b) { return a != b && (uint32_t)(a - b) < 0x80000000UL; }
static bool valid(const StoreRecord_t *record)
{
    return record->magic == RECORD_MAGIC && record->commit == RECORD_COMMIT &&
        record->length > 0 && record->length <= STORE_PAYLOAD_BYTES &&
        (record->type == STORE_TYPE_CONFIG || record->type == STORE_TYPE_FAULT) &&
        record->crc == CRC32_Calculate((const uint8_t *)record, offsetof(StoreRecord_t, crc));
}
bool PersistentStore_Init(PersistentStore_t *store, const StoreBackend_t *backend)
{
    if (!store || !backend || !backend->read || !backend->program || !backend->erase ||
        backend->bank_bytes < 4 * STORE_SLOT_BYTES || backend->bank_bytes % STORE_SLOT_BYTES) return false;
    memset(store, 0, sizeof(*store)); store->backend = *backend;
    StoreRecord_t record;
    bool found = false; uint32_t newest = 0;
    for (unsigned bank = 0; bank < 2; ++bank) {
        for (uint32_t offset = 0; offset < backend->bank_bytes; offset += STORE_SLOT_BYTES) {
            if (!backend->read(backend->context, bank, offset, &record, sizeof(record))) return false;
            /* 连损坏和部分写入的槽也跳过，绝不在非擦除槽上原地重写。 */
            bool blank = true;
            const uint8_t *bytes = (const uint8_t *)&record;
            for (unsigned i = 0; i < sizeof(record); ++i) if (bytes[i] != 0xFF) { blank = false; break; }
            if (!blank) store->next_offset[bank] = offset + STORE_SLOT_BYTES;
            /* 搬移副本 reserved=1，不能在新记录提交前改变当前扇区。 */
            if (valid(&record) && record.reserved == 0 && (!found || newer(record.sequence, newest))) {
                found = true; newest = record.sequence; store->active_bank = bank;
            }
        }
    }
    store->next_sequence = found ? newest + 1 : 1;
    store->initialized = true; return true;
}
bool PersistentStore_ReadLatest(PersistentStore_t *store, uint32_t type, uint32_t version,
                                void *payload, uint32_t length, uint32_t *sequence)
{
    if (!store || !store->initialized || !payload || !length || length > STORE_PAYLOAD_BYTES) return false;
    bool found = false; uint32_t latest = 0;
    StoreRecord_t record;
    for (unsigned bank = 0; bank < 2; ++bank) {
        for (uint32_t offset = 0; offset < store->next_offset[bank]; offset += STORE_SLOT_BYTES) {
            if (!store->backend.read(store->backend.context, bank, offset, &record, sizeof(record))) return false;
            if (valid(&record) && record.type == type && record.version == version && record.length == length &&
                (!found || newer(record.sequence, latest))) {
                memcpy(payload, record.payload, length); latest = record.sequence; found = true;
            }
        }
    }
    if (found && sequence) *sequence = latest;
    return found;
}
static bool write_record(PersistentStore_t *store, unsigned bank, uint32_t offset, StoreRecord_t *record)
{
    record->magic = RECORD_MAGIC;
    if (record->reserved == 0) record->sequence = store->next_sequence++;
    record->crc = CRC32_Calculate((const uint8_t *)record, offsetof(StoreRecord_t, crc));
    record->commit = RECORD_COMMIT;
    uint32_t word;
    for (unsigned i = 0; i < sizeof(*record); i += 4) {
        memcpy(&word, (const uint8_t *)record + i, 4);
        if (!store->backend.program(store->backend.context, bank, offset + i, word)) return false;
    }
    StoreRecord_t verify;
    return store->backend.read(store->backend.context, bank, offset, &verify, sizeof(verify)) &&
        valid(&verify) && memcmp(record, &verify, sizeof(verify)) == 0;
}
bool PersistentStore_Append(PersistentStore_t *store, uint32_t type, uint32_t version,
                            const void *payload, uint32_t length)
{
    if (!store || !store->initialized || !payload || !length || length > STORE_PAYLOAD_BYTES ||
        (type != STORE_TYPE_CONFIG && type != STORE_TYPE_FAULT)) return false;
    unsigned bank = store->active_bank;
    if (store->next_offset[bank] >= store->backend.bank_bytes) {
        /* 只擦写非当前扇区；先搬移最新参数及故障，再追加。老扇区始终保留。
           搬移副本保留旧序号；全部复制完并追加新记录后才提交新扇区。 */
        StoreRecord_t keep[2]; bool have[2] = {false, false};
        for (unsigned source = 0; source < 2; ++source) {
            for (uint32_t offset = 0; offset < store->next_offset[source]; offset += STORE_SLOT_BYTES) {
                StoreRecord_t record;
                if (!store->backend.read(store->backend.context, source, offset, &record, sizeof(record))) return false;
                if (valid(&record)) {
                    unsigned index = record.type - 1;
                    if (!have[index] || newer(record.sequence, keep[index].sequence)) { keep[index] = record; have[index] = true; }
                }
            }
        }
        bank ^= 1U;
        if (!store->backend.erase(store->backend.context, bank)) return false;
        store->next_offset[bank] = 0;
        for (unsigned i = 0; i < 2; ++i) {
            if (have[i]) {
                keep[i].reserved = 1; /* 原序号保持不变，只有完整追加才切换 active_bank。 */
                uint32_t offset = store->next_offset[bank]; store->next_offset[bank] += STORE_SLOT_BYTES;
                if (!write_record(store, bank, offset, &keep[i])) return false;
            }
        }
    }
    StoreRecord_t record;
    memset(&record, 0, sizeof(record)); record.type = type; record.version = version; record.length = length;
    memcpy(record.payload, payload, length);
    uint32_t offset = store->next_offset[bank]; store->next_offset[bank] += STORE_SLOT_BYTES;
    bool ok = write_record(store, bank, offset, &record);
    if (ok) store->active_bank = bank;
    /* 写入失败后重新扫描，恢复搬移/部分写入后的真实位置和序号。 */
    if (!ok) { StoreBackend_t backend = store->backend; (void)PersistentStore_Init(store, &backend); }
    return ok;
}
