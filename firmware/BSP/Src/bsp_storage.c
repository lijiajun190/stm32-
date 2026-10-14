#include "bsp_storage.h"
#include "main.h"
#include <string.h>
#ifdef APP_UPGRADE_LAYOUT
#define STORAGE_BASE 0x08008000UL
#define STORAGE_BANK_BYTES 0x4000UL
#define STORAGE_SECTOR_0 FLASH_SECTOR_2
#define STORAGE_SECTOR_1 FLASH_SECTOR_3
#else
#define STORAGE_BASE 0x08040000UL
#define STORAGE_BANK_BYTES 0x20000UL
#define STORAGE_SECTOR_0 FLASH_SECTOR_6
#define STORAGE_SECTOR_1 FLASH_SECTOR_7
#endif
static bool storage_unlocked, restore_data_cache;
static bool read_bank(void *context, unsigned bank, uint32_t offset, void *data, uint32_t length)
{
    (void)context;
    if (bank > 1 || offset > STORAGE_BANK_BYTES || length > STORAGE_BANK_BYTES - offset) return false;
    memcpy(data, (const void *)(STORAGE_BASE + bank * STORAGE_BANK_BYTES + offset), length);
    return true;
}
static bool program_bank(void *context, unsigned bank, uint32_t offset, uint32_t value)
{
    (void)context;
    if (bank > 1 || offset % 4 || offset > STORAGE_BANK_BYTES - 4) return false;
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORAGE_BASE + bank * STORAGE_BANK_BYTES + offset, value) == HAL_OK;
}
static bool erase_bank(void *context, unsigned bank)
{
    (void)context;
    if (bank > 1) return false;
    FLASH_EraseInitTypeDef erase = {0}; uint32_t error;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = bank == 0 ? STORAGE_SECTOR_0 : STORAGE_SECTOR_1;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* 当前板供电 3.3 V */
    return HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;
}
StoreBackend_t Bsp_Storage_Backend(void)
{
    StoreBackend_t backend = {0}; backend.bank_bytes = STORAGE_BANK_BYTES;
    backend.read = read_bank; backend.program = program_bank; backend.erase = erase_bank;
    return backend;
}
bool Bsp_Storage_Unlock(void)
{
    if (storage_unlocked) return false;
    if (HAL_FLASH_Unlock() != HAL_OK) return false;
    /* 扫描空槽可能将 0xFF 留在 Flash 数据缓存中。轮询式 HAL_FLASH_Program
       不主动刷新缓存，事务期间禁用它，让逐记录回读校验看到真实 Flash。 */
    restore_data_cache = (FLASH->ACR & FLASH_ACR_DCEN) != 0;
    if (restore_data_cache) __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET();
    __DSB(); __ISB();
    storage_unlocked = true;
    return true;
}
void Bsp_Storage_Lock(void)
{
    (void)HAL_FLASH_Lock();
    if (storage_unlocked) {
        __HAL_FLASH_DATA_CACHE_RESET();
        if (restore_data_cache) __HAL_FLASH_DATA_CACHE_ENABLE();
        __DSB(); __ISB();
        storage_unlocked = false;
    }
}
