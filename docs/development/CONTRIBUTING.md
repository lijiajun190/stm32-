# 开发约定

当前目录以 [结构说明](../PROJECT_STRUCTURE.md) 为准，早期目录规范已移入 `docs/archive/`。

- 新功能放在对应层：业务逻辑进 `firmware/Application/`，设备驱动进 `firmware/Components/`，外设封装进 `firmware/BSP/`。
- 每种设备保留一份正式驱动；先搜索同名实现再创建文件，避免恢复多套 `App/Components`。
- 修改前说明位置和理由；只做必要改动，保留中文解释注释与英文标识符。
- 新测试放在 `tests/`，PC 工具放在 `tools/`；不要把独立测试通过源文件通配符混入正式固件。
- 修改源码目录时同步检查 CMake include/source 路径、主机测试脚本和文档示例。
- 保留第三方版权和许可文件；CubeMX 再生成后复核自定义 hooks 与构建配置。

提交改动前按影响范围运行固件构建、C 主机测试或 Python 测试。需要真实硬件的结果只记录实测值，附固件版本、配置和原始日志。
