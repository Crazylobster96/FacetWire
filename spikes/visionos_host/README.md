# FacetWire visionOS 宿主 Spike

本 Spike 用于证明：原生 visionOS 宿主和不支持空间能力的降级宿主，可以消费相同的 FacetWire C Bridge 与 DisplayList。

## Windows 可以验证什么

Windows 程序是“空间合同模拟器”，不是 Vision Pro 模拟器。它验证：

- 复用现有 `fwui_render_placeholder` C ABI，不传递任何平台 UI 对象；
- 不透明度语义保持 `0 = 完全透明`、`1 = 完全不透明`；
- 二维 Surface 可以按模拟距离缩放；
- 不支持空间展示时仍能通过二维窗口 fallback 保留内容；
- Native Buffer 始终由分配它的库释放。

在仓库根目录执行：

```bat
scripts\build-visionos-spike-windows.cmd run
```

手工检查：

1. 调整 `Surface opacity`，确认 100% 完全不透明、0% 完全透明。
2. 调整 `Simulated distance`，确认 Surface 保持 16:9 比例缩放。
3. 切换 `Force 2D fallback`，确认状态发生变化且内容不消失。

截至 2026-08-22，上述 Windows 手工检查已由用户确认通过。

## macOS / visionOS Simulator 验证

Apple 目录保存 XcodeGen 工程定义，避免把自动生成的 Xcode 工程提交为 Spike 源文件。在安装当前 Xcode、visionOS Simulator Runtime 和 XcodeGen 的 Mac 上运行：

```sh
./scripts/validate-visionos-spike-macos.sh
```

随后在生成的工程中检查二维窗口和 `Open volume` 操作。真机签名必须使用测试者本地的 Apple Developer Team。

## 验收边界

Windows PASS 不等于 visionOS PASS。只有 Windows 合同测试、visionOS Simulator 和 Vision Pro 真机项目分别留有证据后，才能提升支持等级。

相关文档：

- `docs/adr/0002-visionos-host-strategy.md`
- `spec/spatial-surface-v0.1.md`
- `docs/verification/visionos-spike/test-matrix.md`
