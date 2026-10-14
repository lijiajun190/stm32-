/**
 * @file    debug.h
 * @brief   调试工具
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stdio.h>

/* 调试级别 */
#define DEBUG_LEVEL_NONE     0
#define DEBUG_LEVEL_ERROR    1
#define DEBUG_LEVEL_WARN     2
#define DEBUG_LEVEL_INFO     3
#define DEBUG_LEVEL_DEBUG    4

/* 当前调试级别（可在编译时修改） */
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_INFO
#endif

/* 调试宏 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
    #define DEBUG_ERROR(fmt, ...) printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define DEBUG_ERROR(fmt, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_WARN
    #define DEBUG_WARN(fmt, ...) printf("[WARN] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define DEBUG_WARN(fmt, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_INFO
    #define DEBUG_INFO(fmt, ...) printf("[INFO] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define DEBUG_INFO(fmt, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
    #define DEBUG_PRINT(fmt, ...) printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

/* 断言宏 */
#ifdef USE_FULL_ASSERT
    #define ASSERT(expr) \
        do { \
            if (!(expr)) { \
                printf("ASSERT FAILED: %s:%d\r\n", __FILE__, __LINE__); \
                while(1); \
            } \
        } while(0)
#else
    #define ASSERT(expr)
#endif

/* 函数声明 */
void Debug_Init(void);
void Debug_PrintHex(const uint8_t *data, uint16_t length);
void Debug_PrintMemory(uint32_t address, uint16_t length);

#endif /* DEBUG_H */
