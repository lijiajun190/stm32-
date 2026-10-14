# PWM 平滑、运行监测与串口升级

## PWM 输出变化率

`MotorControl_Step()` 对启动输出和正常 PID 输出都进行斜率限制：

- 正常增加：每 50 ms 最多增加 50/1000，即每秒最多增加 100% 占空比。
- 正常减少：每 50 ms 最多减少 80/1000，即每秒最多减少 160% 占空比。
- 使用本次真实 `dt_ms` 计算步长，配置集中在 `control_config.h`。
- 停机、传感器失效、测速丢失、堵转、超速和其他锁存故障直接清零；不经过慢降路径。
- 启动目标仍为 300/1000，约 300 ms 爬升到启动目标；现有 2 秒启动反馈宽限不随调速重置。
- 直接占空比/扫测只用于专用开环验证固件，维持原来的固定测试波形。

真实风机的最小有效启动输出、斜率和 PID 参数仍需要上板联合整定。

## 运行监测

使用 TIM2 专用 1 MHz 自由运行计数器，32 位时间差支持回绕。TIM1、TIM3、TIM4、TIM11 的原用途不变。
FreeRTOS `traceTASK_SWITCHED_IN` hook 只记录时间，不打印、不分配、不调用 RTOS API。

| 指标 | 定义 |
| --- | --- |
| CPU 占用 | 1 减去 Idle 调度时间占比；串口日志每 5 秒输出最近窗口，PC 查询显示启动以来累计占比 |
| 任务 CPU | 调度进到下次切出期间的时间，使用 64 位累计值；ISR 时间计入当时被打断的任务 |
| 栈余量 | `uxTaskGetStackHighWaterMark()` 历史最小未使用栈，输出字节数 |
| 剩余堆 | FreeRTOS 当前剩余堆及历史最小剩余堆，单位字节 |
| 执行时间 | 本次循环开始到周期等待前的墙钟时间，包含传感器转换等待、被抢占和串口发送 |
| 控制周期抖动 | 相邻循环实际开始时刻与标称周期的差的绝对值；累计最大、均值及最短/最长周期 |
| overruns | 循环执行时间超过标称周期的次数 |

监测累计值不在 Flash 维护窗口后清零，维护暂停也会反映在周期/执行峰值中。
Idle 不定义业务循环周期，相关执行/抖动值为 0；CPU 与栈余量仍可查询。
堆配置为 28 KB，为正式任务、队列及交互留出分配余量；实际上板需检查剩余堆和所有任务栈水位。

## 两种固件布局

`Debug` / `Release` 仍为独立应用：入口 `0x08000000`，前 256 KB 为应用，扇区 6/7 为原参数日志。
`Upgrade` 为可串口升级应用，必须与本项目 Bootloader 配套使用。

| 可升级布局 | 起始地址 | 容量 | 扇区 |
| --- | --- | --- | --- |
| Bootloader | 0x08000000 | 32 KB | 0/1 |
| 参数日志 Bank 0 | 0x08008000 | 16 KB | 2 |
| 参数日志 Bank 1 | 0x0800C000 | 16 KB | 3 |
| 保留 | 0x08010000 | 64 KB | 4 |
| 应用槽 | 0x08020000 | 128 KB | 5 |
| 升级暂存槽 | 0x08040000 | 128 KB | 6 |
| 保留 | 0x08060000 | 128 KB | 7 |

每个镜像槽最后 32 字节为长度、CRC32、版本和提交信息；实际二进制最大 131040 字节。
应用链接器限制长度并配置 `VTOR=0x08020000`，普通构建不改变地址。
升级布局不自动迁移原扇区 6/7 的配置：首次迁移前用 PC 工具读取并保存配置，迁移后重新提交。
Bootloader 不擦除扇区 0..4 或 7，升级不会触碰新的参数日志。

## 构建与首次下载

在仓库根目录执行，脚本自动寻找已安装的 STM32Cube 工具：

```powershell
./tools/build_firmware.ps1 -Preset Debug
./tools/build_firmware.ps1 -Preset Upgrade
python tools/hood_tool.py pack firmware/build/Upgrade/stm32-2.bin firmware/build/Upgrade/hood-v1.hup --version 1 --installed firmware/build/Upgrade/application-initial.bin
```

首次使用 ST-LINK / STM32CubeProgrammer 下载：

1. `firmware/build/Upgrade/bootloader.bin` 下载到 `0x08000000`。
2. `firmware/build/Upgrade/application-initial.bin` 下载到 `0x08020000`。此文件含 Bootloader 要求的提交尾部；原始 `stm32-2.bin` 不含尾部。
3. 首次从旧布局迁移时，清理新的日志扇区 2/3 以及暂存扇区 6；旧程序数据不能作为新布局日志使用。
4. 复位后应用上电待机，电机不自动启动。

也可首次只下载 Bootloader，然后通过 PC 上传 `.hup` 包。此时没有有效应用，Bootloader 会一直等待串口。
不要将独立 Debug 应用下载到升级槽，也不要将 Upgrade 应用下载到 `0x08000000`。

## PC 工具

Python 3.10+，串口库 `pyserial`；桌面窗口使用 Python 自带 Tkinter。

```powershell
python -m pip install -r tools/requirements.txt
python tools/hood_tool.py gui
python tools/hood_tool.py --port COM3 info
python tools/hood_tool.py --port COM3 status
python tools/hood_tool.py --port COM3 config
python tools/hood_tool.py --port COM3 monitor
python tools/hood_tool.py --port COM3 stop
python tools/hood_tool.py --port COM3 set-config tools/config-example.json
python tools/hood_tool.py --port COM3 flash firmware/build/Upgrade/hood-v1.hup
```

USART1：PA9 TX、PA10 RX，115200、8N1，USB 串口使用 3.3 V TTL 并共地。
一个串口只由一个客户端占用；关闭原串口终端后使用 PC 工具。
桌面窗口提供 Status、Settings、Runtime 和 Firmware 页面，操作放在工作线程，进度和结果返回主线程。
设置提交使用原 SysMgr/Storage 流程，只有状态 `config_saved=1` 才说明当前参数已保存。
PC 不提供直接启动电机命令；启停和模式选择仍通过板上输入，PC 的 STOP 为立即停机请求。

## 升级与恢复

1. `.hup` 文件包含目标地址、镜像长度、CRC32、版本号和包头 CRC；PC 上传前先验证。
2. 运行应用时，PC 先请求停机，等待转速低于 100 RPM、使能关闭、配置已保存，再发 BOOT 请求。
3. BOOT 确认回复到复位期间设置独立禁启动标志，防止 UI 在这个窗口重新启动。
4. Bootloader 在整个升级过程保持 PC7 禁用和 PB15 输入；PC2 按住复位可强制进入 Bootloader。
5. `BEGIN` 只擦除暂存槽，`DATA` 顺序写入并回读，重复数据包幂等确认。
6. `COMMIT` 检查实际 Flash CRC、初始栈指针、Thumb 入口和入口范围，最后写入提交标记。
7. `RUN` 回复后安装暂存镜像，重新校验应用并提交应用尾部，再跳转应用。
8. 上传中断且未提交：原应用保持有效；重新执行 flash 会从头上传。
9. 安装中断：暂存镜像保留，下一次复位重试完整安装；未提交/CRC 错误的应用绝不启动。
10. 每次复位检查应用 CRC；已安装应用意外损坏且暂存仍有效时，也可从暂存恢复。

本实现提供断电后重试和重新上传，不提供跨复位上传偏移续传，也不是新应用运行失败后的自动版本回滚。
镜像校验使用 CRC32 做传输/存储完整性检查，没有实现签名认证。
Bootloader 启用独立看门狗并在 CRC/安装循环中喂狗，应用继续使用现有看门狗监控；擦除时间需真实测量。

## 协议

帧：`A5 5A | version:u8 | command:u8 | sequence:u16 | length:u16 | payload | crc32:u32`，全部小端序。
CRC32 覆盖 version 到 payload。回复 command 为请求值 OR `0x80`，payload 第一个字节为状态。
状态：0 成功、1 非法参数、2 忙、3 CRC 错误、4 Flash 失败、5 状态错误、6 不支持。
设备和 PC 接收均支持噪声恢复、半帧和长度限制，1.5 秒字节间隙清空半帧；空串口读取不会延长半帧寿命。PC 按 sequence 匹配回复，忽略周期文本日志。

| 命令 | 值 | 输入 / 输出 |
| --- | --- | --- |
| INFO | 0x01 | 应用输出 5 个 u32；Bootloader 输出 10 个 u32，具体字段见 `Client.info()` |
| BEGIN | 0x10 | 长度、CRC、版本，各 u32；只在 Bootloader |
| DATA | 0x11 | offset:u32 + 最多 252 B；返回 received:u32 |
| COMMIT / ABORT / RUN | 0x12 / 0x13 / 0x14 | 无输入；只在 Bootloader |
| STATUS | 0x20 | 14 个 u32 + 5 个 float32，见 `Client.status()` |
| GET_CONFIG / SET_CONFIG | 0x21 / 0x22 | 固定 32 B：2 个 u32、5 个 float32、1 个 u32 |
| MONITOR | 0x23 | task_id:u8；返回 15 个 u32，64 位 CPU 时间由低/高两字合成 |
| STOP | 0x24 | 无输入；应用立即停机 |
| BOOT | 0x30 | 无输入；仅升级布局应用在安全停机后支持 |

## 验证结果与待实测

- 新增 C 主机组 1156 项断言通过，总计 43280 项 C 检查通过。
- Python 客户端 15 个测试通过：噪声/分片/CRC、半帧超时恢复、包校验、重复请求、设置序列化、上传及应用进入 Bootloader。
- 内存 Flash 故障注入覆盖上传 word、暂存提交尾部、安装 word、应用提交尾部、安装状态、应用擦除中断及多字节半写。
- Debug 普通应用、Upgrade 应用和 Bootloader 均构建通过；验证升级应用初始向量位于新地址，`.hup` 与首次镜像可生成。
- 桌面代码可导入，串口依赖由 requirements 提供；尚未进行真实设备连接或桌面窗口视觉验收。

待上板：占空比爬升波形、转速反馈启动宽限、CPU/栈/堆观测、USART1 过载/错误恢复、首次分区迁移、升级中断供电测试、向量跳转、IWDG 擦除时间、长时间运行。
