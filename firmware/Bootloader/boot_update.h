/** 不依赖 HAL 的暂存校验/安装引擎，Flash 后端可注入掉电。 */
#ifndef BOOT_UPDATE_H
#define BOOT_UPDATE_H
#include <stdint.h>
#include <stdbool.h>
#include "boot_layout.h"
enum { BOOT_AREA_APP, BOOT_AREA_STAGE };
typedef struct {
    void *context;
    bool (*read)(void *, unsigned, uint32_t, void *, uint32_t);
    bool (*program)(void *, unsigned, uint32_t, uint32_t);
    bool (*erase)(void *, unsigned);
    void (*feed_watchdog)(void *);
} BootBackend_t;
typedef struct {
    uint32_t length, crc, version;
} BootImage_t;
typedef struct {
    BootBackend_t backend;
    BootImage_t image;
    uint32_t received;
    bool receiving;
} BootUpdate_t;
void BootUpdate_Init(BootUpdate_t *update, const BootBackend_t *backend);
bool BootUpdate_ImageValid(BootUpdate_t *update, unsigned area, BootImage_t *image);
uint8_t BootUpdate_Begin(BootUpdate_t *update, const BootImage_t *image);
uint8_t BootUpdate_Data(BootUpdate_t *update, uint32_t offset, const uint8_t *bytes, uint32_t length);
uint8_t BootUpdate_Commit(BootUpdate_t *update);
uint8_t BootUpdate_Abort(BootUpdate_t *update);
/* 暂存已提交且尚未安装时，重启后完整重试；绝不跳转到未提交应用。 */
bool BootUpdate_Recover(BootUpdate_t *update);
#endif
