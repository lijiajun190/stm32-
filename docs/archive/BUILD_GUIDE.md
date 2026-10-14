# 🚀 快速开始指南

> 本文为早期骨架指南，保留原内容供查阅；其中的 TODO、路径和集成步骤已过时。当前操作请使用 [构建与烧录](../BUILDING.md)。

## ✅ 已完成

1. ✅ 创建了 48 个源文件（BSP、Components、Algorithm、Application、Utils）
2. ✅ 修改了 CMakeLists.txt，添加了所有源文件和包含目录

## 📝 CMakeLists.txt 修改内容

已在 CMakeLists.txt 中添加：

### 源文件收集（第 45-55 行）
```cmake
file(GLOB_RECURSE BSP_SOURCES "BSP/Src/*.c")
file(GLOB_RECURSE COMPONENT_SOURCES
    "Components/Sensors/*.c"
    "Components/Actuators/*.c"
    "Components/Display/*.c"
    "Components/Input/*.c"
)
file(GLOB_RECURSE ALGORITHM_SOURCES "Algorithm/*.c")
file(GLOB_RECURSE APPLICATION_SOURCES "Application/*.c")
file(GLOB_RECURSE UTILS_SOURCES "Utils/*.c")
```

### 添加到编译目标（第 60-64 行）
```cmake
${BSP_SOURCES}
${COMPONENT_SOURCES}
${ALGORITHM_SOURCES}
${APPLICATION_SOURCES}
${UTILS_SOURCES}
```

### 包含目录（第 70-77 行）
```cmake
BSP/Inc
Components/Sensors
Components/Actuators
Components/Display
Components/Input
Algorithm
Application
Utils
```

## 🔨 立即编译测试

在项目根目录执行：

```bash
# 配置 CMake
cmake --preset Debug

# 编译
cmake --build --preset Debug
```

## ⚠️ 可能的编译错误

### 1. 缺少 HAL 头文件引用
如果编译报错找不到 `tim.h`、`adc.h`、`i2c.h` 等文件，需要确保 CubeMX 生成了这些文件。

**解决方法：**
- 打开 `stm32-2.ioc`
- 重新生成代码（Project → Generate Code）

### 2. printf 重定向
`bsp_uart.c` 中有 `fputc` 函数重定向 printf，需要在链接器选项中添加 `-u _printf_float`（如果使用浮点打印）。

### 3. FreeRTOS 相关
确保 FreeRTOS 已正确配置：
- `FreeRTOSConfig.h` 存在
- 任务栈大小足够（建议至少 256 字）

## 📋 编译后的下一步

### 1. 完善硬件相关代码

> 2026-10-04 更新：下方 BSP GPIO 的 TODO 示例是早期框架说明，实际实现已完成。
> 当前 `main.c` 已调用 `App_Init()` 和 `Tasks_Init()` 并检查返回值；无需重复添加。
> PA0 低有效，PC2/PC3/PC4 高有效；开环固件按键映射和验收步骤见
> `Tests/STAGE_3_4_VALIDATION.md`。

在以下文件中完善 TODO：

**BSP/Src/bsp_gpio.c**
```c
void Bsp_LED_Set(Bsp_LedId_t id, bool on)
{
    // TODO: 根据实际硬件配置
    switch(id) {
        case BSP_LED_1:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, 
                             on ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        // ... 其他 LED
    }
}
```

**BSP/Src/bsp_gpio.c** - 按键读取
```c
bool Bsp_Key_Read(Bsp_KeyId_t id)
{
    // TODO: 根据按键有效电平配置
    GPIO_TypeDef *port;
    uint16_t pin;
    
    switch(id) {
        case BSP_KEY_MODE:
            port = GPIOA;
            pin = GPIO_PIN_0;
            break;
        // ... 其他按键
    }
    
    // 假设按键按下为低电平
    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}
```

### 2. 在 main.c 中初始化应用

在 `Core/Src/main.c` 中添加：

```c
/* USER CODE BEGIN Includes */
#include "app_main.h"
#include "tasks.h"
/* USER CODE END Includes */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    
    // 外设初始化（CubeMX 生成）
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_USART1_UART_Init();
    MX_TIM1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    
    /* USER CODE BEGIN 2 */
    // 应用初始化
    App_Init();
    
    // 创建 FreeRTOS 任务
    Tasks_Init();
    /* USER CODE END 2 */
    
    // 启动调度器
    osKernelStart();
    
    while (1) {
    }
}
```

### 3. 逐步测试

按照 `开发流程.md` 的阶段测试：

1. **测试 BSP 层**
   - LED 闪烁
   - 串口输出
   - 按键读取

2. **测试传感器**
   - I2C 扫描设备
   - 读取 SHT30 温湿度
   - 读取 BMP280 气压

3. **测试电机**
   - PWM 输出
   - 占空比调节

## 🎯 当前状态

✅ 代码框架完整  
✅ CMakeLists.txt 已配置  
⏳ 需要编译测试  
⏳ 需要完善硬件相关代码  
⏳ 需要在 main.c 中集成  

## 📞 遇到问题？

1. **编译错误**：检查 CubeMX 是否生成了所有外设代码
2. **链接错误**：检查是否有重复定义或缺少函数实现
3. **运行时错误**：检查栈大小、中断优先级配置

祝编译成功！🎉

