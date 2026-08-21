# Mac 既有 Codex 项目提示词：增量验证 FacetWire visionOS Spike

## 使用背景

Mac 上已经存在对应的 FacetWire Codex 项目，并且已经完成过一轮 macOS 和 iOS 验证。本次不是创建新项目，也不是重新执行全部跨平台验证，而是在原项目、原工作区和既有验证证据之上补充 visionOS Simulator 验证。

把下面整段提示词交给 Mac 上现有 FacetWire Codex 项目：

```text
你当前位于 Mac 上已经存在的 FacetWire Codex 项目中。这个项目此前已经完成一轮
macOS 和 iOS 验证。本次任务是增量补充 visionOS Host Spike 验证，不是新建项目，
也不是重新执行全部 macOS/iOS 测试。

一、保护现有项目状态

1. 不要创建新的 Codex 项目、任务或仓库副本，不要重新 clone FacetWire。
2. 先运行 pwd、git rev-parse --show-toplevel、git status --short --branch 和
   git log -5 --oneline，确认当前就是既有 FacetWire 工作区。
3. 保留所有既有 macOS/iOS 验证记录、签名设置、本地配置和用户改动。
4. 禁止执行 git reset --hard、git clean、git checkout --、强制覆盖、删除构建配置，
   也不要擅自 stash 用户改动。
5. 如果工作区存在未提交改动：先判断是否与本次 visionOS 文件重叠。无重叠则保留
   并继续；有重叠则停止修改并向我说明，不得覆盖。

二、同步本次增量

1. 在工作区允许安全快进时执行：

   git fetch origin
   git pull --ff-only origin main

2. 确认当前历史至少包含以下提交：

   546869a feat: add visionOS host spike
   2e71fab chore: mark visionOS validator executable

3. 不要重新生成现有 Flutter macOS/iOS Runner，也不要改动已经验证通过的平台配置。

三、完整阅读本次新增设计

- docs/adr/0002-visionos-host-strategy.md
- spec/spatial-surface-v0.1.md
- spikes/visionos_host/README.md
- docs/verification/visionos-spike/test-matrix.md
- docs/verification/visionos-spike/2026-08-22-windows.md

同时查看现有 docs/verification/ui-spike/ 下的 macOS/iOS 结果，理解已有验证基线，
不要覆盖、合并或改写这些历史证据。

四、只执行 visionOS 增量验证

1. 检查 Xcode、visionOS SDK、已安装的 visionOS Simulator Runtime 和 XcodeGen。
2. 如果缺少 XcodeGen，先向我申请安装许可；不要静默安装依赖。
3. 运行：

   ./scripts/validate-visionos-spike-macos.sh

4. 如果默认 Apple Vision Pro Simulator 名称不可用，运行：

   xcrun simctl list devices available

   然后通过 FW_VISIONOS_DESTINATION 指定实际存在的 visionOS Simulator，再重试。
5. 启动生成的 visionOS 应用，只验证本次新增项目：
   - Swift/C 静态桥接；
   - DisplayList Decoder 单元测试；
   - 二维窗口显示；
   - Open volume 与体积窗口；
   - 不透明度 Slider；
   - 窗口缩放和宽高比；
   - 注视焦点；
   - 间接捏合；
   - Semantics/无障碍标签；
   - 关闭、重新打开窗口后的生命周期。

五、故障处理边界

1. 只修复能够稳定复现、且属于 spikes/visionos_host、SpatialSurface 草案或
   visionOS 验证脚本的缺陷。
2. 不得为了通过测试而降低断言、跳过测试或删除错误处理。
3. 不得重新设计 FacetWire Core ABI，不得把 SwiftUI、RealityKit 或其他平台对象
   加入插件 ABI，也不得把 Flutter 作为 visionOS 宿主依赖。
4. 如果修改触及共享 C Bridge 或 DisplayList Decoder，才执行受影响的 macOS/iOS
   定向回归；共享代码没有变化时，不重复之前已经通过的 macOS/iOS 全量验证。
5. 不要把 visionOS Simulator 通过推导为 Vision Pro 真机通过。

六、记录增量证据

1. 在 docs/verification/visionos-spike/ 下新增一份带实际日期的 Mac 验证记录，写明：
   - 基于既有 Mac Codex 项目的增量验证；
   - 起始和结束 Git commit；
   - macOS、Xcode、visionOS SDK、Simulator Runtime 和 CPU 架构；
   - XcodeGen 版本；
   - 实际命令、测试数量、构建结果和失败日志摘要；
   - 是否修改代码，以及修改原因；
   - 仍需 Vision Pro 真机验证的项目。
2. 只更新 test-matrix.md 中本次实际执行的 VSP-SIM-* 行。
3. 没有 Vision Pro 真机时，VSP-DEV-* 必须继续保持 NOT RUN。
4. 保留 Windows、macOS 和 iOS 的既有验证文件，不改写历史结果。

七、最终报告

完成后向我报告：
- 是否成功快进到包含 2e71fab 的版本；
- visionOS 构建、单元测试和手工检查分别通过了哪些项目；
- 新增或修改了哪些文件；
- 是否触及共享代码，是否需要补跑 macOS/iOS 定向回归；
- 哪些项目仍然只能在 Vision Pro 真机验证；
- git status 和 git diff 摘要。

除非我明确要求，不要提交或推送。
```

## 预期结果

- 复用 Mac 上现有的 FacetWire Codex 项目和验证环境；
- 原有 macOS/iOS 验证证据保持不变；
- 只新增 visionOS Simulator 验证记录；
- 只有共享代码实际发生变化时，才补做必要的 macOS/iOS 定向回归；
- Vision Pro 真机项目继续与 Simulator 结果分开记录。
