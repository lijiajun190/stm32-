# STM32项目开发规范

> 历史规范与问题复盘，已停止作为当前目录约定。本文中的双 Components 目录、Git 操作和进度描述保留为历史记录；当前规范见 [开发约定](../development/CONTRIBUTING.md)。

## 📁 目录结构规范

### ✅ 正确的目录结构

```
stm32-2/
├── App/                          # 应用层代码（新开发的所有代码放这里）
│   └── Components/              # 硬件组件驱动（阶段三开发重点）
│       ├── Display/             # 显示设备
│       │   ├── oled.c          # OLED驱动实现
│       │   ├── oled.h          # OLED驱动接口
│       │   └── oled_test.c     # OLED测试程序
│       ├── Sensors/             # 传感器驱动
│       ├── Actuators/           # 执行器驱动
│       └── Input/               # 输入设备驱动
├── Components/                  # ⚠️ 预留目录（暂时保留空框架）
│   ├── Sensors/                # 空
│   ├── Actuators/              # 空
│   └── Input/                  # 空
├── BSP/                         # 板级支持包
├── Algorithm/                   # 算法层
├── Application/                 # 应用逻辑层
└── Utils/                       # 工具函数
```

### ❌ 常见错误

1. **同一功能在两个目录都有实现**
   - ❌ `Components/Display/oled.c` + `App/Components/Display/oled.c`
   - ✅ 只在 `App/Components/Display/oled.c` 实现

2. **API接口不一致**
   - ❌ 旧版：`OLED_Init(void)`，新版：`OLED_Init(I2C_HandleTypeDef *hi2c)`
   - ✅ 统一使用HAL库标准接口

3. **CMakeLists.txt配置遗漏**
   - ❌ 只包含 `Components/Display/*.c`
   - ✅ 同时包含 `App/Components/*/*.c`

---

## 🔧 开发流程规范

### 阶段三：独立硬件驱动开发

#### 1. 创建新驱动前的检查清单
```bash
# ✅ 检查是否已有旧实现
find . -name "你要创建的模块名.c" -o -name "你要创建的模块名.h"

# ✅ 确认目录结构
# 所有新驱动必须放在 App/Components/ 下对应子目录
```

#### 2. 标准驱动开发步骤

**每个驱动必须包含：**
```
App/Components/[类别]/
├── xxx.h              # 驱动接口头文件
├── xxx.c              # 驱动实现
├── xxx_test.c         # 独立测试程序
└── README.md          # 驱动说明文档
```

**驱动接口规范：**
```c
// 所有驱动必须提供的标准接口
bool XXX_Init(外设句柄);          // 初始化
bool XXX_DeInit(void);             // 反初始化
ErrorCode XXX_Read/Write(...);     // 读写操作
ErrorCode XXX_GetStatus(...);      // 状态查询
```

#### 3. 开发顺序（严格按照）

按照 `项目工作流程.md` 中的顺序逐个完成：
1. ✅ **OLED** - 已完成
2. ⬜ **按键和编码器** - 进行中
3. ⬜ **电机PWM**
4. ⬜ **光电测速**
5. ⬜ **ADC传感器**
6. ⬜ **SHT30**
7. ⬜ **BMP280**
8. ⬜ **蜂鸣器和LED**

**每完成一个驱动：**
```bash
# 1. 编写驱动代码
# 2. 编写测试程序
# 3. 在main.c中调用测试
# 4. 确认功能正常
git add App/Components/[类别]/
git commit -m "feat: add [模块名] driver"
# 5. 再开始下一个驱动
```

---

## 🚨 本次问题复盘

### 问题描述
工程中存在两套OLED驱动，导致API不一致和编译冲突：
- 旧驱动：`Components/Display/oled.c` (129行，不完整)
- 新驱动：`App/Components/Display/oled.c` (509行，完整)

### 根本原因
1. CMakeLists.txt只配置了 `Components/Display/*.c`
2. 没有配置 `App/Components/*/*.c`
3. 旧驱动未及时清理

### 解决措施
1. ✅ 删除旧驱动 `Components/Display/`
2. ✅ 修改CMakeLists.txt，添加：
   ```cmake
   file(GLOB_RECURSE APP_COMPONENT_SOURCES "App/Components/*/*.c")
   target_sources(... ${APP_COMPONENT_SOURCES} ...)
   target_include_directories(... App/Components/Display ...)
   ```

### 预防措施
- [ ] 每次创建新驱动前，先搜索是否存在同名文件
- [ ] 驱动只在 `App/Components/` 下开发，不在 `Components/` 下创建
- [ ] 定期检查 `Components/` 目录是否为空框架

---

## 📝 CMakeLists.txt配置规范

### 正确配置示例

```cmake
# 用户源文件收集
file(GLOB_RECURSE BSP_SOURCES "BSP/Src/*.c")
file(GLOB_RECURSE COMPONENT_SOURCES
    "Components/Sensors/*.c"
    "Components/Actuators/*.c"
    "Components/Input/*.c"
)
file(GLOB_RECURSE APP_COMPONENT_SOURCES "App/Components/*/*.c")  # ← 新驱动在这里
file(GLOB_RECURSE ALGORITHM_SOURCES "Algorithm/*.c")
file(GLOB_RECURSE APPLICATION_SOURCES "Application/*.c")
file(GLOB_RECURSE UTILS_SOURCES "Utils/*.c")

# 添加到可执行文件
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${BSP_SOURCES}
    ${COMPONENT_SOURCES}
    ${APP_COMPONENT_SOURCES}              # ← 必须包含
    ${ALGORITHM_SOURCES}
    ${APPLICATION_SOURCES}
    ${UTILS_SOURCES}
)

# 头文件包含路径
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/Inc
    Components/Sensors
    Components/Actuators
    Components/Input
    App/Components/Display              # ← 新驱动头文件路径
    App/Components/Sensors
    App/Components/Actuators
    App/Components/Input
    Algorithm
    Application
    Utils
)
```

---

## ✅ 验收标准

每个驱动开发完成后必须通过：

1. **编译检查**
   ```bash
   cmake --build build/Debug
   # 无警告，无错误
   ```

2. **功能测试**
   - 独立测试程序能正常运行
   - 串口输出测试结果
   - 硬件功能符合预期

3. **代码审查**
   - 接口符合规范
   - 有超时保护
   - 有错误处理
   - 有必要的注释

4. **文档检查**
   - README.md 说明清晰
   - API接口有文档
   - 使用示例完整

---

## 📌 快速参考

### 创建新驱动检查清单
- [ ] 确认没有旧实现：`find . -name "模块名.*"`
- [ ] 在 `App/Components/[类别]/` 下创建
- [ ] 编写 `.h` `.c` `_test.c` 三个文件
- [ ] 创建 `README.md` 说明文档
- [ ] 在 `CMakeLists.txt` 中确认路径包含
- [ ] 编译测试通过
- [ ] Git提交 `feat: add [模块名] driver`

### 遇到冲突时处理流程
1. 用 `find` 查找所有同名文件
2. 比较文件内容和API接口
3. 保留完整的实现（通常在 `App/` 下）
4. 删除冗余实现
5. 更新 `CMakeLists.txt`
6. 清理build目录重新编译

---

**最后更新：** 2026-09-12  
**维护者：** 项目开发团队
