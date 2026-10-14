/** @file motor_open_loop.c @brief 非阻塞开环测试，每个档位停留完整指定时间。 */
#include "motor_open_loop.h"
#include <stddef.h>

static const uint16_t test_duties[] = {0U, 300U, 600U, 1000U, 0U};

bool Motor_OpenLoop_Start(Motor_OpenLoop_t *test, uint32_t now_ms, uint32_t dwell_ms)
{
    if (test == NULL || dwell_ms < MOTOR_OPEN_LOOP_MIN_DWELL_MS ||
        dwell_ms > MOTOR_OPEN_LOOP_MAX_DWELL_MS) {
        return false;
    }
    test->active = true;
    test->step = 0U;
    test->step_start_ms = now_ms;
    test->dwell_ms = dwell_ms;
    return true;
}

void Motor_OpenLoop_Stop(Motor_OpenLoop_t *test)
{
    if (test != NULL) {
        test->active = false;
    }
}

bool Motor_OpenLoop_Poll(Motor_OpenLoop_t *test, uint32_t now_ms, uint16_t *duty)
{
    if (test == NULL || duty == NULL || !test->active ||
        now_ms - test->step_start_ms < test->dwell_ms) {
        return false;
    }
    ++test->step;
    *duty = test_duties[test->step];
    /* 不追赶漏掉的档位：即使任务被延迟，每个输出档位仍完整停留 dwell_ms。 */
    test->step_start_ms = now_ms;
    if (test->step == sizeof(test_duties) / sizeof(test_duties[0]) - 1U) {
        test->active = false;
    }
    return true;
}
