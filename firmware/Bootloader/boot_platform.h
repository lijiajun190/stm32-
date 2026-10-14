#ifndef BOOT_PLATFORM_H
#define BOOT_PLATFORM_H
#include "boot_update.h"
void BootPlatform_Init(void);
BootBackend_t BootPlatform_Backend(void);
bool BootPlatform_Requested(void);
bool BootPlatform_ReadByte(uint8_t *byte);
bool BootPlatform_Send(const uint8_t *bytes, uint16_t length);
void BootPlatform_Feed(void);
void BootPlatform_Jump(void);
#endif
