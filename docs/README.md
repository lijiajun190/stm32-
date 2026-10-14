# 文档导航

新读者建议依次阅读项目 [README](../README.md)、[构建与烧录](BUILDING.md)、[引脚分配](hardware/PINOUT.md) 和最新验收记录。

| 目录 / 文档 | 内容 |
| --- | --- |
| [BUILDING.md](BUILDING.md) | 环境、构建布局、首次烧录及升级包生成 |
| [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) | 当前目录职责与旧路径迁移对照 |
| [目录整理验证](validation/PROJECT_LAYOUT_VALIDATION.md) | 本次迁移的源码校验、构建与测试结果 |
| [hardware/PINOUT.md](hardware/PINOUT.md) | 引脚及接线参考，以实际硬件和 `.ioc` 为准 |
| [development/CONTRIBUTING.md](development/CONTRIBUTING.md) | 当前代码分层和开发约定 |
| [development/DEVELOPMENT_PLAN.md](development/DEVELOPMENT_PLAN.md) | 原始开发路线和验收目标 |
| [development/ROADMAP.md](development/ROADMAP.md) | 进度记录和待上板工作 |
| [validation/](validation/) | 各阶段测试记录，历史数字对应各阶段版本 |
| [components/OLED.md](components/OLED.md) | OLED 接口及早期使用示例 |
| [archive/](archive/README.md) | 早期指南、实现说明和已停用脚本 |

当前软件行为优先参考源码和后期验收记录：

1. [PWM、运行监测、PC 工具与升级](validation/SMOOTHING_MONITOR_UPGRADE_VALIDATION.md)
2. [完整模式与人机交互](validation/COMPLETE_MODES_UI_VALIDATION.md)
3. [参数保存、心跳及看门狗](validation/STAGE_7_8_VALIDATION.md)

早期文档中的“未实现”、性能估算和接线示例不代表当前状态或真实硬件验收结果。
