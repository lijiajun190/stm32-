/* 主机回放真实模式/UI/蜂鸣器代码，覆盖时间边界、无效值和安全优先级。 */
#include "state_machine.h"
#include "ui_model.h"
#include "control_config.h"
#include "motor_control.h"
#include "buzzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

GPIO_TypeDef test_gpioa, test_gpiob, test_gpioc;
uint32_t test_primask;
static unsigned checks, delay_calls;
#define CHECK(x) do { ++checks; if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{ if (state == GPIO_PIN_SET) port->ODR |= pin; else port->ODR &= ~pin; }
void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin) { port->ODR ^= pin; }
void HAL_Delay(uint32_t ms) { (void)ms; ++delay_calls; }

static SensorData_t sensor;
static SystemConfig_t config;
static void update(uint32_t now, uint32_t faults)
{ sensor.timestamp_ms = now; StateMachine_Update(&sensor, now, faults); }
static void input(UI_InputType_t type, int32_t delta, uint32_t now)
{ UI_Event_t event; event.type = type; event.delta = delta; StateMachine_HandleInputAt(&event, now); }
static void init(void)
{
    sensor = (SensorData_t){0};
    sensor.valid_flags = SENSOR_AUTO_REQUIRED | SENSOR_VALID_ADC | SENSOR_VALID_BMP280;
    sensor.temperature_raw = sensor.temperature = 30.0f;
    sensor.gas_voltage = 1.0f; sensor.wind_speed = 2.0f;
    sensor.humidity = 50.0f; sensor.pressure = 100000.0f;
    SystemConfig_Defaults(&config);
    StateMachine_Init(); StateMachine_ProcessEvent(EVENT_POWER_ON);
    StateMachine_ApplyConfig(&config); update(0, 0);
}
static void mode(uint32_t now)
{ input(UI_INPUT_MODE, 0, now); update(now, 0); }

static void test_modes(void)
{
    init(); CHECK(config.shutdown_delay_ms == CONTROL_SHUTDOWN_DELAY_MS);
    mode(100); CHECK(StateMachine_GetState() == SYSTEM_STATE_MANUAL);
    input(UI_INPUT_TOGGLE_RUN, 0, 200); update(200, 0);
    CHECK(StateMachine_GetStatus().target_rpm == CONTROL_DEFAULT_RPM);
    input(UI_INPUT_TOGGLE_RUN, 0, 300); update(300, 0);
    CHECK(StateMachine_GetStatus().shutdown_pending && StateMachine_GetStatus().run_requested);
    input(UI_INPUT_ADJUST, 5, 400); update(400, 0);
    CHECK(StateMachine_GetStatus().target_rpm == CONTROL_DEFAULT_RPM); /* 倒计时固定余烟档。 */
    input(UI_INPUT_TOGGLE_RUN, 0, 500); update(500, 0);
    CHECK(!StateMachine_GetStatus().shutdown_pending && StateMachine_GetStatus().target_rpm == 1100);
    input(UI_INPUT_TOGGLE_RUN, 0, 600); update(600 + config.shutdown_delay_ms - 1U, 0);
    CHECK(StateMachine_GetStatus().shutdown_remaining_ms == 1);
    update(600 + config.shutdown_delay_ms, 0);
    CHECK(StateMachine_GetState() == SYSTEM_STATE_STANDBY && !StateMachine_GetStatus().run_requested);

    mode(31000); input(UI_INPUT_TOGGLE_RUN, 0, 31100); update(31100, 0);
    input(UI_INPUT_TOGGLE_RUN, 0, 31200); update(31200, 0);
    input(UI_INPUT_STOP, 0, 31300); input(UI_INPUT_TOGGLE_RUN, 0, 31300); update(31300, 0);
    CHECK(!StateMachine_GetStatus().run_requested && !StateMachine_GetStatus().shutdown_pending);
    input(UI_INPUT_TOGGLE_RUN, 0, 31400); update(31400, 0);
    input(UI_INPUT_RESET, 0, 31500); update(31500, 0);
    CHECK(StateMachine_GetStatus().target_rpm == 0 && !StateMachine_GetStatus().run_requested);

    init(); mode(100); mode(200); mode(300);
    CHECK(StateMachine_GetState() == SYSTEM_STATE_BOOST && StateMachine_GetStatus().target_rpm == 0);
    input(UI_INPUT_TOGGLE_RUN, 0, 400); update(400, 0);
    CHECK(StateMachine_GetStatus().target_rpm == config.max_rpm && !StateMachine_GetStatus().automatic_required);
    update(400 + CONTROL_BOOST_DURATION_MS - 1U, 0); CHECK(StateMachine_GetStatus().boost_remaining_ms == 1);
    update(400 + CONTROL_BOOST_DURATION_MS, 0);
    CHECK(StateMachine_GetState() == SYSTEM_STATE_STANDBY && StateMachine_GetStatus().target_rpm == 0);

    init(); config.max_rpm = config.manual_rpm = 1000;
    StateMachine_ApplyConfig(&config); update(0, 0);
    mode(100); mode(200); mode(300); input(UI_INPUT_TOGGLE_RUN, 0, 400); update(400, 0);
    CHECK(StateMachine_GetStatus().target_rpm == 1000);
    mode(500); CHECK(StateMachine_GetState() == SYSTEM_STATE_CLEANING && !StateMachine_GetStatus().run_requested);
    input(UI_INPUT_TOGGLE_RUN, 0, 600); input(UI_INPUT_ADJUST, INT_MAX, 600); update(600, 0);
    CHECK(StateMachine_GetStatus().target_rpm == 0);
    mode(700); CHECK(StateMachine_GetState() == SYSTEM_STATE_SLEEP);
    input(UI_INPUT_TOGGLE_RUN, 0, 800); input(UI_INPUT_TOGGLE_RUN, 0, 800); update(800, 0);
    CHECK(StateMachine_GetState() == SYSTEM_STATE_STANDBY && StateMachine_GetStatus().target_rpm == 0);
    input(UI_INPUT_ACTIVITY, 0, 900); update(900 + CONTROL_STANDBY_SLEEP_MS - 1U, 0);
    CHECK(StateMachine_GetState() == SYSTEM_STATE_STANDBY);
    update(900 + CONTROL_STANDBY_SLEEP_MS, 0); CHECK(StateMachine_GetState() == SYSTEM_STATE_SLEEP);
    input(UI_INPUT_ADJUST, 1, 400000); update(400000, 0); CHECK(StateMachine_GetState() == SYSTEM_STATE_STANDBY);

    /* 自动故障保留完整传感器复位要求，不能因 ERROR 模式绕过气体有效性。 */
    init(); mode(100); mode(200); input(UI_INPUT_TOGGLE_RUN, 0, 300); update(300, 0);
    CHECK(StateMachine_GetStatus().automatic_required);
    update(400, CONTROL_FAULT_SENSOR); input(UI_INPUT_RESET, 0, 500); update(500, CONTROL_FAULT_SENSOR);
    CHECK(StateMachine_GetStatus().automatic_required && StateMachine_GetStatus().reset_pending);
    CHECK(!StateMachine_GetStatus().run_requested && StateMachine_GetStatus().target_rpm == 0);
    update(600, 0); CHECK(StateMachine_GetState() == SYSTEM_STATE_STANDBY && !StateMachine_GetStatus().automatic_required);

    init(); mode(100); mode(200); input(UI_INPUT_TOGGLE_RUN, 0, 300); update(300, 0);
    input(UI_INPUT_TOGGLE_RUN, 0, 400); update(400, 0);
    CHECK(StateMachine_GetStatus().shutdown_pending && StateMachine_GetStatus().automatic_required);
    update(500, CONTROL_FAULT_SENSOR);
    CHECK(!StateMachine_GetStatus().shutdown_pending && StateMachine_GetStatus().target_rpm == 0);
    {
        MotorControl_t motor; MotorControl_Input_t request = {0};
        MotorControl_Init(&motor); MotorControl_LatchFault(&motor, CONTROL_FAULT_SENSOR);
        request.now_ms = 3000; request.sensor = &sensor; request.reset_fault = true;
        request.automatic = StateMachine_GetStatus().automatic_required;
        sensor.timestamp_ms = 3000; sensor.valid_flags = SENSOR_VALID_SHT30;
        CHECK(MotorControl_Step(&motor, &request) == 0 && motor.faults == CONTROL_FAULT_SENSOR);
        sensor.valid_flags = SENSOR_AUTO_REQUIRED;
        request.now_ms = sensor.timestamp_ms = 3050;
        CHECK(MotorControl_Step(&motor, &request) == 0 && !motor.faults);
    }

    /* HAL 毫秒计数回绕时，运行时长和关机时长依旧使用无符号差值。 */
    init(); mode(UINT_MAX - 1000U); mode(UINT_MAX - 900U); mode(UINT_MAX - 800U);
    input(UI_INPUT_TOGGLE_RUN, 0, UINT_MAX - 700U); update(UINT_MAX - 700U, 0);
    update(300, 0); CHECK(StateMachine_GetStatus().run_requested);
    update(CONTROL_BOOST_DURATION_MS - 701U, 0); CHECK(StateMachine_GetStatus().target_rpm == 0);
    init(); config.shutdown_delay_ms = 0; StateMachine_ApplyConfig(&config); update(0, 0);
    mode(10); input(UI_INPUT_TOGGLE_RUN, 0, 20); update(20, 0);
    input(UI_INPUT_TOGGLE_RUN, 0, 30); update(30, 0);
    CHECK(!StateMachine_GetStatus().run_requested && !StateMachine_GetStatus().shutdown_pending);
}

static void test_ui(void)
{
    UI_Model_t ui; UI_Result_t result; UI_View_t view = {0};
    char lines[UI_LINE_COUNT][UI_LINE_SIZE];
    init(); UI_Model_Init(&ui, &config);
    System_Status_t status = StateMachine_GetStatus();
    result = UI_Model_Action(&ui, UI_ACTION_PAGE, 0, &config, &status);
    CHECK(ui.page == UI_PAGE_SENSORS && result.input.type == UI_INPUT_ACTIVITY);
    (void)UI_Model_Action(&ui, UI_ACTION_PAGE, 0, &config, &status);
    result = UI_Model_Action(&ui, UI_ACTION_PAGE, 0, &config, &status);
    CHECK(ui.page == UI_PAGE_SETTINGS && result.input.type == UI_INPUT_STOP);
    result = UI_Model_Action(&ui, UI_ACTION_ROTATE, INT_MAX, &config, &status);
    CHECK(ui.draft.manual_rpm == ui.draft.max_rpm && ui.dirty && SystemConfig_Validate(&ui.draft));
    (void)UI_Model_Action(&ui, UI_ACTION_POWER, 0, &config, &status);
    (void)UI_Model_Action(&ui, UI_ACTION_ROTATE, INT_MIN, &config, &status);
    CHECK(ui.draft.max_rpm == CONTROL_TARGET_RPM_MIN && ui.draft.manual_rpm == CONTROL_TARGET_RPM_MIN);
    (void)UI_Model_Action(&ui, UI_ACTION_POWER, 0, &config, &status);
    (void)UI_Model_Action(&ui, UI_ACTION_DOWN, 0, &config, &status);
    CHECK(ui.draft.shutdown_delay_ms == config.shutdown_delay_ms - 10000U);
    result = UI_Model_Action(&ui, UI_ACTION_MODE, 0, &config, &status);
    CHECK(result.save_config && ui.page == UI_PAGE_MAIN && result.input.type == UI_INPUT_ACTIVITY);

    for (unsigned i = 0; i < 3; ++i) (void)UI_Model_Action(&ui, UI_ACTION_PAGE, 0, &config, &status);
    CHECK(ui.page == UI_PAGE_SETTINGS && ui.draft.manual_rpm == config.manual_rpm && !ui.dirty);
    for (unsigned i = 0; i < 8; ++i) {
        result = UI_Model_Action(&ui, UI_ACTION_ROTATE, INT_MAX, &config, &status);
        CHECK(SystemConfig_Validate(&ui.draft));
        result = UI_Model_Action(&ui, UI_ACTION_ROTATE, INT_MIN, &config, &status);
        CHECK(SystemConfig_Validate(&ui.draft));
        (void)UI_Model_Action(&ui, UI_ACTION_POWER, 0, &config, &status);
    }
    CHECK(ui.field == 0);
    result = UI_Model_Action(&ui, UI_ACTION_PAGE, 0, &config, &status);
    CHECK(!ui.dirty && !result.save_config); /* 离开页面放弃未提交的编辑。 */
    status.state = SYSTEM_STATE_SLEEP;
    result = UI_Model_Action(&ui, UI_ACTION_MODE, 0, &config, &status);
    CHECK(result.input.type == UI_INPUT_ACTIVITY && ui.page == UI_PAGE_MAIN);
    status.state = SYSTEM_STATE_ERROR;
    result = UI_Model_Action(&ui, UI_ACTION_MODE, 0, &config, &status);
    CHECK(!result.save_config && ui.page == UI_PAGE_FAULTS);
    result = UI_Model_Action(&ui, UI_ACTION_RESET, 0, &config, &status);
    CHECK(result.input.type == UI_INPUT_RESET);

    view.status.state = SYSTEM_STATE_MANUAL;
    view.sensor = sensor; view.now_ms = 100; view.sensor.timestamp_ms = 100;
    for (unsigned p = 0; p < UI_PAGE_COUNT; ++p) {
        ui.page = (UI_Page_t)p;
        UI_Model_Render(&ui, &view, lines);
        for (unsigned row = 0; row < UI_LINE_COUNT; ++row) {
            CHECK(strlen(lines[row]) <= 21U);
            for (unsigned col = 0; lines[row][col]; ++col)
                CHECK(lines[row][col] >= ' ' && lines[row][col] <= 'Z');
        }
    }
    ui.page = UI_PAGE_SENSORS; UI_Model_Render(&ui, &view, lines);
    CHECK(strcmp(lines[2], "GAS:1.000V") == 0);
    view.sensor.valid_flags = 0; UI_Model_Render(&ui, &view, lines); CHECK(strcmp(lines[2], "GAS:--") == 0);
    view.sensor = sensor; view.now_ms = 1000; UI_Model_Render(&ui, &view, lines); CHECK(strcmp(lines[2], "GAS:--") == 0);
    view.now_ms = 0; view.sensor.gas_voltage = (float)NAN;
    UI_Model_Render(&ui, &view, lines); CHECK(strcmp(lines[2], "GAS:--") == 0);
    for (unsigned i = 0; i < 8; ++i) {
        view.motor_faults = 1UL << i; UI_Model_Render(&ui, &view, lines);
        CHECK(strcmp(lines[0], "FAULTS") == 0 && lines[3][0] && lines[5][0]);
        for (unsigned row = 0; row < UI_LINE_COUNT; ++row) CHECK(strlen(lines[row]) <= 21U);
    }
    view.motor_faults = CONTROL_FAULT_SENSOR | CONTROL_FAULT_STALL;
    ui.fault_index = 1; UI_Model_Render(&ui, &view, lines); CHECK(strcmp(lines[3], "SENSOR NOT READY") == 0);
    view.motor_faults = 0; view.last_fault_valid = true; view.last_fault.reason = FAULT_REASON_SENSOR;
    view.last_fault.detail = SENSOR_ERROR_SHT30; ui.page = UI_PAGE_FAULTS;
    UI_Model_Render(&ui, &view, lines); CHECK(strcmp(lines[6], "CHECK SHT30 / I2C1") == 0);
    view.status.state = SYSTEM_STATE_SLEEP; UI_Model_Render(&ui, &view, lines);
    for (unsigned row = 0; row < UI_LINE_COUNT; ++row) CHECK(!lines[row][0]);
}

static bool buzzer_on(void) { return !(test_gpioc.ODR & BUZZER_Pin); }
static void test_buzzer(void)
{
    Buzzer_Init(); CHECK(!Buzzer_IsBusy() && !buzzer_on());
    Buzzer_Request(BUZZER_SIGNAL_CLICK, 100); CHECK(Buzzer_IsBusy() && buzzer_on());
    Buzzer_Process(149); CHECK(buzzer_on()); Buzzer_Process(150);
    CHECK(!Buzzer_IsBusy() && !buzzer_on());
    Buzzer_Request(BUZZER_SIGNAL_ERROR, 200); Buzzer_Request(BUZZER_SIGNAL_CLICK, 220);
    Buzzer_Process(250); CHECK(!buzzer_on() && Buzzer_IsBusy());
    Buzzer_Process(350); CHECK(buzzer_on()); Buzzer_Process(400); CHECK(!buzzer_on());
    Buzzer_Process(500); CHECK(buzzer_on()); Buzzer_Process(550); CHECK(!Buzzer_IsBusy() && !buzzer_on());
    Buzzer_Request(BUZZER_SIGNAL_MODE, UINT_MAX - 10U); Buzzer_Process(188); CHECK(Buzzer_IsBusy());
    Buzzer_Process(189); CHECK(!Buzzer_IsBusy() && !buzzer_on());
    Buzzer_Request(BUZZER_SIGNAL_WARNING, 1000); Buzzer_Init(); CHECK(!Buzzer_IsBusy() && !buzzer_on());
    CHECK(delay_calls == 0);
}
int main(void)
{
    test_modes(); test_ui(); test_buzzer();
    printf("PASS: %u complete-mode/UI/nonblocking-buzzer checks\n", checks);
    return 0;
}
