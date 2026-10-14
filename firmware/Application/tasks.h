/**
 * @file    tasks.h
 * @brief   FreeRTOS 任务定义
 */

#ifndef TASKS_H
#define TASKS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "motor.h"
#include "bsp_timer.h"
#include "motor_open_loop.h"
#include "sensor_data.h"
#include "control_config.h"
#include "state_machine.h"
#include "motor_control.h"
#include "heartbeat.h"
#include "runtime_monitor.h"

/* 任务优先级 */
#define TASK_PRIORITY_WATCHDOG      (configMAX_PRIORITIES - 1)
#define TASK_PRIORITY_MOTOR_CTRL    (tskIDLE_PRIORITY + 4)
#define TASK_PRIORITY_SENSOR        (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_SYSTEM_MGR    (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_UI            (tskIDLE_PRIORITY + 2)
#define TASK_PRIORITY_LOGGER        (tskIDLE_PRIORITY + 1)

/* 任务周期 */
#define TASK_PERIOD_MOTOR_CTRL_MS   50
#define TASK_PERIOD_SENSOR_MS       100
#define TASK_PERIOD_UI_MS           100
#define TASK_PERIOD_INPUT_SCAN_MS   10
#define TASK_PERIOD_SYSTEM_MGR_MS   100
#define TASK_PERIOD_WATCHDOG_MS     100
#define TASK_PERIOD_LOGGER_MS       1000

/* 阶段四固定的开环验证档位，单位为千分比。 */
#define MOTOR_OPEN_LOOP_DUTY_0      0U
#define MOTOR_OPEN_LOOP_DUTY_30     300U
#define MOTOR_OPEN_LOOP_DUTY_60     600U
#define MOTOR_OPEN_LOOP_DUTY_100    1000U

/* 正常固件使用手动/自动模式；启用旧按键档位还必须开启开环验证开关。 */
#ifndef APP_OPEN_LOOP_BUTTON_CONTROL
#define APP_OPEN_LOOP_BUTTON_CONTROL 0
#endif
#if APP_OPEN_LOOP_BUTTON_CONTROL && !APP_ENABLE_OPEN_LOOP_TESTS
#error "Open-loop button control requires APP_ENABLE_OPEN_LOOP_TESTS"
#endif


/* 事件标志位 */
/* 与 state_machine.h 的事件枚举分开命名，避免宏替换枚举值。 */
#define SYSTEM_FLAG_SENSOR_ERROR    (1U << 0)
#define SYSTEM_FLAG_MOTOR_STALL     (1U << 1)
#define SYSTEM_FLAG_OVER_TEMP       (1U << 2)
#define SYSTEM_FLAG_USER_INPUT      (1U << 3)
#define SYSTEM_FLAG_STOP_REQUEST    (1U << 4)
#define SYSTEM_FLAG_STORAGE_REQUEST (1U << 5)
#define SYSTEM_FLAG_MOTOR_STOPPED   (1U << 6)
#define SYSTEM_FLAG_STORAGE_GRANT   (1U << 7)
#define SYSTEM_FLAG_BOOT_REQUEST    (1U << 8)

typedef enum {
    MOTOR_COMMAND_SET_DUTY = 0,
    MOTOR_COMMAND_START_SWEEP,
    MOTOR_COMMAND_SET_RPM,
    MOTOR_COMMAND_STOP
} Motor_CommandType_t;

typedef struct {
    Motor_CommandType_t type;
    uint16_t duty;             /* 开环命令，0~1000；0 表示关闭使能。 */
    uint16_t target_rpm;
    uint32_t dwell_ms;
    uint32_t timestamp_ms;
    bool automatic;
    bool reset_fault;
    SystemConfig_t config; /* 系统管理任务发布不可变副本，电机任务不共享配置内存。 */
} Motor_Command_t;

typedef struct {
    Motor_Info_t motor;
    Bsp_SpeedInfo_t speed;
    bool sweep_active;
    uint16_t target_rpm;
    uint32_t faults;
    uint32_t timestamp_ms;
} Motor_Telemetry_t;

typedef struct {
    Runtime_Task_t tasks[HEARTBEAT_COUNT];
    uint32_t stack_free_bytes[HEARTBEAT_COUNT];
    uint64_t cpu_total_us;
    uint32_t heap_free_bytes, heap_min_bytes;
} Tasks_MonitorSnapshot_t;
void Tasks_GetMonitorSnapshot(Tasks_MonitorSnapshot_t *snapshot);

/* 全局句柄 */
extern QueueHandle_t g_sensor_queue;
extern QueueHandle_t g_motor_command_queue;
extern QueueHandle_t g_ui_event_queue;
extern SemaphoreHandle_t g_i2c_mutex;
extern SemaphoreHandle_t g_config_mutex;
extern EventGroupHandle_t g_system_events;
void Tasks_ReportIdleHeartbeat(void);
void Task_Storage(void *argument);

/* 函数声明 */
bool Tasks_Init(void);
/* 正常固件只接受 duty=0 的停机请求。非零 duty 仅在专用开环测试固件启用。 */
bool Tasks_SetMotorDuty(uint16_t duty);
bool Tasks_SetMotorDutyPercent(uint8_t percent);
/* 专用测试固件的 0/30/60/100% 扫测；正常固件返回 false。 */
bool Tasks_StartOpenLoopSweep(uint32_t dwell_ms);
bool Tasks_StopOpenLoopSweep(void);
void Task_MotorControl(void *argument);
void Task_Sensor(void *argument);
void Task_UI(void *argument);
void Task_SystemManager(void *argument);
void Task_Watchdog(void *argument);
void Task_Logger(void *argument);

#endif /* TASKS_H */
