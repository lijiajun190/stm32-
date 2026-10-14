#!/bin/bash
# OLED驱动测试 - 快速编译脚本

echo "========================================"
echo "STM32F411 OLED驱动测试 - 编译脚本"
echo "========================================"
echo ""

# 设置项目路径
PROJECT_DIR="D:/stm32/stm32-youyanji/stm32-2"
cd "$PROJECT_DIR" || exit 1

echo "[1/4] 清理旧的构建文件..."
rm -rf build/
echo "✓ 清理完成"
echo ""

echo "[2/4] 配置CMake..."
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    echo "✗ CMake配置失败！"
    exit 1
fi
echo "✓ CMake配置成功"
echo ""

echo "[3/4] 编译项目..."
cmake --build build
if [ $? -ne 0 ]; then
    echo "✗ 编译失败！"
    exit 1
fi
echo "✓ 编译成功"
echo ""

echo "[4/4] 检查输出文件..."
if [ -f "build/stm32-2.elf" ]; then
    echo "✓ 固件文件已生成: build/stm32-2.elf"
    ls -lh build/stm32-2.elf
else
    echo "✗ 固件文件不存在！"
    exit 1
fi

echo ""
echo "========================================"
echo "编译完成！请烧录 build/stm32-2.elf"
echo "========================================"
echo ""
echo "下一步："
echo "1. 使用STM32CubeProgrammer烧录固件"
echo "2. 打开串口工具（波特率115200）"
echo "3. 复位开发板，观察串口输出"
echo "4. 检查OLED显示效果"
echo ""
