# visionOS 宿主 Spike 测试矩阵

状态只能填写 `PASS`、`FAIL`、`BLOCKED` 或 `NOT RUN`。Windows 合同模拟、Apple Simulator 和 Vision Pro 真机属于三个独立门禁。

| ID | 宿主 | 测试内容 | 预期结果 | 状态 | 证据/备注 |
| --- | --- | --- | --- | --- | --- |
| VSP-WIN-001 | Windows x64 | 构建 `scripts\build-visionos-spike-windows.cmd` | MSVC `/W4 /WX` 构建成功 | PASS | MSVC 19.41.34120；见 `2026-08-22-windows.md` |
| VSP-WIN-002 | Windows x64 | 运行 Native Bridge CTest | 所有权与 DisplayList 测试全部通过 | PASS | CTest 1/1 |
| VSP-WIN-003 | Windows x64 | 启动模拟器 | 窗口打开并报告 ABI v1 / FWDL v1 | PASS | 5 秒响应性烟测 |
| VSP-WIN-004 | Windows x64 | 将不透明度从 100% 调到 0% | Surface 从完全不透明变为完全透明 | PASS | 2026-08-22 用户手工确认 |
| VSP-WIN-005 | Windows x64 | 调整模拟距离 | Surface 缩放并保持 16:9 | PASS | 2026-08-22 用户手工确认 |
| VSP-WIN-006 | Windows x64 | 切换二维 fallback | 状态改变，内容持续可见 | PASS | 2026-08-22 用户手工确认 |
| VSP-SIM-001 | macOS | 生成 Xcode 工程 | XcodeGen 无警告完成 | PASS | XcodeGen 2.46.0；见 `2026-08-22-macos.md` |
| VSP-SIM-002 | visionOS Simulator | 构建并运行单元测试 | Swift/C Bridge 与 Decoder 测试通过 | PASS | xrsimulator 构建成功；XCTest 2/2 |
| VSP-SIM-003 | visionOS Simulator | 打开二维窗口 | Placeholder 正确显示，缩放不裁切 | PASS | 2026-08-22 用户确认缩放后保持 16:9 且不裁切 |
| VSP-SIM-004 | visionOS Simulator | 选择 `Open volume` | 打开包含相同内容的体积窗口 | PASS | 修复多场景声明后创建第二场景；相同 Surface/Slider 内容可见 |
| VSP-SIM-005 | visionOS Simulator | 注视与间接捏合 | 焦点和 Slider 交互符合系统行为 | PASS | 2026-08-22 用户确认焦点高亮、间接点击和 Slider 交互正常 |
| VSP-DEV-001 | Vision Pro | 安装已签名构建 | 应用启动并使用静态链接 Bridge | NOT RUN | |
| VSP-DEV-002 | Vision Pro | 在舒适距离阅读 | 文本和控件清晰、无明显不适 | NOT RUN | |
| VSP-DEV-003 | Vision Pro | VoiceOver 导航 | Surface 标签和控件按顺序播报 | NOT RUN | |
| VSP-DEV-004 | Vision Pro | 连续运行 30 分钟 | 无持续掉帧、内存增长或温度警告 | NOT RUN | |

## 门禁规则

`VSP-WIN-001..006`、`VSP-SIM-001..005` 和 `VSP-DEV-001..003` 全部通过后，架构 Spike 才能接受。提升为“受支持宿主”之前还必须通过 30 分钟真机稳定性测试。
