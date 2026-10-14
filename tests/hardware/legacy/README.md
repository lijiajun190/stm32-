# 历史硬件测试示例

保留原内层 `stm32-2/stm32-2/` 中的 `output_test.c/.h` 和原显示目录中的 `oled_test.c`，供学习与历史查阅。

这些示例不纳入生产固件的 CMake 源文件收集，也不纳入主机自动测试。包含阻塞延时及早期 GPIO 有效电平假设；使用前必须根据当前 `firmware/Core/Inc/main.h`、设备驱动接口和真实接线复核。不要在正式 FreeRTOS 控制任务中直接调用。
