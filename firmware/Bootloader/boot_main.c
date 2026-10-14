#include "boot_platform.h"
#include "serial_protocol.h"
#include "stm32f4xx_hal.h"
static BootUpdate_t update;
static ProtocolParser_t parser;
static ProtocolFrame_t request, reply;
int main(void)
{
    BootPlatform_Init(); BootBackend_t backend = BootPlatform_Backend();
    BootUpdate_Init(&update, &backend); Protocol_Init(&parser);
    bool stay = BootPlatform_Requested();
    bool app_valid = BootUpdate_Recover(&update);
    uint32_t started = HAL_GetTick();
    for (;;) {
        BootPlatform_Feed(); uint8_t byte;
        if (!stay && app_valid && (uint32_t)(HAL_GetTick() - started) >= 1500U) BootPlatform_Jump();
        if (!BootPlatform_ReadByte(&byte) || !Protocol_Feed(&parser, byte, HAL_GetTick(), &request)) continue;
        stay = true; reply.command = request.command | 0x80U; reply.sequence = request.sequence;
        reply.length = 1; reply.payload[0] = REPLY_OK;
        bool run = false;
        if (request.command != CMD_BEGIN && request.command != CMD_DATA && request.length) reply.payload[0] = REPLY_BAD_ARGUMENT;
        else switch (request.command) {
            case CMD_INFO: {
                BootImage_t app = {0}, stage = {0};
                app_valid = BootUpdate_ImageValid(&update, BOOT_AREA_APP, &app);
                bool stage_valid = BootUpdate_ImageValid(&update, BOOT_AREA_STAGE, &stage);
                uint32_t values[] = {0U, 1U, BOOT_APP_BASE, BOOT_IMAGE_MAX, 252U,
                    app_valid, app.version, stage_valid, stage.version, update.received};
                for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
                    Protocol_Put32(reply.payload + 1U + i * 4U, values[i]);
                reply.length += sizeof(values); break;
            }
            case CMD_BEGIN: {
                if (request.length != 12) { reply.payload[0] = REPLY_BAD_ARGUMENT; break; }
                BootImage_t image; image.length = Protocol_Get32(request.payload);
                image.crc = Protocol_Get32(request.payload + 4); image.version = Protocol_Get32(request.payload + 8);
                reply.payload[0] = BootUpdate_Begin(&update, &image); break;
            }
            case CMD_DATA:
                if (request.length <= 4) reply.payload[0] = REPLY_BAD_ARGUMENT;
                else reply.payload[0] = BootUpdate_Data(&update, Protocol_Get32(request.payload), request.payload + 4, request.length - 4U);
                Protocol_Put32(reply.payload + 1, update.received); reply.length += 4; break;
            case CMD_COMMIT: reply.payload[0] = BootUpdate_Commit(&update); break;
            case CMD_ABORT: reply.payload[0] = BootUpdate_Abort(&update); break;
            case CMD_RUN:
                if (update.receiving) reply.payload[0] = REPLY_BAD_STATE;
                else if (!BootUpdate_ImageValid(&update, BOOT_AREA_STAGE, NULL) &&
                         !BootUpdate_ImageValid(&update, BOOT_AREA_APP, NULL)) reply.payload[0] = REPLY_BAD_STATE;
                else run = true;
                break;
            default: reply.payload[0] = REPLY_UNSUPPORTED; break;
        }
        uint8_t encoded[PROTOCOL_FRAME_MAX];
        uint16_t length = Protocol_Encode(&reply, encoded, sizeof(encoded));
        if (BootPlatform_Send(encoded, length) && run && BootUpdate_Recover(&update)) BootPlatform_Jump();
    }
}
