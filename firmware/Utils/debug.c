/**
 * @file    debug.c
 * @brief   调试工具实现
 */

#include "debug.h"

/**
 * @brief  初始化调试功能
 */
void Debug_Init(void)
{
    printf("[DEBUG] Debug module initialized (Level: %d)\r\n", DEBUG_LEVEL);
}

/**
 * @brief  打印十六进制数据
 */
void Debug_PrintHex(const uint8_t *data, uint16_t length)
{
    printf("HEX[%d]: ", length);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\r\n         ");
        }
    }
    printf("\r\n");
}

/**
 * @brief  打印内存内容
 */
void Debug_PrintMemory(uint32_t address, uint16_t length)
{
    uint8_t *ptr = (uint8_t *)address;
    printf("Memory[0x%08lX]:\r\n", address);

    for (uint16_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("0x%08lX: ", address + i);
        }
        printf("%02X ", ptr[i]);
        if ((i + 1) % 16 == 0) {
            printf("\r\n");
        }
    }
    if (length % 16 != 0) {
        printf("\r\n");
    }
}
