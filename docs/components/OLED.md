# OLED SSD1306 驱动说明文档

> 以下保留早期驱动接口和使用示例，性能数字不是当前硬件实测结果。当前 OLED 由 `firmware/Application/tasks.c` 集成，页面验收见 [模式与 UI](../validation/COMPLETE_MODES_UI_VALIDATION.md)。早期独立示例已移至 `tests/hardware/legacy/oled_test.c`，不要直接加入正式控制任务。

## 📌 硬件I2C vs 软件I2C 选择

### ✅ **结论：使用硬件I2C（已实现）**

---

## 🔍 详细对比

### 硬件I2C优势

| 优势 | 说明 |
|------|------|
| ✅ **性能更好** | 硬件自动生成时序，可配合DMA使用，CPU占用率低 |
| ✅ **时序准确** | 硬件保证I2C协议时序，不受中断影响 |
| ✅ **稳定可靠** | 自动处理ACK/NACK、时钟延展、总线仲裁 |
| ✅ **速度更快** | 支持100kHz/400kHz，甚至1MHz（Fast Mode+） |
| ✅ **已经配置** | CubeMX已生成所有初始化代码 |
| ✅ **代码简洁** | 直接调用HAL库函数，无需手写位操作 |

### 软件I2C劣势

| 劣势 | 说明 |
|------|------|
| ❌ **占用CPU** | GPIO位操作模拟时序，需要精确延时 |
| ❌ **受中断影响** | 时序可能被中断打断，导致通信失败 |
| ❌ **速度受限** | 一般只能跑到50-100kHz |
| ❌ **代码复杂** | 需要手写SCL/SDA翻转、ACK检测等逻辑 |
| ❌ **浪费资源** | 你的硬件I2C已配置好却不用 |

---

## 📦 当前实现方案

### 硬件配置
```
MCU:        STM32F411RET6
I2C:        I2C2 (硬件)
引脚:       PB10 (SCL) + PB3 (SDA)
速度:       400kHz (Fast Mode)
OLED:       SSD1306 128x64 四引脚
地址:       0x78 (8位) / 0x3C (7位)
```

### 连接方式
```
OLED模块    →   STM32F411
─────────────────────────
VCC         →   3.3V
GND         →   GND
SCL         →   PB10 (I2C2_SCL)
SDA         →   PB3  (I2C2_SDA)
```

**⚠️ 注意：**
- SCL和SDA都需要4.7kΩ上拉电阻到3.3V
- 大部分OLED模块已板载上拉电阻，无需外接
- 如果通信不稳定，检查是否缺少上拉电阻

---

## 📁 文件结构

```
firmware/Components/Display/
├── oled.h          # OLED驱动头文件（API声明）
└── oled.c          # OLED驱动实现（硬件I2C）
```

独立示例：`tests/hardware/legacy/oled_test.c`；本文档：`docs/components/OLED.md`。

---

## 🚀 快速上手

### 1. 在main.c中添加头文件

```c
/* USER CODE BEGIN Includes */
#include "oled.h"
/* USER CODE END Includes */
```

### 2. 在初始化部分调用OLED_Init

```c
/* USER CODE BEGIN 2 */

// 初始化OLED
OLED_Status_t status = OLED_Init(&hi2c2);
if (status == OLED_OK) {
    printf("OLED Init Success!\r\n");
    
    // 显示欢迎信息
    OLED_Clear();
    OLED_ShowString(10, 2, "STM32F411", OLED_FONT_6X8);
    OLED_ShowString(10, 3, "OLED Ready", OLED_FONT_6X8);
    OLED_Refresh();
} else {
    printf("OLED Init Failed! Error: %d\r\n", status);
}

/* USER CODE END 2 */
```

### 3. 在主循环或任务中更新显示

```c
/* USER CODE BEGIN 3 */

OLED_Clear();
OLED_ShowString(0, 0, "Counter:", OLED_FONT_6X8);
OLED_ShowNum(54, 0, counter++, 5, OLED_FONT_6X8);
OLED_Refresh();

HAL_Delay(100);

/* USER CODE END 3 */
```

---

## 📚 API函数说明

### 基础函数

| 函数 | 功能 | 参数说明 |
|------|------|----------|
| `OLED_Init()` | 初始化OLED | hi2c: I2C句柄（&hi2c2） |
| `OLED_Clear()` | 清空屏幕 | 无 |
| `OLED_Fill()` | 全屏填充 | data: 0x00=全黑, 0xFF=全白 |
| `OLED_Refresh()` | 刷新显示 | 将缓冲区内容显示到屏幕 |

### 显示函数

| 函数 | 功能 | 示例 |
|------|------|------|
| `OLED_ShowChar()` | 显示单个字符 | `OLED_ShowChar(0, 0, 'A', OLED_FONT_6X8)` |
| `OLED_ShowString()` | 显示字符串 | `OLED_ShowString(0, 0, "Hello", OLED_FONT_6X8)` |
| `OLED_ShowNum()` | 显示整数 | `OLED_ShowNum(0, 0, 12345, 5, OLED_FONT_6X8)` |
| `OLED_ShowFloat()` | 显示浮点数 | `OLED_ShowFloat(0, 0, 25.6, 2, 1, OLED_FONT_6X8)` |

### 绘图函数

| 函数 | 功能 | 示例 |
|------|------|------|
| `OLED_DrawPixel()` | 画点 | `OLED_DrawPixel(10, 10, true)` |
| `OLED_DrawLine()` | 画线 | `OLED_DrawLine(0, 0, 127, 63)` |
| `OLED_DrawRect()` | 画矩形 | `OLED_DrawRect(10, 10, 50, 30, false)` |

### 控制函数

| 函数 | 功能 | 示例 |
|------|------|------|
| `OLED_DisplayOn()` | 开关显示 | `OLED_DisplayOn(true)` |
| `OLED_SetContrast()` | 设置对比度 | `OLED_SetContrast(128)` |
| `OLED_SetColorMode()` | 设置颜色模式 | `OLED_SetColorMode(OLED_COLOR_REVERSED)` |

---

## 💡 坐标系统说明

### 字符显示坐标
```
X: 0-127 (列，像素)
Y: 0-7   (页，每页8个像素高度)

页0: 第0-7行像素
页1: 第8-15行像素
...
页7: 第56-63行像素
```

### 像素绘制坐标
```
X: 0-127 (列)
Y: 0-63  (行)

(0,0)                  (127,0)
  ┌────────────────────┐
  │                    │
  │     128 x 64       │
  │                    │
  └────────────────────┘
(0,63)                (127,63)
```

---

## 🧪 测试步骤

### 第一步：I2C设备扫描测试

在`main.c`中添加I2C扫描代码：

```c
/* USER CODE BEGIN 2 */

printf("Scanning I2C2 bus...\r\n");
for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c2, addr << 1, 1, 100) == HAL_OK) {
        printf("Found device at address: 0x%02X\r\n", addr);
    }
}
printf("Scan complete!\r\n\r\n");

/* USER CODE END 2 */
```

**预期结果：**
```
Found device at address: 0x3C
```

### 第二步：OLED初始化测试

```c
/* USER CODE BEGIN 2 */

OLED_Status_t status = OLED_Init(&hi2c2);
if (status == OLED_OK) {
    printf("OLED Init: SUCCESS\r\n");
} else {
    printf("OLED Init: FAILED (Error %d)\r\n", status);
}

/* USER CODE END 2 */
```

### 第三步：显示测试

参考`oled_test.c`中的`OLED_Test()`函数

---

## ⚠️ 常见问题

### Q1: OLED没有显示任何内容

**可能原因：**
1. I2C地址不对（尝试0x78或0x7A）
2. 接线错误（检查SCL/SDA是否接对）
3. 缺少上拉电阻
4. 供电电压不对（应为3.3V）
5. 忘记调用`OLED_Refresh()`

**解决方法：**
```c
// 1. 先扫描I2C总线，确认地址
// 2. 检查硬件连接
// 3. 确保每次显示后调用OLED_Refresh()
OLED_ShowString(0, 0, "Test", OLED_FONT_6X8);
OLED_Refresh();  // ← 必须调用！
```

### Q2: 显示花屏或乱码

**可能原因：**
1. I2C速度太快（线路太长或信号质量差）
2. 缺少上拉电阻
3. 电源不稳定

**解决方法：**
```c
// 在i2c.c中降低速度到100kHz
hi2c2.Init.ClockSpeed = 100000;  // 改回100kHz
```

### Q3: 通信超时（HAL_TIMEOUT）

**可能原因：**
1. 硬件未连接
2. I2C总线被拉低（短路）
3. 上拉电阻过大或没有

**解决方法：**
1. 用万用表测量SCL/SDA电压（应为3.3V）
2. 检查是否有短路
3. 添加4.7kΩ上拉电阻

### Q4: 如何提高刷新速度？

**方法：**
1. 已经使用400kHz（当前配置）
2. 使用局部刷新而不是全屏刷新
3. 减少不必要的`OLED_Refresh()`调用
4. 使用DMA传输（高级优化）

---

## 🔧 调试技巧

### 1. 启用调试输出

在`oled.c`中添加调试信息：

```c
#define OLED_DEBUG  1

#if OLED_DEBUG
    #define OLED_LOG(fmt, ...) printf("[OLED] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define OLED_LOG(fmt, ...)
#endif

// 在关键位置添加日志
OLED_LOG("Init start");
OLED_LOG("I2C write status: %d", status);
```

### 2. 使用逻辑分析仪

抓取I2C波形，检查：
- 起始位和停止位是否正确
- ACK/NACK是否正常
- 时序是否符合标准

### 3. 测试单个功能

逐个测试驱动功能：
```c
// 只测试初始化
OLED_Init(&hi2c2);

// 只测试清屏
OLED_Clear();

// 只测试显示一个字符
OLED_ShowChar(0, 0, 'A', OLED_FONT_6X8);
OLED_Refresh();
```

---

## 📊 性能指标

| 指标 | 数值 |
|------|------|
| I2C速度 | 400kHz |
| 全屏刷新时间 | ~20ms |
| 初始化时间 | ~100ms |
| 单字符显示 | <1ms |
| 内存占用（GRAM） | 1024字节 |
| 代码大小 | ~3KB |

---

## 📝 下一步计划

1. ✅ 完成OLED基础驱动
2. ⏳ 测试OLED显示功能
3. ⏳ 创建UI管理模块
4. ⏳ 实现多页面切换
5. ⏳ 添加中文字体支持（可选）

---

## 📖 参考资料

- [SSD1306数据手册](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [STM32 HAL I2C驱动文档](https://www.st.com/resource/en/user_manual/dm00105879-description-of-stm32f4-hal-and-ll-drivers-stmicroelectronics.pdf)
- [I2C协议说明](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)

---

**文档版本：** v1.0  
**最后更新：** 2026-09-12  
**维护者：** STM32油烟机项目组
