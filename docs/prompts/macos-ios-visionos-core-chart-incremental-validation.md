# Mac 既有 Codex 项目提示词：增量验证 Core Chart Renderer 0.2

> 历史提示词：仅用于复现 0.2 验证。当前 0.3 请使用
> `macos-ios-visionos-chart-visualization-v0.3-incremental-validation.md`。

你正在已有 FacetWire Mac Codex 项目中工作，前序 macOS、iOS、visionOS 验证已经完成。
本次只增量验证最新 main 中的 Core Chart Renderer 0.2 与 Chart Element Layering，不重新生成或覆盖既有签名配置。

1. 先检查工作树；若存在本地改动，判断是否为已验证的 Xcode/CocoaPods/签名状态并安全
   保留。不得 reset、checkout、clean 或删除用户改动。
2. 工作树可安全合并后执行 fetch，并仅 fast-forward 到最新 main。
3. 阅读 docs/guides/core-chart-renderer-cross-platform-validation.zh-CN.md、公共
   chart_renderer.h、插件 README 和正式 Demo README。
4. 运行原生 Chart 合同、插件生命周期、Flutter analyze 和 Flutter test。
5. 在 macOS、iOS 和 visionOS 的既有工程中构建正式 examples/placeholder_demo。
6. 打开首页图表图标，执行 CH-01..CH-18；必须看到 Native PASS、Balanced PASS 和
   Transparent PASS。逐类检查全部 Chart Playground 类型，并检查 0/90/180/270°、值标签、整体 opacity 0/0.1/0.99/1，以及单个元素的透明度、平移、缩放、旋转、强调色和提升状态。
7. 不得把 Simulator 结果记为真机结果；只有实际运行 Vision Pro/iPhone 才标记真机通过。
8. 把结果写入 docs/verification/macos-ios/ 下的新日期文档，包含 commit、命令、设备、
   自动测试数量、手工检查、截图文件名、NOT RUN 项和已保留的平台配置改动。
9. 若需要提交，只提交本轮验证证据和确有必要的平台工程增量；提交前再次运行相关测试。

最终报告必须区分：代码失败、工具链/签名失败、未运行以及真实通过，不得根据编译成功
推断 UI 或 Native Asset 已通过。
