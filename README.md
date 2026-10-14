# STM32 智能油烟机控制系统

基于 **STM32F411RET6 + FreeRTOS** 的油烟机控制项目，包含电机转速闭环、多传感器采集、OLED 交互、故障保护、参数保存和串口固件升级。

**当前状态：开发版。** 核心软件功能已实现，主机测试与固件构建已通过；

## 功能

- 20 kHz PWM、光电测速、PID 转速控制、输出限幅与变化率限制。
- 手动、自动、限时爆炒、停机清洁维护、待机睡眠及余烟延时关机。
- MQ135 电压趋势、风速、SHT30 温湿度、BMP280 气压采集及滤波。
- 按键、旋转编码器、OLED 多页面、LED 和非阻塞蜂鸣器。
- 堵转、超速、过温、测速及传感器异常保护，任务心跳和独立看门狗。
- 双 Bank Flash 参数/最近故障保存，CPU、栈、堆、任务周期和抖动监测。
- 独立 Bootloader、暂存镜像及安装中断恢复，Python 串口 CLI 和桌面工具。

MQ135 尚不提供经过标定的 ppm；清洁模式为停机维护；睡眠仅关闭显示。升级使用 CRC32 校验，尚未实现签名认证或新版本运行失败后的自动回滚。

## 目录结构

```text
stm32-youyanji/
├── README.md                 # 项目介绍与使用入口
├── firmware/                 # STM32 固件及 CubeMX 工程
│   ├── Application/          # 任务、状态机、控制、UI、配置与监测
│   ├── Algorithm/            # PID、滤波等算法
│   ├── Components/           # 显示、传感器、执行器、输入和输出驱动
│   ├── BSP/                  # 板级外设封装
│   ├── Bootloader/           # 独立引导与升级引擎
│   ├── Protocol/             # 串口协议
│   ├── Utils/                # CRC、调试等公共模块
│   ├── Core/                 # CubeMX 生成的初始化和中断代码
│   ├── Drivers/              # ST HAL、CMSIS 及原始许可文件
│   ├── Middlewares/          # FreeRTOS 等第三方代码
│   ├── cmake/                # ARM 工具链和 CubeMX 构建配置
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   └── stm32-2.ioc            # 保留原 CubeMX 工程名称
├── tools/                    # 构建脚本、PC CLI/GUI、配置示例与依赖
├── tests/                    # C/Python 主机测试、HAL 替身及历史硬件示例
├── docs/                     # 构建、硬件、开发计划和阶段验收文档
├── .vscode/                  # 从仓库根目录打开的编辑器配置
└── .gitignore
```

`firmware/build/` 为生成目录；`.local/` 保存整理前的构建缓存与本地备份，均不属于公开项目内容。目录迁移对照见 [结构说明](docs/PROJECT_STRUCTURE.md)。

## 快速开始

以下命令均从仓库根目录执行。需要 CMake 3.22+、Ninja、Arm GNU Toolchain；Windows 脚本也可自动查找已安装的 STM32Cube 工具。

```powershell
./tools/build_firmware.ps1 -Preset Debug
./tools/build_firmware.ps1 -Preset Upgrade
```

产物在 `firmware/build/Debug/` 或 `firmware/build/Upgrade/`。固件文件保留 `stm32-2.elf` / `stm32-2.bin` 名称。两种布局的下载地址不同，首次烧录前请阅读 [构建与烧录](docs/BUILDING.md)。

PC 工具需要 Python 3.10+，GUI 另外需要 Tkinter：

```powershell
python -m pip install -r tools/requirements.txt
python tools/hood_tool.py gui
python tools/hood_tool.py --port COM3 status
```

USART1 使用 115200、8N1、3.3 V TTL。接线见 [引脚分配](docs/hardware/PINOUT.md)，完整命令见 [工具说明](tools/README.md)。

## 测试与文档

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
./tests/run_host_tests.ps1
```

C 主机脚本当前使用 LCC64，可通过 `-LccRoot` 指定安装目录；详细环境限制见 [测试说明](tests/README.md)。主机模拟测试不能替代真实硬件验收。

- [文档导航](docs/README.md)
- [开发约定](docs/development/CONTRIBUTING.md)
- [开发计划与历史进度](docs/development/ROADMAP.md)
- [升级、协议及运行监测验收](docs/validation/SMOOTHING_MONITOR_UPGRADE_VALIDATION.md)

## 许可状态

项目自有代码的许可证尚待确定；当前不宣称已经获得完整开源授权。第三方代码保留其原始版权及许可文件，主要位于 `firmware/Drivers/` 和 `firmware/Middlewares/`。正式发布前还需确定项目许可证并核实字库等资源来源。
