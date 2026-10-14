# 构建与烧录

本文的命令从仓库根目录执行。STM32 工程位于 `firmware/`，CubeMX 文件仍为 `firmware/stm32-2.ioc`，工程及产物名称暂时保留 `stm32-2`。

## 环境

- CMake 3.22+、Ninja、Arm GNU Toolchain（`arm-none-eabi-gcc`）。
- Windows 可使用 STM32Cube 随附工具，`tools/build_firmware.ps1` 会查找安装目录并临时设置 PATH。
- Python 3.10+ 用于 PC 工具和镜像打包，串口访问需要 `tools/requirements.txt` 中的 pyserial；GUI 需要 Tkinter。

## 构建

```powershell
./tools/build_firmware.ps1 -Preset Debug
./tools/build_firmware.ps1 -Preset Release
./tools/build_firmware.ps1 -Preset Upgrade
```

已经将工具加入 PATH 时，也可跨平台使用以下方式：

```sh
cd firmware
cmake --preset Debug
cmake --build --preset Debug --parallel 4
```

需要升级布局时将两处 `Debug` 换为 `Upgrade`。每个布局使用独立生成目录，勿复用整理前带有旧绝对路径的 CMake 缓存。

| Preset | 应用起始地址 | 用途 |
| --- | --- | --- |
| Debug / Release | `0x08000000` | 独立运行应用，参数日志位于扇区 6/7 |
| Upgrade | `0x08020000` | 与 Bootloader 配套，参数日志位于扇区 2/3 |

构建生成 `firmware/build/<Preset>/stm32-2.elf`、`stm32-2.bin` 和 `bootloader.bin`。单独构建 Bootloader 可在 `firmware/` 执行 `cmake --build --preset Upgrade --target bootloader`。

## 首次烧录

独立布局使用 ST-LINK 下载 `firmware/build/Debug/stm32-2.elf`，或将对应 `.bin` 下载到 `0x08000000`。

升级布局先生成带尾部提交信息的首次镜像和串口升级包：

```powershell
python tools/hood_tool.py pack firmware/build/Upgrade/stm32-2.bin firmware/build/Upgrade/hood-v1.hup --version 1 --installed firmware/build/Upgrade/application-initial.bin
```

使用 STM32CubeProgrammer / ST-LINK：

1. 将 `firmware/build/Upgrade/bootloader.bin` 下载到 `0x08000000`。
2. 将 `firmware/build/Upgrade/application-initial.bin` 下载到 `0x08020000`。裸 `stm32-2.bin` 没有有效镜像要求的提交尾部。
3. 从旧布局迁移前先备份配置；首次迁移需清理新日志扇区 2/3 及暂存扇区 6。旧配置不自动迁移。

也可仅首次烧录 Bootloader，再通过串口上传 `.hup` 包。完整 Flash 布局、停机条件与断电恢复边界见 [升级验收文档](validation/SMOOTHING_MONITOR_UPGRADE_VALIDATION.md)。

## CubeMX 与编辑器

在 VS Code 中打开仓库根目录；`.vscode/` 已指向 `firmware/` 和 Debug 编译数据库。STM32Cube 工程相关的 `.settings/` 保留在固件目录作为本地配置。

CubeMX 再生成前备份自定义配置，并检查 `firmware/CMakeLists.txt`、中断及 FreeRTOS hooks；不要把代码重新生成到重复的嵌套目录。正式业务入口是 `Application/`，历史硬件测试不会自动接入业务固件。
