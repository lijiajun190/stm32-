# 测试说明

测试独立于固件源目录。C 主机测试编译 `firmware/` 中的正式模块，HAL 替身位于 `stubs/`；Python 测试使用模拟串口，无需连接开发板。

从仓库根目录执行：

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
./tests/run_host_tests.ps1
```

C 测试脚本当前依赖 LCC64，默认路径为开发机已有的 MATLAB 随附编译器。其他安装位置需显式传入：

```powershell
./tests/run_host_tests.ps1 -LccRoot "C:/path/to/lcc64"
```

该目录需包含 `bin/lcc64.exe`、`bin/lcclnk64.exe`、`include64/` 与 `lib64/`。目前尚未提供通用 GCC/Clang 主机测试入口。测试产物生成在 `firmware/build/host-tests/`。

| 测试 | 覆盖内容 |
| --- | --- |
| `test_upgrade_runtime.c` | 升级引擎、协议、掉电恢复、运行监测与 PWM 平滑 |
| `test_gpio_open_loop.c` | GPIO、按键、LED、开环控制 |
| `test_speed_motor.c` | 测速换算、PPR 1/2、电机输出 |
| `test_control.c` | PID、滤波、状态机与保护 |
| `test_modes_ui.c` | 模式、UI 模型与非阻塞蜂鸣器 |
| `test_system.c` | 参数日志、配置、心跳、半写恢复 |
| `test_sensors.c` | 传感器驱动及异常处理 |
| `test_hood_tool.py` | PC 协议解析、超时、打包与模拟升级 |

`hardware/legacy/` 保留早期 OLED、LED/蜂鸣器硬件示例。这些示例不参加默认构建或主机测试，使用前需复核引脚、有效电平与接口，不代表当前已通过上板验收。

各阶段结果和待测项统一存放在 [docs/validation/](../docs/validation/)。
