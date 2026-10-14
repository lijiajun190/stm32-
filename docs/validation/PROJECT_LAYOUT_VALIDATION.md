# 目录整理验证记录

日期：2026-10-14。迁移对照见 [项目结构](../PROJECT_STRUCTURE.md)。

本次将固件、工具、测试、文档分目录存放，合并 `App/Components` 层级，更新脚本路径与文档入口。早期独立硬件测试移到 `tests/hardware/legacy/`，不再由生产固件的源文件通配符收集。未修改 C/H 业务逻辑，未执行 Git 提交或远程发布。

## 验证结果

| 项目 | 结果 |
| --- | --- |
| 源文件完整性 | 对迁移前 1,433 份 `.c/.h/.s/.ld` 文件逐一校验 SHA256，全部保留且内容一致 |
| C 主机测试 | 43,280 项检查通过 |
| Python 客户端测试 | 15 个测试通过 |
| Debug 构建 | 应用及独立 Bootloader 从新路径配置、构建通过 |
| Upgrade 构建 | 应用及独立 Bootloader 从新路径配置、构建通过 |
| 升级包生成 | 新路径 CLI 成功生成 `.hup` 与首次烧录镜像，应用长度 97,704 字节 |
| PC 工具入口 | CLI 帮助及 GUI 模块导入通过 |
| Markdown 本地链接 | 整理后的文档、根 README、工具及测试说明未发现缺失目标 |

本次在新建 `firmware/build/` 下构建，没有复用旧目录缓存。旧产物保留于本地 `.local/build-before-layout/`。

## 使用入口

仓库根目录执行：

```powershell
./tools/build_firmware.ps1 -Preset Debug
./tools/build_firmware.ps1 -Preset Upgrade
./tests/run_host_tests.ps1
python -m unittest discover -s tests -p "test_*.py" -v
```

以上为软件结构迁移验证，不新增真实硬件验收结论。串口实机、GUI 窗口显示、风机闭环、升级断电及长期运行仍按原验收计划进行。
