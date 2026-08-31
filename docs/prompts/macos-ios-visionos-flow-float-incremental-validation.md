# Mac 既有 Codex 项目提示词：增量验证 Flow Layout Float 0.1

你正在 Mac 上已有的 FacetWire Codex 项目中继续工作。此前 macOS、iOS、visionOS 和
Flow inline 已完成验证；本次只补验最新 `main` 中的 Flow Layout `float-start/end`、共享
C Bridge 与 Flutter UI，不要新建仓库、项目或替换既有签名配置。

开始前只做只读检查：`pwd`、`git status --short --branch`、当前分支和 commit。只有工作树
完全干净且 `main` 可快进时，才执行 `git fetch origin` 与 `git pull --ff-only origin main`。
如发现任何本地修改或未跟踪的 Xcode/CocoaPods 文件，立即停止拉取并逐项列出；不得自行
stash、reset、checkout、clean、删除或覆盖。

拉取成功后：

1. 使用仓库固定的 `FACETWIRE_FLUTTER_ROOT`，不得 upgrade、切换 channel 或更新 lockfile。
2. 运行根 CMake/CTest、正式 `examples/placeholder_demo` 的 `flutter analyze` 与
   `flutter test`、Spike 的 `flutter analyze` 与 `flutter test`。
3. 构建 macOS Debug、iOS Simulator Debug，并运行
   `./scripts/validate-visionos-spike-macos.sh`；保留原有 Developer Team 与 bundle 设置。
4. 核对 Native Bridge：`contentCase` 0..2=block，3..5=inline，6..8=float-start，
   9..11=float-end；`pageMode` 0/1/2=continuous/virtual-pages/columns。Float 报告必须包含
   对应 `placementMode`、`inlineObjects=false`、`composeStatus=0`、`complete=true`、
   `fragmentCount=3`，supported slice 必须含 `float-start+float-end`。
5. 在 macOS App 与 iOS Simulator 打开“Flow Layout 0.1 验证”，分别选择 Level 1/2/3，
   检查“起始浮动”和“末端浮动”：连续模式中对象位于逻辑 start/end，后一段文字在另一侧
   绕排且不重叠；虚拟页与双栏中对象整体推进，区域切换后旧 exclusion 不得影响新区域；
   Level 3 仍使用相同 bounds 的 Placeholder。
6. 在 visionOS Simulator（以及用户已配置签名时的 Vision Pro 真机）执行同一检查。
   Simulator 通过与真机通过必须分开记录，不得互相替代。
7. 反复切换 block/inline/float-start/float-end、三种 pageMode、递归/单层、各层不透明度、
   随窗口适配/固定 1:1，确认 Level、sourceItemId、对象类型和已有行为不漂移。

把结果写入新文件
`docs/verification/macos-ios/YYYY-MM-DD-flow-float.md`，记录 commit、工具版本、命令退出码、
自动测试计数、macOS/iOS/visionOS 手工矩阵、Simulator/真机边界、截图路径和失败原因。
截图放在既有忽略目录，不提交二进制证据。若为修复 Apple 工程接入而产生源码修改，先说明
原因与影响，运行完整回归后再提交到独立 `codex/` 分支；不要直接覆盖 main 或推送，除非
用户明确授权。
