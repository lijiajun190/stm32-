# BSP GPIO 与电机开环控制

本次对应截图第 3、4 项。代码已经完成逻辑实现和 STM32 Debug 构建，真实 LED、
按键、电机、PWM 波形和 RPM 仍需按板卡接线进行上板验证。

## GPIO 逻辑

| 信号 | 配置 | 软件有效电平 |
|---|---|---|
| `PA0 / KEY_ENCODER` | 上拉、EXTI 下降沿 | 低电平按下 |
| `PC2 / KEY_MODE` | 下拉、EXTI 上升沿 | 高电平按下 |
| `PC3 / KEY_UP` | 下拉、EXTI 上升沿 | 高电平按下 |
| `PC4 / KEY_DOWN` | 下拉、普通输入 | 高电平按下 |
| `PB12 / LED_R` | 推挽输出 | 低电平点亮 |
| `PB14 / LED_G` | 推挽输出 | 低电平点亮 |
| `PC13 / LED_BLUE` | 推挽输出 | 低电平点亮 |
| `PC7 / MOTOR_EN` | 推挽输出 | 高电平使能 |
| `PC9 / MOTOR_DIR` | 推挽输出 | 低电平默认方向 |

`Bsp_GPIO_Init()` 在 CubeMX 初始化之后写入安全状态：三路 LED 熄灭、PC7
拉低、PC9 设为默认方向。`Bsp_LED_Set/Toggle()`、`Bsp_Key_Read()`、
`Bsp_Motor_Enable()` 和 `Bsp_Motor_SetDirection()` 已提供实际 GPIO 操作。
输入驱动的按键消抖释放路径也已修正，启动时已经按住的键会被忽略到释放，
避免上电误触发操作。

## PWM 与开环 API

TIM1 的 `PB15 / TIM1_CH3N` 使用互补输出，`OCNPolarity=LOW`，并保持
`MOTOR_EN=LOW` 作为关闭状态。`Bsp_PWM_SetDuty()` 将 0~1000 映射为
`CCR3=0~1000`（当前 `ARR=999`）；`Bsp_PWM_Start()` 只启动 `CH3N`，
`Motor_Stop()` 先关闭 PC7，再调用 `Bsp_PWM_Stop()` 停止 PWM。

默认开环按键入口：PC2 短按并释放选 30%，PC3 选 60%，PC4 选 100%；PA0
按下消抖后选 0% 停机。按住 PA0 时其他按键不能选档。功能键的长按和重复
事件不会启动电机。上电已经按住的键需先松开后再按。
`tasks.h` 中的 `APP_OPEN_LOOP_BUTTON_CONTROL=0` 可关闭这一阶段的按键入口，
后续正式手动/自动模式应替换该映射。按键扫描周期 10 ms，编码器更新 100 ms。

电机任务是唯一执行器写入者，可使用：

```c
Tasks_SetMotorDuty(0);       // 0%
Tasks_SetMotorDuty(300);     // 30%
Tasks_SetMotorDuty(600);     // 60%
Tasks_SetMotorDuty(1000);    // 100%
Tasks_SetMotorDutyPercent(30);
```

测试四个固定档位时，逐次调用上述接口并等待串口日志。也可以启动自动
扫测（默认不启动）：

```c
Tasks_StartOpenLoopSweep(5000);  // 0/30/60/100/0%，每档 5 s，单次结束
Tasks_StopOpenLoopSweep();       // 请求停机，下一次电机任务周期执行
```

上述 API 返回命令是否成功提交；应检查返回值。电机任务每 50 ms 执行命令；
`dwell_ms` 有效范围为 1000~60000 ms，默认不自动启动扫测。设置固定占空比
会取消当前扫测。完整扫测最终回到 0%，PWM 启动失败时中止扫测。
扫测状态只由电机任务持有，其他任务通过队列操作，避免同时写入参数。
日志任务每秒输出实际电机状态：

```text
[SPEED] duty=600/1000 en=1 period=... tick=1000000Hz freq=...Hz rpm=... valid=1 sweep=1 state=1
```

测速无效时 RPM 为 0 且 `valid=0`；电机停下或遮挡传感器后，原来的 RPM
在 1000 ms 无脉冲超时后归零。开环阶段测速丢失不自动取消固定占空比；
堵转/超速保护和 PID 属于后续阶段。

## 软件验证

在仓库根目录运行 `./tests/run_host_tests.ps1`。
GPIO/按键/扫测测试直接编译正式 `gpio.c`、`bsp_gpio.c`、`led.c` 和
`motor_open_loop.c`；按键驱动测试使用正式 `button.c` 的状态机。旧版 LCC64
不支持其中的指定初始化表，脚本在 `build` 生成按字段声明顺序初始化的副本，
仅删除字段标记，不改动配置值和处理逻辑。

GPIO/按键/开环测试 112 项通过，测速与 PWM 测试在每转 1、2 脉冲配置下
各 67 项通过。覆盖生成的 GPIO 上下拉和 EXTI 边沿、三路 LED 映射、BSP
按键有效电平、按下/释放抖动、长按、启动按住、毫秒回绕、扫测结束和取消，
以及 0/30/60/100% CCR 值。主机 HAL 替身不能验证真实信号极性和波形。

## 上板验收顺序

1. 下载 `firmware/build/Debug/stm32-2.elf`，先不连接电机功率级。确认上电后 LED 全灭、
   PC7 低、PC9 为低，串口能看到任务创建成功及开环按键映射提示。
2. 逐个按下四个按键，检查 PA0 只在低电平按下时触发，PC2/PC3/PC4 只在高
   电平按下时触发；每次按键只产生一次稳定事件，松开抖动不重复触发。
3. 使用 `Bsp_LED_Set()` 或调试器分别设置 RGB LED，确认低电平点亮。
4. 示波器探头接 PB15，依次按 PA0/PC2/PC3/PC4 设置 0/300/600/1000，确认非零
   档位频率约 20 kHz，
   高电平比例分别约为 0%/30%/60%/100%。确认 0% 时 PC7 仍为低。
5. 连接电机驱动后先使用 30% 档，确认 PC7 高电平、PC9 方向正确，再观察
   PA6 捕获和串口 RPM。逐步测试 60%、100%，记录占空比、周期、频率、RPM。
6. 停止脉冲或断开传感器，确认规定超时后 `rpm=0 valid=0`；调用停止接口后
   PB15 无 PWM 且 PC7 低。

本次未执行烧录、示波器测量、真实电机运行和连续运行测试；这些由后续上板验收完成。
未创建 Git 提交。
