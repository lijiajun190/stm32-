#!/bin/bash

# 按键和编码器驱动测试脚本
# 用途：编译、烧录、打开串口监视器

set -e  # 遇到错误立即退出

echo "=========================================="
echo "  按键和编码器驱动测试"
echo "=========================================="

# 步骤 1：清理并编译
echo ""
echo "[1/4] 清理旧的编译文件..."
rm -rf build
mkdir -p build
cd build

echo ""
echo "[2/4] 配置 CMake..."
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug ..

echo ""
echo "[3/4] 编译项目..."
cmake --build . -- -j$(nproc 2>/dev/null || echo 4)

if [ $? -ne 0 ]; then
    echo "❌ 编译失败！请检查错误信息。"
    exit 1
fi

echo ""
echo "✅ 编译成功！"
echo ""
echo "固件位置："
echo "  - ELF: build/stm32-2.elf"
echo "  - HEX: build/stm32-2.hex"
echo "  - BIN: build/stm32-2.bin"

# 步骤 2：烧录固件
echo ""
echo "[4/4] 准备烧录固件..."
echo ""
echo "请选择烧录方式："
echo "  1) 使用 OpenOCD 自动烧录"
echo "  2) 手动烧录（使用 STM32CubeProgrammer）"
echo "  3) 跳过烧录，仅编译"
read -p "请输入选项 [1-3]: " choice

case $choice in
    1)
        echo ""
        echo "正在使用 OpenOCD 烧录..."
        if command -v openocd &> /dev/null; then
            openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
                -c "program stm32-2.elf verify reset exit"
            echo "✅ 烧录完成！"
        else
            echo "❌ 未找到 OpenOCD，请安装或选择手动烧录。"
            exit 1
        fi
        ;;
    2)
        echo ""
        echo "请使用 STM32CubeProgrammer 烧录以下文件："
        echo "  $(pwd)/stm32-2.hex"
        echo ""
        read -p "烧录完成后按 Enter 继续..."
        ;;
    3)
        echo ""
        echo "跳过烧录。"
        exit 0
        ;;
    *)
        echo "❌ 无效选项。"
        exit 1
        ;;
esac

# 步骤 3：打开串口监视器
echo ""
echo "=========================================="
echo "  准备打开串口监视器"
echo "=========================================="
echo ""
echo "串口配置："
echo "  - 波特率: 115200"
echo "  - 数据位: 8"
echo "  - 停止位: 1"
echo "  - 校验:   无"
echo ""

# 检测串口工具
if command -v screen &> /dev/null; then
    read -p "按 Enter 打开 screen 串口监视器（退出：Ctrl+A, K）..."
    # 自动检测 Windows 的 COM 口
    COM_PORT=$(ls /dev/ttyS* 2>/dev/null | head -n 1)
    if [ -z "$COM_PORT" ]; then
        echo "❌ 未检测到串口设备，请手动指定。"
        exit 1
    fi
    screen $COM_PORT 115200
elif command -v minicom &> /dev/null; then
    read -p "按 Enter 打开 minicom 串口监视器..."
    minicom -D /dev/ttyS0 -b 115200
else
    echo "未安装串口工具，请手动打开串口监视器："
    echo "  - Windows: PuTTY / 串口助手"
    echo "  - Linux:   screen /dev/ttyUSB0 115200"
    echo "  - macOS:   screen /dev/tty.usbserial 115200"
fi

echo ""
echo "=========================================="
echo "  测试指南"
echo "=========================================="
echo ""
echo "1. 观察初始化输出："
echo "   - Button driver initialized"
echo "   - Encoder driver initialized"
echo ""
echo "2. 测试按键："
echo "   - 短按：PRESS → RELEASE → CLICK"
echo "   - 长按：PRESS → LONG_PRESS → REPEAT..."
echo ""
echo "3. 测试编码器："
echo "   - 顺时针：Delta: 1, Total 递增"
echo "   - 逆时针：Delta: -1, Total 递减"
echo ""
echo "详细测试步骤请查看 INPUT_TEST_GUIDE.md"
echo ""
