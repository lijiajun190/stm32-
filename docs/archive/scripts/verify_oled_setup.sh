#!/bin/bash

echo "========================================="
echo "OLED驱动配置验证脚本"
echo "========================================="

# 切换到项目目录
cd "$(dirname "$0")"

echo ""
echo "【1/5】检查驱动文件是否存在..."
if [ -f "App/Components/Display/oled.c" ]; then
    echo "  ✅ oled.c 存在 ($(wc -l < App/Components/Display/oled.c) 行)"
else
    echo "  ❌ oled.c 不存在！"
    exit 1
fi

if [ -f "App/Components/Display/oled.h" ]; then
    echo "  ✅ oled.h 存在"
else
    echo "  ❌ oled.h 不存在！"
    exit 1
fi

echo ""
echo "【2/5】检查CMakeLists.txt配置..."
if grep -q "APP_COMPONENT_SOURCES" CMakeLists.txt; then
    echo "  ✅ 已配置 APP_COMPONENT_SOURCES"
else
    echo "  ❌ 未配置 APP_COMPONENT_SOURCES"
    exit 1
fi

if grep -q "App/Components/Display" CMakeLists.txt; then
    echo "  ✅ 已包含 App/Components/Display 头文件路径"
else
    echo "  ⚠️  未包含 App/Components/Display 头文件路径（可能导致编译失败）"
fi

echo ""
echo "【3/5】检查main.c是否包含oled.h..."
if grep -q "#include \"oled.h\"" Core/Src/main.c; then
    echo "  ✅ main.c 已包含 oled.h"
else
    echo "  ❌ main.c 未包含 oled.h"
    exit 1
fi

echo ""
echo "【4/5】检查main.c是否调用OLED_Init..."
if grep -q "OLED_Init" Core/Src/main.c; then
    echo "  ✅ main.c 已调用 OLED_Init()"
else
    echo "  ❌ main.c 未调用 OLED_Init()"
    exit 1
fi

echo ""
echo "【5/5】检查I2C配置..."
if grep -q "hi2c2" Core/Src/i2c.c; then
    echo "  ✅ I2C2 已配置"
else
    echo "  ❌ I2C2 未配置"
    exit 1
fi

# 提取I2C速度
speed=$(grep -A5 "void MX_I2C2_Init" Core/Src/i2c.c | grep "ClockSpeed" | awk '{print $3}' | tr -d ';')
echo "  ℹ️  I2C2 速度: ${speed}Hz"

if [ "$speed" == "400000" ]; then
    echo "     (400kHz快速模式 - 如果通信不稳定，改为100000)"
elif [ "$speed" == "100000" ]; then
    echo "     (100kHz标准模式 - 稳定但慢)"
fi

echo ""
echo "========================================="
echo "✅ 所有检查通过！"
echo "========================================="
echo ""
echo "下一步："
echo "1. 清理编译缓存：rm -rf build/"
echo "2. 重新编译：cmake -S . -B build && cmake --build build"
echo "3. 烧录程序"
echo "4. 打开串口（115200波特率）查看输出"
echo ""
echo "如果OLED还是没反应，请查看 OLED_TROUBLESHOOTING.md"
