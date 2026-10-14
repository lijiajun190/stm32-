/** 二进制帧采用显式小端序，不能直接发送含指针或编译器填充的结构。 */
#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#define PROTOCOL_VERSION 1U
#define PROTOCOL_PAYLOAD_MAX 256U
#define PROTOCOL_FRAME_MAX (PROTOCOL_PAYLOAD_MAX + 12U)
enum { CMD_INFO = 1, CMD_BEGIN = 0x10, CMD_DATA = 0x11, CMD_COMMIT = 0x12,
       CMD_ABORT = 0x13, CMD_RUN = 0x14, CMD_STATUS = 0x20, CMD_GET_CONFIG = 0x21,
       CMD_SET_CONFIG = 0x22, CMD_MONITOR = 0x23, CMD_STOP = 0x24, CMD_BOOT = 0x30 };
enum { REPLY_OK, REPLY_BAD_ARGUMENT, REPLY_BUSY, REPLY_BAD_CRC, REPLY_FLASH_ERROR,
       REPLY_BAD_STATE, REPLY_UNSUPPORTED };
typedef struct {
    uint8_t command;
    uint16_t sequence, length;
    uint8_t payload[PROTOCOL_PAYLOAD_MAX];
} ProtocolFrame_t;
typedef struct {
    uint8_t bytes[PROTOCOL_FRAME_MAX];
    uint16_t used;
    uint32_t last_byte_ms, crc_errors, format_errors;
} ProtocolParser_t;
void Protocol_Init(ProtocolParser_t *parser);
bool Protocol_Feed(ProtocolParser_t *parser, uint8_t byte, uint32_t now_ms, ProtocolFrame_t *frame);
uint16_t Protocol_Encode(const ProtocolFrame_t *frame, uint8_t *bytes, uint16_t capacity);
uint32_t Protocol_Get32(const uint8_t *bytes);
void Protocol_Put32(uint8_t *bytes, uint32_t value);
#endif
