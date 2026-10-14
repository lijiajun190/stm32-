/**
 * @file    tasks.c
 * @brief   FreeRTOS 任务实现
 */

#include "tasks.h"
#include "button.h"
#include "encoder.h"
#include "sensor_filter.h"
#include "motor_control.h"
#include "mq135.h"
#include "anemometer.h"
#include "sht30.h"
#include "bmp280.h"
#include "bsp_adc.h"
#include "state_machine.h"
#include "oled.h"
#include "bsp_watchdog.h"
#include "bsp_storage.h"
#include "system_persist.h"
#include "system_fault.h"
#include "fault_record.h"
#include "ui_model.h"
#include "buzzer.h"
#include "led.h"
#include "bsp_runtime.h"
#include "app_serial.h"
#include <stdio.h>
#include <string.h>

/* 全局句柄 */
QueueHandle_t g_sensor_queue = NULL;
QueueHandle_t g_motor_command_queue = NULL;
QueueHandle_t g_ui_event_queue = NULL;
SemaphoreHandle_t g_i2c_mutex = NULL;
SemaphoreHandle_t g_config_mutex = NULL;
EventGroupHandle_t g_system_events = NULL;
static QueueHandle_t motor_telemetry_queue = NULL;
static QueueHandle_t system_status_queue = NULL;
static SemaphoreHandle_t display_mutex = NULL;
static Heartbeat_t heartbeat;
static SystemPersist_t persistence;
static SystemConfig_t runtime_config;
static bool storage_save_pending;
static bool storage_fault_pending;
static QueueHandle_t config_update_queue, fault_queue;
static uint32_t configuration_changed_ms;
static TaskHandle_t application_handles[HEARTBEAT_COUNT];
static UI_Model_t ui_model;
static RuntimeMonitor_t runtime_monitor;
static bool monitor_ready;
static const uint32_t monitor_periods[HEARTBEAT_COUNT] = {50, 100, 10, 100, 1000, 100, 0, 100};

void Tasks_MonitorSwitch(void *handle)
{
    if (monitor_ready) RuntimeMonitor_Switch(&runtime_monitor, handle, Bsp_Runtime_Micros());
}

static void monitor_end(Heartbeat_ID_t id)
{
    taskENTER_CRITICAL();
    RuntimeMonitor_End(&runtime_monitor, id, Bsp_Runtime_Micros(), monitor_periods[id] * 1000U);
    taskEXIT_CRITICAL();
}

void Tasks_GetMonitorSnapshot(Tasks_MonitorSnapshot_t *snapshot)
{
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    taskENTER_CRITICAL();
    RuntimeMonitor_Account(&runtime_monitor, Bsp_Runtime_Micros());
    memcpy(snapshot->tasks, runtime_monitor.tasks, sizeof(snapshot->tasks));
    taskEXIT_CRITICAL();
    for (unsigned id = 0; id < HEARTBEAT_COUNT; ++id) {
        snapshot->cpu_total_us += snapshot->tasks[id].cpu_us;
        TaskHandle_t handle = id == HEARTBEAT_IDLE ? xTaskGetIdleTaskHandle() : application_handles[id];
        if (handle) snapshot->stack_free_bytes[id] = uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
    }
    snapshot->heap_free_bytes = xPortGetFreeHeapSize();
    snapshot->heap_min_bytes = xPortGetMinimumEverFreeHeapSize();
}

static void report_heartbeat(Heartbeat_ID_t id)
{
    taskENTER_CRITICAL();
    Heartbeat_Report(&heartbeat, id, HAL_GetTick());
    if (id != HEARTBEAT_IDLE)
        RuntimeMonitor_Begin(&runtime_monitor, id, Bsp_Runtime_Micros(), monitor_periods[id] * 1000U);
    taskEXIT_CRITICAL();
}
void Tasks_ReportIdleHeartbeat(void)
{
    if (!runtime_monitor.handles[HEARTBEAT_IDLE]) {
        taskENTER_CRITICAL();
        RuntimeMonitor_Register(&runtime_monitor, HEARTBEAT_IDLE, xTaskGetIdleTaskHandle());
        taskEXIT_CRITICAL();
    }
    report_heartbeat(HEARTBEAT_IDLE);
}

static void fill_fault(FaultRecord_t *record, FaultReason_t reason, uint32_t detail,
                       const Motor_Telemetry_t *motor, const SensorData_t *sensor)
{
    memset(record, 0, sizeof(*record));
    record->reason = reason; record->detail = detail; record->uptime_ms = HAL_GetTick();
    if (motor) { record->rpm = motor->speed.rpm; record->target_rpm = motor->target_rpm; record->duty = motor->motor.duty; record->mode = StateMachine_GetState(); }
    if (sensor) { record->sensor_valid = sensor->valid_flags; record->sensor_errors = sensor->error_flags; record->temperature_mC = (int32_t)(sensor->temperature_raw * 1000.0f); }
}

#if !APP_OPEN_LOOP_BUTTON_CONTROL
static void UI_Dispatch(UI_Action_t action, int32_t delta)
{
    System_Status_t status = {0};
    (void)xQueuePeek(system_status_queue, &status, 0);
    SystemConfig_t config = ui_model.draft;
    if (action == UI_ACTION_PAGE && !Tasks_GetConfig(&config)) {
        Buzzer_Request(BUZZER_SIGNAL_WARNING, HAL_GetTick());
        return;
    }
    UI_Result_t result = UI_Model_Action(&ui_model, action, delta, &config, &status);
    if (result.save_config) {
        if (!Tasks_SetConfig(&ui_model.draft)) {
            ui_model.page = UI_PAGE_SETTINGS;
            result.feedback = UI_FEEDBACK_WARNING;
        } else ui_model.dirty = false;
    }
    Buzzer_Signal_t signal = result.feedback == UI_FEEDBACK_WARNING ? BUZZER_SIGNAL_WARNING :
                            result.feedback == UI_FEEDBACK_MODE ? BUZZER_SIGNAL_MODE : BUZZER_SIGNAL_CLICK;
    Buzzer_Request(signal, HAL_GetTick());
    if (result.input.type == UI_INPUT_STOP || result.input.type == UI_INPUT_RESET)
        xEventGroupSetBits(g_system_events, SYSTEM_FLAG_STOP_REQUEST);
    /* 活动事件也入队，睡眠唤醒和待机计时由系统管理任务统一处理。 */
    if (xQueueSend(g_ui_event_queue, &result.input, 0) != pdPASS)
        xEventGroupSetBits(g_system_events, SYSTEM_FLAG_STOP_REQUEST);
}
#endif

/* 回调来自 UI 任务的消抖扫描，不在 EXTI 中操作队列或电机。 */
static void UI_ButtonCallback(Button_ID_t id, Button_Event_t event)
{
#if APP_OPEN_LOOP_BUTTON_CONTROL
    if (id == BUTTON_ENCODER && event == BUTTON_EVENT_PRESS) (void)Tasks_SetMotorDuty(0);
    if (event == BUTTON_EVENT_CLICK && !Button_IsPressed(BUTTON_ENCODER)) {
        if (id == BUTTON_MODE) (void)Tasks_SetMotorDuty(MOTOR_OPEN_LOOP_DUTY_30);
        if (id == BUTTON_UP) (void)Tasks_SetMotorDuty(MOTOR_OPEN_LOOP_DUTY_60);
        if (id == BUTTON_DOWN) (void)Tasks_SetMotorDuty(MOTOR_OPEN_LOOP_DUTY_100);
    }
#else
    if (event == BUTTON_EVENT_CLICK) {
        if (id == BUTTON_ENCODER) UI_Dispatch(UI_ACTION_POWER, 0);
        if (id == BUTTON_MODE) UI_Dispatch(UI_ACTION_MODE, 0);
        if (id == BUTTON_UP) UI_Dispatch(UI_ACTION_UP, 0);
        if (id == BUTTON_DOWN) UI_Dispatch(UI_ACTION_DOWN, 0);
    } else if (event == BUTTON_EVENT_LONG_PRESS) {
        if (id == BUTTON_ENCODER) UI_Dispatch(UI_ACTION_RESET, 0);
        else if (id == BUTTON_UP) UI_Dispatch(UI_ACTION_PAGE, 0);
        else if (id == BUTTON_MODE || id == BUTTON_DOWN) UI_Dispatch(UI_ACTION_STOP, 0);
    }
#endif
}

#if !APP_OPEN_LOOP_BUTTON_CONTROL
static void UI_EncoderCallback(int16_t delta)
{
    static int32_t remainder;
    remainder += delta;
    int32_t steps = remainder / CONTROL_ENCODER_COUNTS;
    remainder %= CONTROL_ENCODER_COUNTS;
    if (steps != 0) UI_Dispatch(UI_ACTION_ROTATE, steps);
}
#endif

/**
 * @brief  创建任务和通信对象
 */
bool Tasks_Init(void)
{
    StoreBackend_t storage_backend = Bsp_Storage_Backend();
    if (!Bsp_Storage_Unlock() || !SystemPersist_Init(&persistence, &storage_backend, &runtime_config)) {
        Bsp_Storage_Lock();
        printf("[PERSIST] init failed\r\n");
        return false;
    } else {
        Bsp_Storage_Lock();
        printf("[PERSIST] config=%s fault_valid=%u\r\n", persistence.config_saved ? "loaded" : "defaults",
               (unsigned)persistence.fault_valid);
    }
    StateMachine_ApplyConfig(&runtime_config);
    UI_Model_Init(&ui_model, &runtime_config);
    storage_save_pending = !persistence.config_saved;
    configuration_changed_ms = HAL_GetTick();
    FaultRecord_t retained;
    if (SystemFault_GetPending(&retained)) {
        bool persisted = Bsp_Storage_Unlock() && SystemPersist_SaveFault(&persistence, &retained);
        Bsp_Storage_Lock();
        if (persisted) SystemFault_ClearPending();
        else return false; /* 未保存邮箱不得被下一次普通故障覆盖。 */
    }
    if (persistence.fault_valid) {
        printf("[LAST_FAULT] seq=%lu reason=%lu detail=0x%lX uptime=%lu reset=0x%lX pc=0x%lX lr=0x%lX cfsr=0x%lX\r\n",
            (unsigned long)persistence.fault_sequence, (unsigned long)persistence.latest_fault.reason,
            (unsigned long)persistence.latest_fault.detail, (unsigned long)persistence.latest_fault.uptime_ms,
            (unsigned long)persistence.latest_fault.reset_flags,
            (unsigned long)persistence.latest_fault.registers[6], (unsigned long)persistence.latest_fault.registers[5],
            (unsigned long)persistence.latest_fault.cfsr);
    }
    // 创建队列
    g_sensor_queue = xQueueCreate(1, sizeof(SensorData_t));
    g_motor_command_queue = xQueueCreate(1, sizeof(Motor_Command_t));
    motor_telemetry_queue = xQueueCreate(1, sizeof(Motor_Telemetry_t));
    system_status_queue = xQueueCreate(1, sizeof(System_Status_t));
    g_ui_event_queue = xQueueCreate(10, sizeof(UI_Event_t));
    config_update_queue = xQueueCreate(1, sizeof(SystemConfig_t));
    fault_queue = xQueueCreate(8, sizeof(FaultRecord_t));

    // 创建互斥量
    g_i2c_mutex = xSemaphoreCreateMutex();
    display_mutex = xSemaphoreCreateMutex();
    g_config_mutex = xSemaphoreCreateMutex();

    // 创建事件组
    g_system_events = xEventGroupCreate();

    if (g_sensor_queue == NULL || g_motor_command_queue == NULL ||
        g_ui_event_queue == NULL || motor_telemetry_queue == NULL || system_status_queue == NULL ||
        g_i2c_mutex == NULL || display_mutex == NULL || g_config_mutex == NULL || g_system_events == NULL ||
        !config_update_queue || !fault_queue) {
        printf("[Tasks] Communication object allocation failed\r\n");
        return false;
    }

    /* 输入事件先入队，由系统管理任务统一决定模式与动作。 */
    Button_RegisterCallback(UI_ButtonCallback);
#if !APP_OPEN_LOOP_BUTTON_CONTROL
    Encoder_RegisterCallback(UI_EncoderCallback);
#endif
    printf("[CONTROL] manual/auto/boost/cleaning/sleep; target=%u..%u rpm; startup stopped\r\n",
           (unsigned)CONTROL_TARGET_RPM_MIN, (unsigned)CONTROL_TARGET_RPM_MAX);

    /* Flash 扫描和启动故障落盘完成后才开始心跳宽限，避免慢启动误报。 */
    Heartbeat_Init(&heartbeat, HAL_GetTick());
    RuntimeMonitor_Init(&runtime_monitor);
    // 创建任务
    /* 失败时不启动调度器，由 main.c 关闭电机并报告启动失败。 */
    if (xTaskCreate(Task_MotorControl, "MotorCtrl", 512, NULL, TASK_PRIORITY_MOTOR_CTRL, &application_handles[HEARTBEAT_MOTOR]) != pdPASS ||
        xTaskCreate(Task_Sensor, "Sensor", 512, NULL, TASK_PRIORITY_SENSOR, &application_handles[HEARTBEAT_SENSOR]) != pdPASS ||
        xTaskCreate(Task_UI, "UI", 512, NULL, TASK_PRIORITY_UI, &application_handles[HEARTBEAT_UI]) != pdPASS ||
        xTaskCreate(Task_SystemManager, "SysMgr", 512, NULL, TASK_PRIORITY_SYSTEM_MGR, &application_handles[HEARTBEAT_SYSTEM]) != pdPASS ||
        xTaskCreate(Task_Storage, "Storage", 1024, NULL, TASK_PRIORITY_LOGGER, &application_handles[HEARTBEAT_STORAGE]) != pdPASS ||
        xTaskCreate(Task_Watchdog, "Watchdog", 512, NULL, TASK_PRIORITY_WATCHDOG, &application_handles[HEARTBEAT_WATCHDOG]) != pdPASS ||
        xTaskCreate(Task_Logger, "Logger", 768, NULL, TASK_PRIORITY_LOGGER, &application_handles[HEARTBEAT_LOGGER]) != pdPASS) {
        printf("[Tasks] Task allocation failed\r\n");
        return false;
    }

    printf("[Tasks] All tasks created\r\n");
    for (unsigned id = 0; id < HEARTBEAT_COUNT; ++id)
        RuntimeMonitor_Register(&runtime_monitor, id, application_handles[id]);
    monitor_ready = true;
    return true;
}

bool Tasks_SetMotorDuty(uint16_t duty)
{
#if !APP_ENABLE_OPEN_LOOP_TESTS
    if (duty == 0U && g_system_events) {
        xEventGroupSetBits(g_system_events, SYSTEM_FLAG_STOP_REQUEST);
        return true;
    }
    return false;
#else
    if (g_motor_command_queue == NULL || duty > 1000U) {
        return false;
    }
    /* 固定命令和扫测命令共用队列，由电机任务按收到的最新命令执行。 */
    Motor_Command_t command = { .type = MOTOR_COMMAND_SET_DUTY, .duty = duty };
    return xQueueOverwrite(g_motor_command_queue, &command) == pdPASS;
#endif
}

bool Tasks_SetMotorDutyPercent(uint8_t percent)
{
    if (percent > 100U) {
        return false;
    }
    return Tasks_SetMotorDuty((uint16_t)percent * 10U);
}

bool Tasks_StartOpenLoopSweep(uint32_t dwell_ms)
{
#if !APP_ENABLE_OPEN_LOOP_TESTS
    (void)dwell_ms;
    return false;
#else
    if (g_motor_command_queue == NULL || dwell_ms < MOTOR_OPEN_LOOP_MIN_DWELL_MS ||
        dwell_ms > MOTOR_OPEN_LOOP_MAX_DWELL_MS) {
        return false;
    }
    Motor_Command_t command = { .type = MOTOR_COMMAND_START_SWEEP, .dwell_ms = dwell_ms };
    return xQueueOverwrite(g_motor_command_queue, &command) == pdPASS;
#endif
}

bool Tasks_StopOpenLoopSweep(void)
{
    return Tasks_SetMotorDuty(0U);
}

bool Tasks_GetConfig(SystemConfig_t *config)
{
    if (!config || !g_config_mutex) return false;
    if (xSemaphoreTake(g_config_mutex, pdMS_TO_TICKS(20)) != pdPASS) return false;
    *config = runtime_config; xSemaphoreGive(g_config_mutex); return true;
}
bool Tasks_SetConfig(const SystemConfig_t *config)
{
    if (!config || !SystemConfig_Validate(config) || !config_update_queue) return false;
    /* 应用请求先由 SysMgr 停机，运行任务不会看到半更新的参数。 */
    return xQueueOverwrite(config_update_queue, config) == pdPASS;
}
bool Tasks_IsConfigSaved(void)
{
    if (!g_config_mutex || xSemaphoreTake(g_config_mutex, pdMS_TO_TICKS(20)) != pdPASS) return false;
    bool saved = !storage_save_pending && persistence.config_saved && uxQueueMessagesWaiting(config_update_queue) == 0;
    xSemaphoreGive(g_config_mutex); return saved;
}
bool Tasks_GetLastFault(FaultRecord_t *fault)
{
    if (!fault) return false;
    if (g_config_mutex && xSemaphoreTake(g_config_mutex, pdMS_TO_TICKS(20)) != pdPASS) return false;
    bool valid = persistence.fault_valid;
    if (valid) *fault = persistence.latest_fault;
    if (g_config_mutex) xSemaphoreGive(g_config_mutex);
    return valid;
}

/**
 * @brief  电机控制任务
 */
void Task_MotorControl(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    Motor_OpenLoop_t sweep = {0};
    MotorControl_t control;
    MotorControl_Init(&control);
    Motor_Command_t request = {0};
    SensorData_t sensor = {0};
    uint32_t reported_faults = 0;

    for (;;) {
        report_heartbeat(HEARTBEAT_MOTOR);
        Motor_Command_t command;
        bool has_command = xQueueReceive(g_motor_command_queue, &command, 0) == pdPASS;
        uint32_t now_ms = HAL_GetTick();
        if (has_command) {
            request = command;
            if (SystemConfig_Validate(&request.config) && !control.running) {
                control.pid.params.kp = request.config.kp;
                control.pid.params.ki = request.config.ki;
                control.pid.params.kd = request.config.kd;
            }
#if APP_ENABLE_OPEN_LOOP_TESTS
            Motor_OpenLoop_Stop(&sweep);
            if (request.type == MOTOR_COMMAND_START_SWEEP) {
                (void)Motor_OpenLoop_Start(&sweep, now_ms, command.dwell_ms);
                request.duty = 0U;
            }
#endif
        }
        Motor_Telemetry_t telemetry = {0};
        Bsp_Timer_SpeedCapture_GetInfo(&telemetry.speed);
        (void)xQueuePeek(g_sensor_queue, &sensor, 0);
        EventBits_t flags = xEventGroupGetBits(g_system_events);
        bool storage_requested = (flags & SYSTEM_FLAG_STORAGE_REQUEST) != 0;
        bool stop_requested = (flags & (SYSTEM_FLAG_STOP_REQUEST | SYSTEM_FLAG_STORAGE_REQUEST | SYSTEM_FLAG_BOOT_REQUEST)) != 0;
        MotorControl_Input_t input = {0};
        input.now_ms = now_ms;
        input.rpm = telemetry.speed.rpm;
        /* 启动前/停机后的旧周期不能算作新反馈；新转速至少包含启动后的脉冲。 */
        input.speed_valid = telemetry.speed.valid;
        input.sensor = &sensor;
        input.automatic = request.automatic;
        input.target_rpm = (!stop_requested && request.type == MOTOR_COMMAND_SET_RPM) ? request.target_rpm : 0;
        input.command_timestamp_ms = request.timestamp_ms;
        input.reset_fault = has_command && request.reset_fault;
        if (input.target_rpm && !control.faults) {
            uint32_t since = control.running ? control.start_ms : now_ms;
            input.speed_valid = telemetry.speed.valid &&
                (uint32_t)(telemetry.speed.last_capture_ms - since) < 0x80000000UL;
        }
        /* 超速检查不受启动反馈的新旧限制影响。 */
        if (telemetry.speed.valid && telemetry.speed.rpm > CONTROL_OVERSPEED_RPM)
            MotorControl_LatchFault(&control, CONTROL_FAULT_OVERSPEED);
        uint16_t duty = MotorControl_Step(&control, &input);
#if APP_ENABLE_OPEN_LOOP_TESTS
        /* 开环测试仍保护超速和停机；它的启动动作来自显式测试 API。 */
        if (!control.faults && !stop_requested) {
            if (request.type == MOTOR_COMMAND_SET_DUTY) duty = request.duty;
            else if (request.type == MOTOR_COMMAND_START_SWEEP) {
                (void)Motor_OpenLoop_Poll(&sweep, now_ms, &request.duty);
                duty = request.duty;
            }
        }
#endif
        if (control.faults || stop_requested) { duty = 0; Motor_OpenLoop_Stop(&sweep); }
        Motor_Enable(duty != 0);
        Motor_SetDuty(duty);
        if (Motor_GetInfo().state == MOTOR_STATE_ERROR) {
            MotorControl_LatchFault(&control, CONTROL_FAULT_OUTPUT);
            Motor_OpenLoop_Stop(&sweep);
        }
        Motor_UpdateSpeed(telemetry.speed.rpm, telemetry.speed.valid != 0U);
        telemetry.motor = Motor_GetInfo();
        telemetry.sweep_active = sweep.active;
        telemetry.target_rpm = input.target_rpm;
        telemetry.faults = control.faults;
        telemetry.timestamp_ms = now_ms;
        if (storage_requested && !telemetry.motor.enabled)
            xEventGroupSetBits(g_system_events, SYSTEM_FLAG_MOTOR_STOPPED);
        else xEventGroupClearBits(g_system_events, SYSTEM_FLAG_MOTOR_STOPPED);
        FaultRecord_t context;
        fill_fault(&context, FAULT_REASON_MOTOR, telemetry.faults, &telemetry, &sensor);
        SystemFault_UpdateSnapshot(&context);
        if (control.faults != reported_faults) {
            if (control.faults) {
                if (xQueueSend(fault_queue, &context, 0) != pdPASS)
                    SystemFault_Fatal(FAULT_REASON_MOTOR, control.faults);
            }
            reported_faults = control.faults;
        }
        /* 长度为 1 的快照队列不会积压，串口输出不占用电机控制任务。 */
        xQueueOverwrite(motor_telemetry_queue, &telemetry);

        monitor_end(HEARTBEAT_MOTOR);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CTRL_MS));
    }
}

/**
 * @brief  传感器任务
 */
void Task_Sensor(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    SensorFilter_t sensor_filter;
    SensorFilter_Init(&sensor_filter);
    uint32_t last_bmp_retry_ms = HAL_GetTick() - 5000U;

    for (;;) {
        report_heartbeat(HEARTBEAT_SENSOR);
        SensorData_t data; memset(&data, 0, sizeof(data));
        if (Bsp_ADC_GetSnapshot(data.adc_raw)) {
            data.valid_flags |= SENSOR_VALID_ADC;
            for (uint8_t i = 0; i < BSP_ADC_CH_MAX; ++i)
                data.adc_voltage[i] = (float)data.adc_raw[i] * 3.3f / 4095.0f;
        } else data.error_flags |= SENSOR_ERROR_ADC;
        MQ135_UpdateVoltage(data.adc_voltage[BSP_ADC_CH_MQ135]);
        Anemometer_UpdateVoltage(data.adc_voltage[BSP_ADC_CH_WIND_SPEED]);
        MQ135_Data_t gas = MQ135_GetData(); Anemometer_Data_t wind = Anemometer_GetData();
        data.gas_voltage = gas.voltage; data.gas_resistance = gas.resistance; data.gas_ratio = gas.ratio;
        data.wind_speed = wind.wind_speed; data.mq_preheating = gas.status == MQ135_STATUS_PREHEATING;
        data.mq_uncalibrated = !gas.calibrated;
        if (gas.valid) data.valid_flags |= SENSOR_VALID_GAS;
        else if (!data.mq_preheating) data.error_flags |= SENSOR_ERROR_RANGE;
        if (wind.valid) data.valid_flags |= SENSOR_VALID_WIND; else data.error_flags |= SENSOR_ERROR_RANGE;
        SHT30_Data_t sht = {0}; BMP280_Data_t bmp = {0};
        bool sht_ok = false, bmp_ok = false;
        bool started = false;
        if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(5)) == pdPASS) {
            started = SHT30_BeginRead();
            xSemaphoreGive(g_i2c_mutex);
        }
        if (started) vTaskDelay(pdMS_TO_TICKS(20));
        if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(5)) == pdPASS) {
            if (started) sht_ok = SHT30_FinishRead(&sht);
            bmp_ok = BMP280_Read(&bmp);
            xSemaphoreGive(g_i2c_mutex);
        }
        if (sht_ok) { data.temperature = sht.temperature; data.temperature_raw = sht.temperature; data.humidity = sht.humidity; data.valid_flags |= SENSOR_VALID_SHT30; }
        else data.error_flags |= SENSOR_ERROR_SHT30;
        if (bmp_ok) { data.pressure = bmp.pressure; data.valid_flags |= SENSOR_VALID_BMP280; }
        else data.error_flags |= SENSOR_ERROR_BMP280;
        data.timestamp_ms = HAL_GetTick();
        SensorFilter_Apply(&sensor_filter, &data);
        (void)xQueueOverwrite(g_sensor_queue, &data);

        /* 断线后先发布本周期的无效数据，再限频重试初始化，不保留旧有效值。 */
        if (!BMP280_IsInitialized() && (uint32_t)(HAL_GetTick() - last_bmp_retry_ms) >= 5000U &&
            xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(5)) == pdPASS) {
            last_bmp_retry_ms = HAL_GetTick();
            (void)BMP280_Init();
            xSemaphoreGive(g_i2c_mutex);
        }

        monitor_end(HEARTBEAT_SENSOR);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_PERIOD_SENSOR_MS));
    }
}

/**
 * @brief  UI 任务
 */
void Task_UI(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t last_encoder_time = last_wake_time;
    TickType_t last_display_time = last_wake_time;
    uint8_t display_page = 0;
    bool display_enabled = OLED_IsInitialized();
    bool display_sleep = false;
    uint32_t previous_faults = 0;
    System_State_t previous_state = SYSTEM_STATE_INIT;
    char display_lines[UI_LINE_COUNT][UI_LINE_SIZE] = {0};
    UI_Page_t rendered_page = UI_PAGE_COUNT;
    System_State_t rendered_state = SYSTEM_STATE_INIT;
    uint32_t rendered_faults = 0;

    for (;;) {
        report_heartbeat(HEARTBEAT_UI);
        Buzzer_Process(HAL_GetTick());
        System_Status_t status = {0};
        Motor_Telemetry_t motor = {0};
        (void)xQueuePeek(system_status_queue, &status, 0);
        (void)xQueuePeek(motor_telemetry_queue, &motor, 0);
        uint32_t now_ms = HAL_GetTick();
        if (motor.faults & ~previous_faults) Buzzer_Request(BUZZER_SIGNAL_ERROR, now_ms);
        else if (status.state != previous_state && previous_state != SYSTEM_STATE_INIT)
            Buzzer_Request(BUZZER_SIGNAL_MODE, now_ms);
        previous_faults = motor.faults;
        previous_state = status.state;
        /* 红灯故障闪烁、绿灯实际运行、蓝灯任务心跳；睡眠仍保留慢心跳。 */
        LED_Set(LED_RED, motor.faults && (now_ms % 500U < 250U));
        LED_Set(LED_GREEN, motor.motor.enabled && !motor.faults);
        LED_Set(LED_BLUE, now_ms % (status.state == SYSTEM_STATE_SLEEP ? 2000U : 1000U) < 80U);
        Button_Process();
        /* 按键按 10 ms 消抖，编码器和后续显示保持原定 100 ms 更新周期。 */
        TickType_t now = xTaskGetTickCount();
        if (now - last_encoder_time >= pdMS_TO_TICKS(TASK_PERIOD_UI_MS)) {
            Encoder_Process();
            last_encoder_time = now;
        }
        /* OLED 分页刷新：一次任务只发送一页，避免整屏 I2C 传输占住输入扫描。 */
        if (display_enabled && now - last_display_time >= pdMS_TO_TICKS(TASK_PERIOD_UI_MS) &&
            xSemaphoreTake(display_mutex, 0) == pdPASS) {
            bool sleep = status.state == SYSTEM_STATE_SLEEP && !motor.faults;
            if (sleep != display_sleep) {
                if (OLED_DisplayOn(!sleep) != OLED_OK) display_enabled = false;
                display_sleep = sleep;
                display_page = 0;
            }
            if (!sleep && display_enabled) {
                if (rendered_page != ui_model.page || rendered_state != status.state ||
                    rendered_faults != motor.faults) display_page = 0;
                /* 八个硬件页共享同一快照，避免半帧显示混合状态。 */
                if (display_page == 0U) {
                    UI_View_t view = {0};
                    view.status = status; view.now_ms = now_ms;
                    view.rpm = motor.speed.rpm; view.speed_valid = motor.speed.valid;
                    view.duty = motor.motor.duty; view.motor_enabled = motor.motor.enabled;
                    view.motor_faults = motor.faults;
                    (void)xQueuePeek(g_sensor_queue, &view.sensor, 0);
                    view.config_saved = Tasks_IsConfigSaved();
                    view.last_fault_valid = Tasks_GetLastFault(&view.last_fault);
                    UI_Model_Render(&ui_model, &view, display_lines);
                    rendered_page = ui_model.page; rendered_state = status.state;
                    rendered_faults = motor.faults;
                }
                (void)OLED_ShowString(0, display_page, "                     ", OLED_FONT_6X8);
                if (display_lines[display_page][0])
                    (void)OLED_ShowString(0, display_page, display_lines[display_page], OLED_FONT_6X8);
                if (OLED_RefreshPage(display_page) != OLED_OK) display_enabled = false;
                display_page = (uint8_t)((display_page + 1U) % 8U);
            }
            xSemaphoreGive(display_mutex);
            last_display_time = now;
        }

        monitor_end(HEARTBEAT_UI);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_PERIOD_INPUT_SCAN_MS));
    }
}

/**
 * @brief  系统管理任务
 */
void Task_SystemManager(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t previous_sensor_errors = 0;

    for (;;) {
        report_heartbeat(HEARTBEAT_SYSTEM);
        SensorData_t sensor = {0};
        Motor_Telemetry_t motor = {0};
        (void)xQueuePeek(g_sensor_queue, &sensor, 0);
        (void)xQueuePeek(motor_telemetry_queue, &motor, 0);
        uint32_t now_ms = HAL_GetTick();
        if (motor.faults || StateMachine_GetState() == SYSTEM_STATE_ERROR)
            StateMachine_Update(&sensor, now_ms, motor.faults);
        bool stop = (xEventGroupGetBits(g_system_events) & (SYSTEM_FLAG_STOP_REQUEST | SYSTEM_FLAG_STORAGE_REQUEST | SYSTEM_FLAG_BOOT_REQUEST)) != 0;
        if (stop) StateMachine_ProcessEvent(EVENT_STOP);
        /* 先取得配置锁再出队；锁忙时请求仍留在队列，下周期重新处理。 */
        SystemConfig_t updated;
        SystemConfig_Defaults(&updated);
        if (xSemaphoreTake(g_config_mutex, pdMS_TO_TICKS(20)) == pdPASS) {
            if (xQueueReceive(config_update_queue, &updated, 0) == pdPASS) {
                StateMachine_ApplyConfig(&updated);
                runtime_config = updated; storage_save_pending = true; configuration_changed_ms = now_ms;
                stop = true;
            }
            updated = runtime_config;
            xSemaphoreGive(g_config_mutex);
        }
        UI_Event_t input;
        while (xQueueReceive(g_ui_event_queue, &input, 0) == pdPASS) {
            /* 停机与同一批输入相遇时停机优先；故障复位只能发送零目标。 */
            if (!stop || input.type == UI_INPUT_RESET || input.type == UI_INPUT_STOP ||
                input.type == UI_INPUT_ACTIVITY) StateMachine_HandleInputAt(&input, now_ms);
        }
        StateMachine_Update(&sensor, now_ms, motor.faults);
        if (sensor.error_flags != previous_sensor_errors) {
            uint32_t new_errors = sensor.error_flags & ~previous_sensor_errors;
            if (new_errors) {
                FaultRecord_t record; fill_fault(&record, FAULT_REASON_SENSOR, new_errors, &motor, &sensor);
                (void)xQueueSend(fault_queue, &record, 0); /* 队列满仅保留电机关键故障。 */
            }
            previous_sensor_errors = sensor.error_flags;
        }
        System_Status_t status = StateMachine_GetStatus();
        if (xSemaphoreTake(g_config_mutex, pdMS_TO_TICKS(20)) == pdPASS) {
            if (runtime_config.manual_rpm != status.manual_rpm) {
                runtime_config.manual_rpm = status.manual_rpm;
                configuration_changed_ms = now_ms; storage_save_pending = true;
            }
            updated = runtime_config;
            xSemaphoreGive(g_config_mutex);
        } else {
            /* 无法取得一致快照时仅发送停机，不让默认参数用于运行控制。 */
            StateMachine_ProcessEvent(EVENT_STOP);
            status = StateMachine_GetStatus();
            stop = true;
        }
        Motor_Command_t command = {0};
        command.type = MOTOR_COMMAND_SET_RPM;
        command.target_rpm = status.target_rpm;
        command.automatic = status.automatic_required;
        command.reset_fault = status.reset_pending;
        command.timestamp_ms = now_ms;
        command.config = updated;
#if !APP_ENABLE_OPEN_LOOP_TESTS
        (void)xQueueOverwrite(g_motor_command_queue, &command);
#endif
        (void)xQueueOverwrite(system_status_queue, &status);
        /* 电机任务在零目标接收前仍检查 STOP 位；管理任务确认后才解除请求。 */
        if (stop) xEventGroupClearBits(g_system_events, SYSTEM_FLAG_STOP_REQUEST);

        monitor_end(HEARTBEAT_SYSTEM);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_PERIOD_SYSTEM_MGR_MS));
    }
}

/**
 * @brief  看门狗任务
 */
void Task_Watchdog(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    if (!Bsp_Watchdog_Start()) SystemFault_Fatal(FAULT_REASON_WATCHDOG_RESET, 1U);

    for (;;) {
        report_heartbeat(HEARTBEAT_WATCHDOG);
        Heartbeat_t health;
        taskENTER_CRITICAL(); health = heartbeat; taskEXIT_CRITICAL();
        uint32_t now = HAL_GetTick();
        uint32_t missing = Heartbeat_Missing(&health, now);
        if (missing == 0U && Heartbeat_Ready(&health, now)) {
            if (!Bsp_Watchdog_Refresh()) SystemFault_Fatal(FAULT_REASON_WATCHDOG_RESET, 2);
            EventBits_t flags = xEventGroupGetBits(g_system_events);
            if ((flags & (SYSTEM_FLAG_STORAGE_REQUEST | SYSTEM_FLAG_MOTOR_STOPPED)) ==
                (SYSTEM_FLAG_STORAGE_REQUEST | SYSTEM_FLAG_MOTOR_STOPPED))
                xEventGroupSetBits(g_system_events, SYSTEM_FLAG_STORAGE_GRANT);
        } else if (missing || (uint32_t)(now - health.start_ms) >= HEARTBEAT_START_GRACE_MS) {
            SystemFault_Fatal(FAULT_REASON_HEARTBEAT, missing);
        }

        monitor_end(HEARTBEAT_WATCHDOG);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_PERIOD_WATCHDOG_MS));
    }
}

void Task_Storage(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    FaultRecord_t pending_fault = {0};
    bool have_fault = false;
    uint32_t retry_after_ms = 0;
    for (;;) {
        report_heartbeat(HEARTBEAT_STORAGE);
        if (!have_fault) have_fault = xQueueReceive(fault_queue, &pending_fault, 0) == pdPASS;
        SystemConfig_t config;
        bool save_config = false;
        System_Status_t status = {0}; Motor_Telemetry_t motor = {0};
        (void)xQueuePeek(system_status_queue, &status, 0);
        (void)xQueuePeek(motor_telemetry_queue, &motor, 0);
        uint32_t now = HAL_GetTick();
        if (xSemaphoreTake(g_config_mutex, 0) == pdPASS) {
            config = runtime_config;
            save_config = storage_save_pending && (uint32_t)(now - configuration_changed_ms) >= 2000U;
            storage_fault_pending = have_fault;
            xSemaphoreGive(g_config_mutex);
        }
        /* 保存不打断正常运行；等待自然停机后锁住启动，故障记录保留在 RAM 队列。
           同一停机窗口最多写一个配置和一个故障，失败 5 s 后重试。 */
        if ((save_config || have_fault) && !status.run_requested && !motor.motor.enabled &&
            (uint32_t)(now - retry_after_ms) < 0x80000000UL) {
            xEventGroupSetBits(g_system_events, SYSTEM_FLAG_STORAGE_REQUEST);
            EventBits_t bits = xEventGroupGetBits(g_system_events);
            if ((bits & (SYSTEM_FLAG_STORAGE_GRANT | SYSTEM_FLAG_MOTOR_STOPPED)) ==
                (SYSTEM_FLAG_STORAGE_GRANT | SYSTEM_FLAG_MOTOR_STOPPED)) {
                /* 健康证明由最高优先级 WatchdogTask 给出，维护期间绝不自行喂狗。 */
                bool ok = true;
                if (xSemaphoreTake(g_config_mutex, 0) == pdPASS) {
                    vTaskSuspendAll();
                    if (Bsp_Storage_Unlock()) {
                        if (have_fault) {
                            ok = SystemPersist_SaveFault(&persistence, &pending_fault);
                            if (ok) have_fault = false;
                        }
                        if (save_config) {
                            bool config_ok = SystemPersist_SaveConfig(&persistence, &config);
                            if (config_ok && memcmp(&config, &runtime_config, sizeof(config)) == 0)
                                storage_save_pending = false;
                            ok = ok && config_ok;
                        }
                        Bsp_Storage_Lock();
                    } else ok = false;
                    /* CPU 单 Bank 暂停不会生成任务心跳；仅在已批准、有界的停机维护
                       结束后重置监测时间基线，不改变 seen_mask。 */
                    taskENTER_CRITICAL();
                    uint32_t resumed = HAL_GetTick();
                    for (unsigned i = 0; i < HEARTBEAT_COUNT; ++i) heartbeat.timestamp[i] = resumed;
                    taskEXIT_CRITICAL();
                    xEventGroupClearBits(g_system_events, SYSTEM_FLAG_STORAGE_REQUEST | SYSTEM_FLAG_STORAGE_GRANT);
                    (void)xTaskResumeAll();
                    xSemaphoreGive(g_config_mutex);
                    retry_after_ms = HAL_GetTick() + (ok ? 0U : 5000U);
                }
            }
        }
        monitor_end(HEARTBEAT_STORAGE);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
    }
}

/**
 * @brief  日志任务
 */
void Task_Logger(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    static Tasks_MonitorSnapshot_t stats;
    static uint64_t previous_cpu[HEARTBEAT_COUNT], previous_total;
    const char *names[HEARTBEAT_COUNT] = {"MotorCtrl", "Sensor", "UI", "SysMgr", "Logger", "Storage", "Idle", "Watchdog"};
    uint32_t log_round = 0;

    for (;;) {
        report_heartbeat(HEARTBEAT_LOGGER);
        Motor_Telemetry_t telemetry = {0};
        SensorData_t serial_sensor = {0}; System_Status_t serial_status = {0};
        (void)xQueuePeek(motor_telemetry_queue, &telemetry, 0);
        (void)xQueuePeek(g_sensor_queue, &serial_sensor, 0);
        (void)xQueuePeek(system_status_queue, &serial_status, 0);
        AppSerial_Poll(&serial_status, &telemetry, &serial_sensor);
        if (xQueuePeek(motor_telemetry_queue, &telemetry, 0) == pdPASS) {
            /* 使用整数小数输出，避免 newlib-nano 缺省关闭浮点 printf。 */
            uint32_t frequency_millihz = (uint32_t)(telemetry.speed.frequency_hz * 1000.0f);
            printf("[SPEED] duty=%u/1000 en=%u period=%lu tick=%luHz freq=%lu.%03luHz rpm=%lu valid=%u sweep=%u state=%u\r\n",
                   (unsigned)telemetry.motor.duty, (unsigned)telemetry.motor.enabled,
                   (unsigned long)telemetry.speed.period_ticks,
                   (unsigned long)telemetry.speed.timer_tick_hz,
                   (unsigned long)(frequency_millihz / 1000U),
                   (unsigned long)(frequency_millihz % 1000U),
                   (unsigned long)telemetry.speed.rpm, (unsigned)telemetry.speed.valid,
                   (unsigned)telemetry.sweep_active, (unsigned)telemetry.motor.state);
            System_Status_t status = {0};
            (void)xQueuePeek(system_status_queue, &status, 0);
            printf("[CONTROL] mode=%s target=%u error=%ld faults=0x%02lX\r\n",
                StateMachine_GetStateName(status.state), (unsigned)telemetry.target_rpm,
                (long)telemetry.target_rpm - (long)telemetry.speed.rpm, (unsigned long)telemetry.faults);
        }
        SensorData_t sensor;
        if (xQueuePeek(g_sensor_queue, &sensor, 0) == pdPASS) {
            printf("[SENSOR] adc=%u,%u,%u,%u mV=%ld,%ld,%ld,%ld T=%ldmC RH=%lum%% P=%luPa wind=%lumm/s gas=%ldmV warm=%u uncal=%u valid=0x%02lX errors=0x%02lX age=%lums\r\n",
                (unsigned)sensor.adc_raw[0], (unsigned)sensor.adc_raw[1], (unsigned)sensor.adc_raw[2], (unsigned)sensor.adc_raw[3],
                (long)(sensor.adc_voltage[0]*1000), (long)(sensor.adc_voltage[1]*1000),
                (long)(sensor.adc_voltage[2]*1000), (long)(sensor.adc_voltage[3]*1000),
                (long)(sensor.temperature*1000), (unsigned long)(sensor.humidity*1000), (unsigned long)sensor.pressure,
                (unsigned long)(sensor.wind_speed*1000), (long)(sensor.gas_voltage*1000),
                (unsigned)sensor.mq_preheating, (unsigned)sensor.mq_uncalibrated,
                (unsigned long)sensor.valid_flags, (unsigned long)sensor.error_flags,
                (unsigned long)(HAL_GetTick()-sensor.timestamp_ms));
        }

        if (++log_round % 5U == 0U) {
            Tasks_GetMonitorSnapshot(&stats);
            uint64_t delta = stats.cpu_total_us - previous_total;
            uint64_t idle = stats.tasks[HEARTBEAT_IDLE].cpu_us - previous_cpu[HEARTBEAT_IDLE];
            uint32_t busy = delta ? (uint32_t)((delta - idle) * 1000U / delta) : 0;
            printf("[MONITOR] cpu=%lu/1000 heap=%lu min_heap=%lu\r\n", (unsigned long)busy,
                (unsigned long)stats.heap_free_bytes, (unsigned long)stats.heap_min_bytes);
            for (unsigned id = 0; id < HEARTBEAT_COUNT; ++id) {
                Runtime_Task_t *t = &stats.tasks[id];
                uint32_t share = delta ? (uint32_t)((t->cpu_us - previous_cpu[id]) * 1000U / delta) : 0;
                printf("[TASK] %s cpu=%lu/1000 stack_free=%lu exec_max_us=%lu jitter_max_us=%lu period_min_us=%lu period_max_us=%lu overruns=%lu\r\n",
                    names[id], (unsigned long)share, (unsigned long)stats.stack_free_bytes[id],
                    (unsigned long)t->execution_max_us, (unsigned long)t->jitter_max_us,
                    (unsigned long)(t->samples ? t->period_min_us : 0), (unsigned long)t->period_max_us,
                    (unsigned long)t->overruns);
                previous_cpu[id] = t->cpu_us;
            }
            previous_total = stats.cpu_total_us;
        }
        monitor_end(HEARTBEAT_LOGGER);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TASK_PERIOD_LOGGER_MS));
    }
}
