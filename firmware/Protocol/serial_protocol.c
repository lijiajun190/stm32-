#include "serial_protocol.h"
#include "crc.h"
#include <string.h>
uint32_t Protocol_Get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
void Protocol_Put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8U * i)); }
void Protocol_Init(ProtocolParser_t *parser) { memset(parser, 0, sizeof(*parser)); }
static void discard(ProtocolParser_t *parser, uint16_t count)
{ parser->used -= count; memmove(parser->bytes, parser->bytes + count, parser->used); }
bool Protocol_Feed(ProtocolParser_t *parser, uint8_t byte, uint32_t now_ms, ProtocolFrame_t *frame)
{
    if (parser->used && (uint32_t)(now_ms - parser->last_byte_ms) > 1500U) parser->used = 0;
    parser->last_byte_ms = now_ms;
    if (parser->used == PROTOCOL_FRAME_MAX) discard(parser, 1);
    parser->bytes[parser->used++] = byte;
    while (parser->used) {
        if (parser->bytes[0] != 0xA5U) { discard(parser, 1); continue; }
        if (parser->used < 2) return false;
        if (parser->bytes[1] != 0x5AU) { discard(parser, 1); continue; }
        if (parser->used < 8) return false;
        uint16_t length = (uint16_t)(parser->bytes[6] | (uint16_t)parser->bytes[7] << 8);
        if (parser->bytes[2] != PROTOCOL_VERSION || length > PROTOCOL_PAYLOAD_MAX) {
            ++parser->format_errors; discard(parser, 1); continue;
        }
        if (parser->used < length + 12U) return false;
        if (CRC32_Calculate(parser->bytes + 2, length + 6U) != Protocol_Get32(parser->bytes + 8U + length)) {
            ++parser->crc_errors; discard(parser, 1); continue;
        }
        frame->command = parser->bytes[3];
        frame->sequence = (uint16_t)(parser->bytes[4] | (uint16_t)parser->bytes[5] << 8);
        frame->length = length; memcpy(frame->payload, parser->bytes + 8, length);
        discard(parser, length + 12U); return true;
    }
    return false;
}
uint16_t Protocol_Encode(const ProtocolFrame_t *frame, uint8_t *bytes, uint16_t capacity)
{
    if (!frame || !bytes || frame->length > PROTOCOL_PAYLOAD_MAX || capacity < frame->length + 12U) return 0;
    bytes[0] = 0xA5; bytes[1] = 0x5A; bytes[2] = PROTOCOL_VERSION; bytes[3] = frame->command;
    bytes[4] = (uint8_t)frame->sequence; bytes[5] = (uint8_t)(frame->sequence >> 8);
    bytes[6] = (uint8_t)frame->length; bytes[7] = (uint8_t)(frame->length >> 8);
    memcpy(bytes + 8, frame->payload, frame->length);
    Protocol_Put32(bytes + 8U + frame->length, CRC32_Calculate(bytes + 2, frame->length + 6U));
    return frame->length + 12U;
}
