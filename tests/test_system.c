/* 主机测试：配置边界、任务心跳和双槽持久化日志的掉电/损坏行为。 */
#include "heartbeat.h"
#include "system_config.h"
#include "persistent_store.h"
#include "system_persist.h"
#include "crc.h"
#include "control_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <math.h>

#define BANK_BYTES (4U * STORE_SLOT_BYTES)
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); return 1; } } while (0)
typedef struct {
    unsigned char bytes[2][BANK_BYTES];
    unsigned erase_count, programmed_words;
    int cut_after_words, torn_bytes, erase_cut_bytes;
    bool fail_program;
} MemStore_t;
/* longjmp 模拟直接掉电，不执行 Append 的失败清理；重新初始化等同下一次上电。 */
static jmp_buf power_loss;
static MemStore_t crash_memory;
static PersistentStore_t crash_store;
static bool read_mem(void *ctx, unsigned bank, uint32_t offset, void *data, uint32_t length)
{
    MemStore_t *mem = (MemStore_t *)ctx; if (bank > 1 || offset + length > BANK_BYTES) return false;
    memcpy(data, &mem->bytes[bank][offset], length); return true;
}
static bool program_mem(void *ctx, unsigned bank, uint32_t offset, uint32_t word)
{
    MemStore_t *mem = (MemStore_t *)ctx; if (bank > 1 || offset + 4 > BANK_BYTES || mem->fail_program) return false;
    unsigned char *dst = &mem->bytes[bank][offset]; const unsigned char *src = (const unsigned char *)&word;
    if (mem->cut_after_words >= 0 && mem->programmed_words == (unsigned)mem->cut_after_words) {
        /* 一个 word 尚未完全编程时掉电：只改变前 0..3 bytes。 */
        for (int i = 0; i < mem->torn_bytes; ++i) dst[i] &= src[i];
        longjmp(power_loss, 1);
    }
    for (unsigned i = 0; i < 4; ++i) { if ((~dst[i]) & src[i]) return false; dst[i] &= src[i]; }
    ++mem->programmed_words;
    return true;
}
static bool erase_mem(void *ctx, unsigned bank)
{
    MemStore_t *mem = (MemStore_t *)ctx; if (bank > 1) return false;
    if (mem->erase_cut_bytes >= 0) {
        memset(mem->bytes[bank], 0xFF, (unsigned)mem->erase_cut_bytes);
        longjmp(power_loss, 1);
    }
    memset(mem->bytes[bank], 0xFF, BANK_BYTES); ++mem->erase_count; return true;
}
static void reset_mem(MemStore_t *mem)
{
    memset(mem, 0xFF, sizeof(*mem)); mem->erase_count = 0; mem->programmed_words = 0;
    mem->fail_program = false; mem->cut_after_words = -1; mem->torn_bytes = 0; mem->erase_cut_bytes = -1;
}
static StoreBackend_t backend_for(MemStore_t *mem)
{
    StoreBackend_t backend;
    backend.context = mem; backend.bank_bytes = BANK_BYTES;
    backend.read = read_mem; backend.program = program_mem; backend.erase = erase_mem;
    return backend;
}
static int test_power_loss(void)
{
    MemStore_t initial;
    PersistentStore_t initial_store;
    SystemConfig_t old_config, new_config, loaded;
    FaultRecord_t old_fault = {0}, loaded_fault;
    StoreBackend_t backend = backend_for(&initial);
    SystemConfig_Defaults(&old_config); new_config = old_config; new_config.manual_rpm = 1200;
    old_fault.reason = FAULT_REASON_MOTOR; old_fault.detail = 0x1234;
    /* append、首次搬移、反向搬移、已有损坏槽：每个写入边界都重启恢复。 */
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        reset_mem(&initial);
        CHECK(PersistentStore_Init(&initial_store, &backend));
        CHECK(PersistentStore_Append(&initial_store, STORE_TYPE_CONFIG, 1, &old_config, sizeof(old_config)));
        CHECK(PersistentStore_Append(&initial_store, STORE_TYPE_FAULT, 1, &old_fault, sizeof(old_fault)));
        if (scenario != 0) {
            while (initial_store.next_offset[initial_store.active_bank] < BANK_BYTES)
                CHECK(PersistentStore_Append(&initial_store, STORE_TYPE_CONFIG, 1, &old_config, sizeof(old_config)));
        }
        if (scenario >= 2) {
            /* 第一次搬移成功后填满 Bank 1，下一次需要擦除有旧记录的 Bank 0。 */
            CHECK(PersistentStore_Append(&initial_store, STORE_TYPE_CONFIG, 1, &old_config, sizeof(old_config)));
            CHECK(PersistentStore_Append(&initial_store, STORE_TYPE_CONFIG, 1, &old_config, sizeof(old_config)));
        }
        if (scenario == 3) {
            /* 同时留有非当前扇区的破损记录；扫描不能把它当成已提交参数。 */
            initial.bytes[0][20] ^= 1;
        }
        unsigned words = (scenario == 0 ? 1U : 3U) * (STORE_SLOT_BYTES / 4U);
        for (unsigned cut = 0; cut <= words; ++cut) {
            for (unsigned torn = 0; torn < (cut == words ? 1U : 4U); ++torn) {
                crash_memory = initial;
                StoreBackend_t crash_backend = backend_for(&crash_memory);
                CHECK(PersistentStore_Init(&crash_store, &crash_backend));
                unsigned previous_bank = crash_store.active_bank;
                uint32_t previous_sequence = crash_store.next_sequence;
                crash_memory.programmed_words = 0; crash_memory.cut_after_words = (int)cut;
                crash_memory.torn_bytes = (int)torn;
                if (setjmp(power_loss) == 0)
                    (void)PersistentStore_Append(&crash_store, STORE_TYPE_CONFIG, 1, &new_config, sizeof(new_config));
                crash_memory.cut_after_words = -1;
                CHECK(PersistentStore_Init(&crash_store, &crash_backend));
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL));
                CHECK(loaded.manual_rpm == (cut == words ? new_config.manual_rpm : old_config.manual_rpm));
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_FAULT, 1, &loaded_fault, sizeof(loaded_fault), NULL));
                CHECK(memcmp(&old_fault, &loaded_fault, sizeof(old_fault)) == 0);
                CHECK(crash_store.next_sequence == previous_sequence + (cut == words ? 1U : 0U));
                CHECK(crash_store.active_bank == (cut == words && scenario != 0 ? previous_bank ^ 1U : previous_bank));
                /* 失败之后再次保存必须成功，且仍能保留另一个类型的数据。 */
                CHECK(PersistentStore_Append(&crash_store, STORE_TYPE_CONFIG, 1, &new_config, sizeof(new_config)));
                CHECK(PersistentStore_Init(&crash_store, &crash_backend));
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == 1200);
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_FAULT, 1, &loaded_fault, sizeof(loaded_fault), NULL));
                CHECK(memcmp(&old_fault, &loaded_fault, sizeof(old_fault)) == 0);
            }
        }
        if (scenario != 0) {
            for (unsigned erased = 0; erased <= BANK_BYTES; erased += 4) {
                crash_memory = initial;
                StoreBackend_t crash_backend = backend_for(&crash_memory);
                CHECK(PersistentStore_Init(&crash_store, &crash_backend));
                crash_memory.erase_cut_bytes = (int)erased;
                if (setjmp(power_loss) == 0)
                    (void)PersistentStore_Append(&crash_store, STORE_TYPE_CONFIG, 1, &new_config, sizeof(new_config));
                crash_memory.erase_cut_bytes = -1;
                CHECK(PersistentStore_Init(&crash_store, &crash_backend));
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == old_config.manual_rpm);
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_FAULT, 1, &loaded_fault, sizeof(loaded_fault), NULL));
                CHECK(memcmp(&old_fault, &loaded_fault, sizeof(old_fault)) == 0);
                CHECK(PersistentStore_Append(&crash_store, STORE_TYPE_CONFIG, 1, &new_config, sizeof(new_config)));
                CHECK(PersistentStore_Init(&crash_store, &crash_backend));
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == new_config.manual_rpm);
                CHECK(PersistentStore_ReadLatest(&crash_store, STORE_TYPE_FAULT, 1, &loaded_fault, sizeof(loaded_fault), NULL));
                CHECK(memcmp(&old_fault, &loaded_fault, sizeof(old_fault)) == 0);
            }
        }
    }
    return 0;
}
static int test_system_persist(void)
{
    MemStore_t memory; SystemPersist_t persist; SystemConfig_t config, loaded;
    FaultRecord_t fault = {0};
    reset_mem(&memory); StoreBackend_t backend = backend_for(&memory);
    CHECK(!SystemPersist_Init(NULL, &backend, &config));
    CHECK(!SystemPersist_Init(&persist, &backend, NULL));
    CHECK(SystemPersist_Init(&persist, &backend, &config));
    CHECK(persist.config_saved && persist.config_sequence == 1 && !persist.fault_valid);
    uint32_t sequence = persist.journal.next_sequence;
    CHECK(SystemPersist_SaveConfig(&persist, &config) && persist.journal.next_sequence == sequence);
    config.manual_rpm = 1400; CHECK(SystemPersist_SaveConfig(&persist, &config));
    fault.reason = FAULT_REASON_HARDFAULT; fault.registers[6] = 0x08001234;
    CHECK(SystemPersist_SaveFault(&persist, &fault));
    CHECK(SystemPersist_Init(&persist, &backend, &loaded));
    CHECK(loaded.manual_rpm == 1400 && persist.fault_valid && persist.latest_fault.registers[6] == 0x08001234);
    CHECK(!SystemPersist_SaveConfig(NULL, &config));
    CHECK(!SystemPersist_SaveFault(NULL, &fault));
    config.kp = NAN; CHECK(!SystemPersist_SaveConfig(&persist, &config));
    fault.reason = 99; CHECK(!SystemPersist_SaveFault(&persist, &fault));
    /* 最近配置 CRC 损坏时回退到上一条已提交配置，故障记录独立保留。 */
    memory.bytes[0][STORE_SLOT_BYTES + 20] ^= 1;
    CHECK(SystemPersist_Init(&persist, &backend, &loaded));
    CHECK(loaded.manual_rpm == CONTROL_DEFAULT_RPM && persist.fault_valid);
    /* 全部格式合法但值非法的配置仍要拒绝，重新保存默认值。 */
    loaded.manual_rpm = 1;
    CHECK(PersistentStore_Append(&persist.journal, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded)));
    CHECK(SystemPersist_Init(&persist, &backend, &loaded));
    CHECK(loaded.manual_rpm == CONTROL_DEFAULT_RPM && persist.config_saved);
    /* 序号从 UINT32_MAX 回绕，扫描和扇区选择仍按递增顺序。 */
    reset_mem(&memory); CHECK(PersistentStore_Init(&persist.journal, &backend));
    persist.journal.next_sequence = 0xFFFFFFFFUL;
    CHECK(PersistentStore_Append(&persist.journal, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded)));
    loaded.manual_rpm = 1100;
    CHECK(PersistentStore_Append(&persist.journal, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded)));
    CHECK(SystemPersist_Init(&persist, &backend, &config));
    CHECK(config.manual_rpm == 1100 && persist.config_sequence == 0 && persist.journal.next_sequence == 1);
    return 0;
}
int main(void)
{
    MemStore_t memory; StoreBackend_t backend; PersistentStore_t store; SystemConfig_t config, loaded;
    Heartbeat_t heartbeat; reset_mem(&memory);
    backend.context = &memory; backend.bank_bytes = BANK_BYTES; backend.read = read_mem; backend.program = program_mem; backend.erase = erase_mem;
    CHECK(PersistentStore_Init(&store, &backend));
    CHECK(!PersistentStore_ReadLatest(&store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL));
    SystemConfig_Defaults(&config); CHECK(SystemConfig_Validate(&config));
    CHECK(PersistentStore_Append(&store, STORE_TYPE_CONFIG, 1, &config, sizeof(config)));
    config.manual_rpm = 900; CHECK(PersistentStore_Append(&store, STORE_TYPE_CONFIG, 1, &config, sizeof(config)));
    CHECK(PersistentStore_ReadLatest(&store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == 900);
    CHECK(PersistentStore_Init(&store, &backend));
    CHECK(PersistentStore_ReadLatest(&store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == 900);
    config.manual_rpm = 1000;
    while (store.next_offset[store.active_bank] < BANK_BYTES) CHECK(PersistentStore_Append(&store, STORE_TYPE_CONFIG, 1, &config, sizeof(config)));
    CHECK(PersistentStore_Append(&store, STORE_TYPE_CONFIG, 1, &config, sizeof(config)) && memory.erase_count > 0);
    CHECK(PersistentStore_Init(&store, &backend));
    CHECK(PersistentStore_ReadLatest(&store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == 1000);
    memory.fail_program = true; config.manual_rpm = 1100;
    CHECK(!PersistentStore_Append(&store, STORE_TYPE_CONFIG, 1, &config, sizeof(config)));
    memory.fail_program = false; CHECK(PersistentStore_Init(&store, &backend));
    CHECK(PersistentStore_ReadLatest(&store, STORE_TYPE_CONFIG, 1, &loaded, sizeof(loaded), NULL) && loaded.manual_rpm == 1000);
    config.manual_rpm = 1; CHECK(!SystemConfig_Validate(&config));
    config.manual_rpm = CONTROL_DEFAULT_RPM; config.kp = -1; CHECK(!SystemConfig_Validate(&config));
    SystemConfig_Defaults(&config); config.gas_high_v = config.gas_low_v; CHECK(!SystemConfig_Validate(&config));
    Heartbeat_Init(&heartbeat, 0); CHECK(!Heartbeat_Ready(&heartbeat, 4999));
    for (unsigned id = 0; id < HEARTBEAT_COUNT; ++id) Heartbeat_Report(&heartbeat, (Heartbeat_ID_t)id, 100);
    CHECK(Heartbeat_Ready(&heartbeat, 200) && Heartbeat_Missing(&heartbeat, 200) == 0);
    CHECK((Heartbeat_Missing(&heartbeat, 601) & (1UL << HEARTBEAT_MOTOR)) != 0);
    Heartbeat_Init(&heartbeat, 0xFFFFFF00UL);
    for (unsigned id = 0; id < HEARTBEAT_COUNT; ++id) Heartbeat_Report(&heartbeat, (Heartbeat_ID_t)id, 0xFFFFFF00UL);
    CHECK(Heartbeat_Missing(&heartbeat, 0xFFFFFFE0UL) == 0);
    Heartbeat_Report(&heartbeat, (Heartbeat_ID_t)99, 0); CHECK(heartbeat.seen_mask == HEARTBEAT_ALL);
    /* 时钟越过零点后的真实超时，及未运行任务在启动宽限结束时的位图。 */
    CHECK(Heartbeat_Missing(&heartbeat, 0x20UL) == (1UL << HEARTBEAT_MOTOR));
    Heartbeat_Init(&heartbeat, 0);
    CHECK(Heartbeat_Missing(&heartbeat, HEARTBEAT_START_GRACE_MS - 1) == 0);
    CHECK(Heartbeat_Missing(&heartbeat, HEARTBEAT_START_GRACE_MS) == HEARTBEAT_ALL);
    if (test_power_loss() || test_system_persist()) return 1;
    printf("PASS: %u persistence/config/heartbeat checks\n", checks); return 0;
}
