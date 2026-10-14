/** 集中配置：全部为待实测整定的初值，不代表实际风机额定值。 */
#ifndef CONTROL_CONFIG_H
#define CONTROL_CONFIG_H
#define CONTROL_TARGET_RPM_MIN       300U
#define CONTROL_TARGET_RPM_MAX       1800U
#define CONTROL_DEFAULT_RPM          600U
#define CONTROL_RPM_STEP             100U
#define CONTROL_ENCODER_COUNTS       4
#define CONTROL_OVERSPEED_RPM        2200U
#define CONTROL_RESET_RPM            100U
#define CONTROL_STALL_RPM            80U
#define CONTROL_STALL_TIMEOUT_MS     1500U
#define CONTROL_START_GRACE_MS       2000U
#define CONTROL_START_DUTY           300U
#define CONTROL_MAX_DUTY             800U
#define CONTROL_MIN_RUN_DUTY         100U
/* PID 输出每 50 ms 的变化上限；停机和故障仍然立即降为 0。 */
#define CONTROL_DUTY_SLEW_UP         50U
#define CONTROL_DUTY_SLEW_DOWN       80U
#define CONTROL_SENSOR_STALE_MS      500U
#define CONTROL_COMMAND_STALE_MS     500U
#define CONTROL_MAX_STEP_MS          150U
#define CONTROL_MAX_TEMPERATURE_C    80.0f
#define CONTROL_RESET_TEMPERATURE_C  70.0f
#define CONTROL_PID_KP               0.25f
#define CONTROL_PID_KI               0.20f
#define CONTROL_PID_KD               0.0f
#define CONTROL_PID_INTEGRAL_LIMIT   4000.0f
#define CONTROL_AUTO_SLEW_RPM        50U  /* 每 100 ms 最大变化 */
#define CONTROL_GAS_LOW_V            0.4f
#define CONTROL_GAS_HIGH_V           2.5f
#define CONTROL_SENSOR_ALPHA         0.2f
#define CONTROL_AUTO_MIN_WIND_MPS    1.0f
#define CONTROL_AUTO_WIND_BOOST      0.1f
#define CONTROL_AUTO_TEMP_BASE_C     45.0f
#define CONTROL_AUTO_TEMP_SCALE_C    100.0f
#define CONTROL_I2C_TIMEOUT_MS       5U
/* 模式时间均使用 HAL 毫秒时基，待上板后按实际排烟效果调整。 */
#define CONTROL_BOOST_DURATION_MS    180000U
#define CONTROL_STANDBY_SLEEP_MS     300000U
#define CONTROL_SHUTDOWN_DELAY_MS    30000U
/* 仅专门的开环验证固件启用；正常固件拒绝非零占空比/扫测 API。 */
#ifndef APP_ENABLE_OPEN_LOOP_TESTS
#define APP_ENABLE_OPEN_LOOP_TESTS 0
#endif
#endif
