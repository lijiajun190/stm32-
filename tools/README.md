# 开发与 PC 工具

所有示例从仓库根目录执行。

| 文件 | 用途 |
| --- | --- |
| `build_firmware.ps1` | 配置和构建 `firmware/` 下的 CMake 工程 |
| `hood_tool.py` | 串口 CLI、升级包生成及 GUI 入口 |
| `hood_gui.py` | Tkinter 桌面界面 |
| `config-example.json` | 可提交的配置示例，实际参数需上板整定 |
| `requirements.txt` | Python 串口依赖 |

```powershell
./tools/build_firmware.ps1 -Preset Debug
python -m pip install -r tools/requirements.txt
python tools/hood_tool.py gui
python tools/hood_tool.py --port COM3 info
python tools/hood_tool.py --port COM3 status
python tools/hood_tool.py --port COM3 config
python tools/hood_tool.py --port COM3 monitor
python tools/hood_tool.py --port COM3 stop
python tools/hood_tool.py --port COM3 set-config tools/config-example.json
```

生成升级包和首次烧录镜像：

```powershell
./tools/build_firmware.ps1 -Preset Upgrade
python tools/hood_tool.py pack firmware/build/Upgrade/stm32-2.bin firmware/build/Upgrade/hood-v1.hup --version 1 --installed firmware/build/Upgrade/application-initial.bin
python tools/hood_tool.py --port COM3 flash firmware/build/Upgrade/hood-v1.hup
```

`flash` 要求已安装配套 Bootloader，首次烧录见 [构建指南](../docs/BUILDING.md)。USART1 为 115200、8N1；运行 PC 客户端前关闭其他占用该串口的程序。

CLI 不提供直接启动风机命令。配置提交后以设备返回的 `config_saved=1` 确认落盘。完整协议与工具限制见 [升级验收](../docs/validation/SMOOTHING_MONITOR_UPGRADE_VALIDATION.md)。
