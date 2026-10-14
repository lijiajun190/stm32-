#include "boot_update.h"
#include "serial_protocol.h"
#include "crc.h"
#include <string.h>
static void feed(BootUpdate_t *u) { if (u->backend.feed_watchdog) u->backend.feed_watchdog(u->backend.context); }
void BootUpdate_Init(BootUpdate_t *u, const BootBackend_t *backend)
{ memset(u, 0, sizeof(*u)); u->backend = *backend; }
static bool checksum(BootUpdate_t *u, unsigned area, uint32_t length, uint32_t expected)
{
    uint8_t bytes[128]; uint32_t state = 0xFFFFFFFFU;
    for (uint32_t offset = 0; offset < length;) {
        uint32_t count = length - offset; if (count > sizeof(bytes)) count = sizeof(bytes);
        if (!u->backend.read(u->backend.context, area, offset, bytes, count)) return false;
        state = CRC32_Update(state, bytes, count); offset += count; feed(u);
    }
    return ~state == expected;
}
static bool vectors_valid(BootUpdate_t *u, unsigned area, uint32_t length)
{
    uint8_t vectors[8];
    if (!u->backend.read(u->backend.context, area, 0, vectors, sizeof(vectors))) return false;
    uint32_t stack = Protocol_Get32(vectors), entry = Protocol_Get32(vectors + 4);
    return stack > 0x20000000U && stack <= 0x20020000U && (stack & 7U) == 0 &&
           (entry & 1U) && (entry & ~1U) >= BOOT_APP_BASE && (entry & ~1U) < BOOT_APP_BASE + length;
}
bool BootUpdate_ImageValid(BootUpdate_t *u, unsigned area, BootImage_t *image)
{
    uint8_t footer[BOOT_FOOTER_BYTES];
    if (area > BOOT_AREA_STAGE || !u->backend.read(u->backend.context, area, BOOT_IMAGE_MAX, footer, sizeof(footer))) return false;
    if (Protocol_Get32(footer) != BOOT_IMAGE_MAGIC || Protocol_Get32(footer + 24) != BOOT_IMAGE_COMMIT ||
        Protocol_Get32(footer + 20) != CRC32_Calculate(footer, 16)) return false;
    BootImage_t found;
    found.length = Protocol_Get32(footer + 4); found.crc = Protocol_Get32(footer + 8);
    found.version = Protocol_Get32(footer + 12);
    if (found.length < 8 || found.length > BOOT_IMAGE_MAX || !vectors_valid(u, area, found.length) ||
        !checksum(u, area, found.length, found.crc)) return false;
    if (image) *image = found;
    return true;
}
uint8_t BootUpdate_Begin(BootUpdate_t *u, const BootImage_t *image)
{
    if (!image || image->length < 8 || image->length > BOOT_IMAGE_MAX) return REPLY_BAD_ARGUMENT;
    if (u->receiving && memcmp(image, &u->image, sizeof(*image)) == 0) return REPLY_OK;
    u->receiving = false; feed(u);
    if (!u->backend.erase(u->backend.context, BOOT_AREA_STAGE)) return REPLY_FLASH_ERROR;
    u->image = *image; u->received = 0; u->receiving = true; return REPLY_OK;
}
uint8_t BootUpdate_Data(BootUpdate_t *u, uint32_t offset, const uint8_t *bytes, uint32_t length)
{
    if (!u->receiving) return REPLY_BAD_STATE;
    if (!bytes || !length || length > 252U || offset % 4U || offset > u->image.length ||
        length > u->image.length - offset || (length % 4U && offset + length != u->image.length)) return REPLY_BAD_ARGUMENT;
    if (offset < u->received) {
        uint8_t old[252];
        if (length > u->received - offset) return REPLY_BAD_ARGUMENT;
        return u->backend.read(u->backend.context, BOOT_AREA_STAGE, offset, old, length) &&
            memcmp(old, bytes, length) == 0 ? REPLY_OK : REPLY_BAD_ARGUMENT;
    }
    if (offset != u->received) return REPLY_BAD_ARGUMENT;
    for (uint32_t i = 0; i < length; i += 4) {
        uint8_t word[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        uint32_t count = length - i; if (count > 4) count = 4;
        memcpy(word, bytes + i, count);
        uint8_t existing[4];
        if (!u->backend.read(u->backend.context, BOOT_AREA_STAGE, offset + i, existing, 4)) return REPLY_FLASH_ERROR;
        if (memcmp(existing, word, 4) == 0) continue; /* ACK 丢失或半写失败后的幂等重发。 */
        if (!u->backend.program(u->backend.context, BOOT_AREA_STAGE, offset + i, Protocol_Get32(word)))
            return REPLY_FLASH_ERROR;
        uint8_t verify[4];
        if (!u->backend.read(u->backend.context, BOOT_AREA_STAGE, offset + i, verify, 4) || memcmp(word, verify, 4))
            return REPLY_FLASH_ERROR;
    }
    u->received += length; feed(u); return REPLY_OK;
}
static bool write_footer(BootUpdate_t *u, unsigned area, const BootImage_t *image)
{
    uint8_t footer[BOOT_FOOTER_BYTES]; memset(footer, 0xFF, sizeof(footer));
    Protocol_Put32(footer, BOOT_IMAGE_MAGIC); Protocol_Put32(footer + 4, image->length);
    Protocol_Put32(footer + 8, image->crc); Protocol_Put32(footer + 12, image->version);
    Protocol_Put32(footer + 20, CRC32_Calculate(footer, 16)); Protocol_Put32(footer + 24, BOOT_IMAGE_COMMIT);
    /* 最后一步是 commit，任何半写尾部均不作为启动镜像。 */
    for (unsigned i = 0; i <= 24; i += 4)
        if (!u->backend.program(u->backend.context, area, BOOT_IMAGE_MAX + i, Protocol_Get32(footer + i))) return false;
    return BootUpdate_ImageValid(u, area, NULL);
}
uint8_t BootUpdate_Commit(BootUpdate_t *u)
{
    if (!u->receiving) return BootUpdate_ImageValid(u, BOOT_AREA_STAGE, NULL) ? REPLY_OK : REPLY_BAD_STATE;
    if (u->received != u->image.length) return REPLY_BAD_STATE;
    if (!checksum(u, BOOT_AREA_STAGE, u->image.length, u->image.crc)) return REPLY_BAD_CRC;
    if (!vectors_valid(u, BOOT_AREA_STAGE, u->image.length)) return REPLY_BAD_ARGUMENT;
    if (!write_footer(u, BOOT_AREA_STAGE, &u->image)) return REPLY_FLASH_ERROR;
    u->receiving = false; return REPLY_OK;
}
uint8_t BootUpdate_Abort(BootUpdate_t *u)
{
    u->receiving = false; u->received = 0;
    return u->backend.erase(u->backend.context, BOOT_AREA_STAGE) ? REPLY_OK : REPLY_FLASH_ERROR;
}
bool BootUpdate_Recover(BootUpdate_t *u)
{
    BootImage_t image; uint8_t state[4];
    bool app_valid = BootUpdate_ImageValid(u, BOOT_AREA_APP, NULL);
    if (BootUpdate_ImageValid(u, BOOT_AREA_STAGE, &image) &&
        u->backend.read(u->backend.context, BOOT_AREA_STAGE, BOOT_IMAGE_MAX + 16U, state, 4) &&
        (Protocol_Get32(state) != 0U || !app_valid)) {
        feed(u);
        if (!u->backend.erase(u->backend.context, BOOT_AREA_APP)) return false;
        uint8_t bytes[128];
        for (uint32_t offset = 0; offset < image.length; offset += sizeof(bytes)) {
            uint32_t count = image.length - offset; if (count > sizeof(bytes)) count = sizeof(bytes);
            memset(bytes, 0xFF, sizeof(bytes));
            if (!u->backend.read(u->backend.context, BOOT_AREA_STAGE, offset, bytes, count)) return false;
            for (uint32_t i = 0; i < count; i += 4)
                if (!u->backend.program(u->backend.context, BOOT_AREA_APP, offset + i, Protocol_Get32(bytes + i))) return false;
            feed(u);
        }
        if (!write_footer(u, BOOT_AREA_APP, &image)) return false;
        /* 安装状态只在应用再次通过 CRC 校验后落盘。状态半写时下次仍重装。 */
        if (!u->backend.program(u->backend.context, BOOT_AREA_STAGE, BOOT_IMAGE_MAX + 16U, 0U)) return false;
    }
    return BootUpdate_ImageValid(u, BOOT_AREA_APP, NULL);
}
