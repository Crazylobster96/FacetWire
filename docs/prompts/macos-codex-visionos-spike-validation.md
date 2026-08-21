# macOS Codex 提示词：验证 FacetWire visionOS Spike

在安装了当前 Xcode 和 visionOS Simulator Runtime 的 Mac 上拉取仓库后，将下面整段提示词交给 Codex：

```text
请在 FacetWire 仓库中完成 visionOS Host Spike 验证。不要重新设计 Core ABI。

开始前完整阅读：
- docs/adr/0002-visionos-host-strategy.md
- spec/spatial-surface-v0.1.md
- spikes/visionos_host/README.md
- docs/verification/visionos-spike/test-matrix.md

先检查 git status，保留所有无关改动。如果缺少 XcodeGen，只有在我批准后才能安装。
运行：

./scripts/validate-visionos-spike-macos.sh

如果默认 Simulator 名称不可用，运行 xcrun simctl list devices 查找已安装的
visionOS Simulator，然后通过 FW_VISIONOS_DESTINATION 指定正确 destination 后重试。

只修复可以稳定复现的 Spike 缺陷；不得降低测试要求，也不得把 Flutter 引入
visionOS 宿主。启动生成的应用并依次检查：
1. 二维窗口；
2. Open volume；
3. 不透明度 Slider；
4. 注视焦点；
5. 间接捏合；
6. 窗口缩放；
7. 无障碍标签。

在 docs/verification/visionos-spike/ 下新增带日期的验证记录，准确写入 Xcode、
SDK、Simulator Runtime、CPU 架构、命令和结果。只更新实际执行过的测试矩阵行。

最后输出 git diff、测试证据和仍需 Vision Pro 真机执行的项目。除非我明确要求，
不要提交或推送。
```

## 预期产出

- visionOS Simulator 构建及单元测试结果；
- 二维窗口和体积窗口截图或日志；
- 测试环境版本信息；
- 已执行矩阵状态；
- 仍需真机验证的清单。
