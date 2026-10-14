/* 固定 50 ms 步进且刷新时间戳，隔离每类故障的触发条件。 */
#include "motor_control.h"
#include "control_config.h"
#include "state_machine.h"
#include "sensor_filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)
static SensorData_t s;
static MotorControl_t c;
static MotorControl_Input_t in;
static void setup(void) {
    s = (SensorData_t){0};
    s.valid_flags = SENSOR_AUTO_REQUIRED | SENSOR_VALID_ADC | SENSOR_VALID_BMP280;
    s.gas_voltage = 1; s.wind_speed = 2; s.temperature = s.temperature_raw = 30;
    s.humidity = 50; s.pressure = 100000;
    in = (MotorControl_Input_t){0}; in.sensor = &s; in.target_rpm = CONTROL_DEFAULT_RPM;
    MotorControl_Init(&c);
}
static uint16_t step(uint32_t now) {
    in.now_ms = in.command_timestamp_ms = s.timestamp_ms = now;
    return MotorControl_Step(&c, &in);
}
static void input(UI_InputType_t type, int32_t delta) {
    UI_Event_t e; e.type = type; e.delta = delta; StateMachine_HandleInput(&e);
}
int main(void) {
    uint32_t t;
    setup(); in.target_rpm = 0; CHECK(step(0) == 0 && !c.running && !c.faults);
    in.target_rpm = 600; CHECK(step(50) == CONTROL_DUTY_SLEW_UP);
    for (t=100; t<2050; t+=50) {
        uint16_t expected = t / 50U * CONTROL_DUTY_SLEW_UP;
        if (expected > CONTROL_START_DUTY) expected = CONTROL_START_DUTY;
        CHECK(step(t) == expected);
        in.target_rpm = (t%100) ? 600 : 700; /* 改目标不得延长宽限。 */
    }
    CHECK(step(2050) == 0 && c.faults == CONTROL_FAULT_FEEDBACK);
    in.speed_valid = true; in.rpm = 600;
    CHECK(step(2100) == 0 && c.faults == CONTROL_FAULT_FEEDBACK);
    in.target_rpm = 0; in.reset_fault = true; in.rpm = 200;
    CHECK(step(2150) == 0 && c.faults);
    in.rpm = 0; CHECK(step(2200) == 0 && !c.faults && !c.running);
    in.reset_fault = false; CHECK(step(2250) == 0);

    setup(); CHECK(step(0) == CONTROL_DUTY_SLEW_UP);
    in.speed_valid = true; in.rpm = 550; CHECK(step(50) <= CONTROL_MAX_DUTY && c.feedback_acquired);
    in.speed_valid = false; CHECK(step(100) == 0 && c.faults == CONTROL_FAULT_FEEDBACK);
    in.target_rpm = 0; in.reset_fault = true;
    CHECK(step(150) == 0 && c.faults);
    for (t=200; t<=2100; t+=50) (void)step(t);
    CHECK(!c.faults);

    setup(); in.speed_valid = true; in.rpm = CONTROL_OVERSPEED_RPM+1;
    CHECK(step(0) == 0 && c.faults == CONTROL_FAULT_OVERSPEED);
    in.target_rpm = 0; in.reset_fault = true; CHECK(step(50) == 0 && c.faults);
    in.rpm = 0; s.temperature_raw = 75; CHECK(step(100) == 0 && c.faults);
    s.temperature_raw = 30; CHECK(step(150) == 0 && !c.faults);

    setup(); in.speed_valid = true; in.rpm = 50;
    for (t=0; t<CONTROL_START_GRACE_MS+CONTROL_STALL_TIMEOUT_MS; t+=50)
        CHECK(step(t)>0 && !c.faults);
    CHECK(step(t)==0 && c.faults==CONTROL_FAULT_STALL);
    setup(); in.speed_valid = true; in.rpm = 600;
    for (t=0; t<=2000; t+=50) (void)step(t);
    in.rpm = 50; for (; t<=2500; t+=50) CHECK(step(t)>0);
    in.rpm = 600; CHECK(step(t)>0);
    in.rpm = 50; CHECK(step(t+50)>0 && !c.faults);

    setup(); s.valid_flags &= ~SENSOR_VALID_BMP280;
    CHECK(step(0)==CONTROL_DUTY_SLEW_UP); /* 气压是可选数据。 */
    s.valid_flags &= ~SENSOR_VALID_SHT30; CHECK(step(50)==0 && c.faults==CONTROL_FAULT_SENSOR);
    setup(); s.valid_flags &= ~SENSOR_VALID_GAS; CHECK(step(0)>0);
    in.automatic = true; CHECK(step(50)==0 && c.faults==CONTROL_FAULT_SENSOR);
    setup(); s.temperature_raw = CONTROL_MAX_TEMPERATURE_C;
    CHECK(step(0)==0 && c.faults==CONTROL_FAULT_OVER_TEMP);
    setup(); (void)step(1000); in.now_ms=1050; in.command_timestamp_ms=1050;
    CHECK(MotorControl_Step(&c,&in)>0);
    s.timestamp_ms=400; in.now_ms=1100; in.command_timestamp_ms=1100;
    CHECK(MotorControl_Step(&c,&in)==0 && c.faults==CONTROL_FAULT_SENSOR);
    setup(); (void)step(0); in.now_ms=550; s.timestamp_ms=550;
    CHECK(MotorControl_Step(&c,&in)==0 && (c.faults & CONTROL_FAULT_COMMAND));
    setup(); (void)step(0); CHECK(step(200)==0 && c.faults==CONTROL_FAULT_TIMING);
    setup(); in.target_rpm=60000; CHECK(step(0)==0 && c.faults==CONTROL_FAULT_COMMAND);
    setup(); CHECK(step(UINT_MAX-100U)>0); CHECK(step(UINT_MAX-50U)>0); CHECK(step(0)>0 && !c.faults);

    setup(); s.gas_voltage=3; CHECK(MotorControl_AutoTarget(&s,300)==350);
    s.gas_voltage=0.2f; CHECK(MotorControl_AutoTarget(&s,1800)==1750);
    CHECK(MotorControl_AutoTarget(&s,0)>=CONTROL_TARGET_RPM_MIN);

    { PID_Controller_t pid; PID_Params_t p={1,1,0,1000,0,100,20000};
      PID_Init(&pid,&p); PID_SetSetpoint(&pid,1000);
      for (t=0;t<100;++t) CHECK(PID_Update(&pid,0,0.05f)==100);
      CHECK(pid.state.integral==0);
      PID_SetSetpoint(&pid,10); CHECK(PID_Update(&pid,0,0.05f)<20);
      CHECK(PID_Update(&pid,0,0)==0 && pid.state.output==0);
      CHECK(PID_Update(&pid,0,-1)==0);
      CHECK(PID_Update(NULL,0,0.05f)==0);
      p.output_min=200; PID_SetParams(&pid,&p); CHECK(PID_Update(&pid,0,0.05f)==0);
    }
    setup(); StateMachine_Init(); StateMachine_ProcessEvent(EVENT_POWER_ON);
    CHECK(StateMachine_GetState()==SYSTEM_STATE_STANDBY);
    input(UI_INPUT_MODE,0); input(UI_INPUT_ADJUST,INT_MAX); StateMachine_Update(&s,0,0);
    CHECK(StateMachine_GetStatus().manual_rpm==CONTROL_TARGET_RPM_MAX && StateMachine_GetStatus().target_rpm==0);
    input(UI_INPUT_ADJUST,INT_MIN); CHECK(StateMachine_GetStatus().manual_rpm==CONTROL_TARGET_RPM_MIN);
    input(UI_INPUT_TOGGLE_RUN,0); StateMachine_Update(&s,0,0);
    CHECK(StateMachine_GetStatus().target_rpm==CONTROL_TARGET_RPM_MIN);
    input(UI_INPUT_MODE,0); CHECK(StateMachine_GetState()==SYSTEM_STATE_AUTO && !StateMachine_GetStatus().run_requested);
    input(UI_INPUT_TOGGLE_RUN,0); StateMachine_Update(&s,0,0);
    CHECK(!StateMachine_GetStatus().run_requested && StateMachine_GetStatus().target_rpm==0);
    input(UI_INPUT_TOGGLE_RUN,0); StateMachine_Update(&s,0,0);
    CHECK(StateMachine_GetStatus().target_rpm<=CONTROL_TARGET_RPM_MIN+CONTROL_AUTO_SLEW_RPM);
    StateMachine_Update(&s,0,CONTROL_FAULT_FEEDBACK); input(UI_INPUT_TOGGLE_RUN,0);
    CHECK(StateMachine_GetStatus().target_rpm==0);
    input(UI_INPUT_RESET,0); StateMachine_Update(&s,0,CONTROL_FAULT_FEEDBACK);
    CHECK(StateMachine_GetState()==SYSTEM_STATE_ERROR);
    StateMachine_Update(&s,0,0); CHECK(StateMachine_GetState()==SYSTEM_STATE_STANDBY && !StateMachine_GetStatus().run_requested);
    CHECK(StateMachine_GetStateName((System_State_t)-1)[0]=='U');

    { SensorFilter_t f; SensorFilter_Init(&f); setup();
      s.adc_voltage[0]=1; SensorFilter_Apply(&f,&s); CHECK(s.gas_voltage==1);
      s.adc_voltage[0]=2; SensorFilter_Apply(&f,&s); CHECK(fabs(s.gas_voltage-1.2f)<0.001);
      s.valid_flags=0; SensorFilter_Apply(&f,&s); CHECK(s.gas_voltage==0 && s.wind_speed==0);
      s.valid_flags=SENSOR_AUTO_REQUIRED|SENSOR_VALID_ADC; s.adc_voltage[0]=2.5f;
      s.temperature=20; s.humidity=40; s.wind_speed=2; SensorFilter_Apply(&f,&s);
      CHECK(s.gas_voltage==2.5f && s.temperature==20);
    }
    printf("PASS: %u PID/state/sensor-filter/safety checks\n",checks); return 0;
}
