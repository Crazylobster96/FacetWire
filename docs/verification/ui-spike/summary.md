# Playground UI Spike 验证汇总 / Verification Summary

> 初始基线：2026-08-21，Windows x64
> 最近更新：2026-08-24，补充 Ubuntu Linux x64 结果
> 决策：ADR-0001 保持**有条件接受（conditionally accepted）**

## 结果 / Results

| 范围 | 结果 | 证据 |
| --- | ---: | --- |
| FacetWire 原生基线 | PASS | CTest 2/2 通过 |
| Playground UI Spike Native Bridge | PASS | Windows MSVC 与 Ubuntu GCC Release CTest 均通过 |
| 原生无效输入及输出清理 | PASS | `facetwire_ui_spike_test` 契约断言 |
| 确定性 Display List batch v1 | PASS | 字节级重复结果断言 |
| 不透明度边界 0.0 / 1.0 | PASS | 编码 alpha 断言 |
| 原生所有权及重复释放 | PASS | 重复释放与 1,000 次渲染/释放循环 |
| Semantics 数据 | PASS | role 与 label 断言；Linux 人工语义信息检查通过 |
| Linux Flutter analyze/test | PASS | analyze 无问题，测试 3/3 通过 |
| Linux Flutter Release 构建 | PASS | Ubuntu x64 Release bundle 生成并验证可执行 |
| Linux Playground 人工交互 | PASS | 启动、占位渲染、缩放/透明度、语义信息、控件交互均通过 |
| Windows、macOS、iOS、Android、visionOS | 用户报告 PASS | 见平台结果记录和各平台专项记录 |
| 完整无障碍、性能与长期稳定性门 | NOT RUN | 需要独立测试证据，不从功能验证结果推导 |

## 已证明的内容 / What This Proves

C 边界能够返回有版本、有限界且确定性的 Display List batch，并通过独立 Semantics
数据传递信息，同时保持清晰的分配器所有权。Dart 解码器、可注入运行时端口、
`CustomPainter` 消费端及单元/组件测试已在 Flutter Playground 中联通。Ubuntu Linux
x64 已完成原生桥接、Flutter 自动测试、Release 构建和基础人工交互验证。

## 尚未证明的内容 / Remaining Boundaries

当前结果不等同于 worker isolate 性能、跨平台 golden 像素一致性、所有 Linux 桌面
环境兼容性、物理设备 GPU 性能或完整 WCAG/屏幕阅读器认证。ADR-0001 因此仍保持
“有条件接受”，上述项目继续作为生产化验收门。

## 复现入口 / Reproduction

- Windows 原生门：运行 `scripts/build-visionos-spike-windows.cmd`。
- Linux 详细命令、结果与代理注意事项：见 `2026-08-24-linux.md`。
- 完整人工/自动测试清单：见 `FacetWire-UI-Spike-Test-Matrix.xlsx`。

## 一致性检查 / Consistency Check

- 平台功能 PASS 与正式无障碍/性能认证严格区分。
- Linux 原生桥接、Flutter 自动测试、Release 构建和人工检查分别保留证据。
- Release 契约测试保留断言，避免 `NDEBUG` 造成测试假通过。
- 平台汇总引用专项记录，不复制或扩大专项记录的结论边界。
