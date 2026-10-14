# 项目结构与迁移对照

仓库根目录负责项目介绍；固件、PC 工具、测试和文档分别维护。CubeMX 生成目录和第三方库内部结构保留，避免破坏生成工具和许可证归属。

| 原位置 | 新位置 |
| --- | --- |
| `stm32-2/` | `firmware/` |
| `stm32-2/App/Components/` | `firmware/Components/` |
| `stm32-2/Tools/` | `tools/` |
| `stm32-2/Tests/` | `tests/` |
| `额外md/` | `docs/validation/` |
| `开发流程.md` | `docs/development/DEVELOPMENT_PLAN.md` |
| `下一步开发计划.md` | `docs/development/ROADMAP.md` |
| `引脚分配表.md` | `docs/hardware/PINOUT.md` |
| `stm32-2/BUILD_GUIDE.md`、`开发规范.md` | `docs/archive/` |
| 显示驱动旁的 README | `docs/components/OLED.md` |
| `OLED_IMPLEMENTATION.md` | `docs/archive/OLED_IMPLEMENTATION.md` |
| 内层 `stm32-2/.../output_test.*`、`oled_test.c` | `tests/hardware/legacy/` |
| 三份早期 `.sh` 脚本 | `docs/archive/scripts/`，仅供历史查阅 |

## 固件分层

- `Application/`：FreeRTOS 任务、业务状态、转速控制、配置保存、交互和运行监测。
- `Algorithm/`：PID 与滤波等算法。
- `Components/`：Display、Sensors、Actuators、Input、Output 设备驱动。
- `BSP/`：ADC、GPIO、PWM、测速、I2C、UART、Flash 等板级封装。
- `Protocol/`、`Utils/`：串口帧和 CRC 等应用/Bootloader 公共模块。
- `Bootloader/`：独立链接的升级程序，使用自己的链接脚本。
- `Core/`、`Drivers/`、`Middlewares/`：CubeMX 生成代码、HAL/CMSIS、FreeRTOS。

## 本地生成内容

构建输出统一位于 `firmware/build/`，主机 C 测试位于其中的 `host-tests/`。Python 缓存、IDE 本机状态、虚拟环境和 `.local/` 由根目录 `.gitignore` 排除。

此次整理保留旧构建产物于 `.local/build-before-layout/`，旧 VS Code 配置于 `.local/vscode-before-layout/`；新构建从干净目录配置。`.local/source-hashes-before-layout.json` 用于本次迁移的源码完整性校验，不作为公开项目文件。

`stm32-2.ioc` 和固件产物名称保留，避免目录整理同时引入 CubeMX 工程重命名。不要仅凭文件名再次创建一层 `stm32-2/`。
