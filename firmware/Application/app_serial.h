#ifndef APP_SERIAL_H
#define APP_SERIAL_H
#include "tasks.h"
void AppSerial_Poll(const System_Status_t *status, const Motor_Telemetry_t *motor, const SensorData_t *sensor);
#endif
