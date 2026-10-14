#ifndef BSP_STORAGE_H
#define BSP_STORAGE_H
#include "persistent_store.h"
StoreBackend_t Bsp_Storage_Backend(void);
/* 单 Bank 擦写只能在维护停机窗口使用；不在异常上下文调用。 */
bool Bsp_Storage_Unlock(void);
void Bsp_Storage_Lock(void);
#endif
