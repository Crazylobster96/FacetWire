# Mac 既有 Codex 项目提示词：增量验证 Flow Layout columns

用途：Mac 上已经有 FacetWire Codex 项目，并已完成过 macOS、iOS 及 visionOS 的前序
验证。本次只验证新增的 `columns + block` 及受影响的共享 Bridge/UI，不新建项目，
不覆盖既有证据。

把下面整段内容发送给 Mac 上现有的 FacetWire Codex 项目：

---

你正在 Mac 上已有的 FacetWire Codex 项目中继续工作。这个项目已经完成过 macOS、
iOS 和 visionOS 的前序验证。本次任务是拉取包含 Flow Layout `columns + block` 的最新
`main`，进行增量验证并保存证据；不要新建仓库、Codex 项目或 Flutter Runner。

先执行只读检查：

```sh
pwd
git status --short
git branch --show-current
git log -1 --oneline
```

保留所有本地签名、设备选择、历史验证文件和用户改动。只有工作树干净且分支没有分叉
时才执行：

```sh
git pull --ff-only
```

若存在本地修改或无法快进，停止拉取，说明确切文件与分支状态，不要 reset、checkout
或覆盖修改。拉取后确认最新提交包含以下合同：

- `pageMode`: 0 = continuous，1 = virtual-pages，2 = columns；
- columns 报告 `columnCount`、`columnGap`、`contentBounds`；
- Fragment 报告 `columnIndex`；
- 支持切片为 `continuous+virtual-pages+columns+block`。

执行自动验证：

```sh
./scripts/validate-mobile-macos.sh
./scripts/validate-visionos-spike-macos.sh
```

如果脚本没有覆盖正式 Playground 的 Dart 测试，再进入
`examples/placeholder_demo` 执行 `flutter analyze` 和 `flutter test`。不要仅用 Dart
回退冒充 Native 通过；测试或界面合同必须显示 Native PASS。

分别在 macOS App 和 iOS Simulator App 打开 Flow Layout 验证页，执行以下手工检查：

1. 连续、虚拟页、双栏三个模式按钮都可选择，切换模式不改变当前 Level。
2. continuous 下每层仍为 text-object-text，Level 3 中间仍是 Placeholder。
3. virtual-pages 下 Level 1/2 为 3 页，pageIndex 为 0、1、2；Level 3 为 2 页，
   pageIndex 为 0、0、1。
4. columns 下报告 `pageCount = 1`、`columnCount = 2`，三个 Fragment 的
   `columnIndex` 为 0、0、1；界面显示两个 Native 计算的栏边界。
5. 三种模式中对象类型、sourceItemId、Level 1/2 图片和 Level 3 Placeholder 身份一致。
6. 递归合成仍按 L1/L2/L3 原始 Zone 坐标与尺寸定位；独立调整三层不透明度时，下层
   可见且几何不漂移，0 完全透明、1 完全不透明。
7. “随窗口适配”和“固定 1:1”都可用，查看策略不能触发重新排版或改变 Plan。
8. 合同区显示 Native PASS、Complete PASS、Balanced PASS、Status 0、3 fragments。

在 visionOS Simulator（有真机时也在 Vision Pro）补验：三模式 Picker 可用，columns
显示 2 栏且末段位于第 2 栏，Level 3 Placeholder 身份不变，诊断为绿色
`PASS · native Flow`。Swift/XCTest 必须实际编译通过，不能只静态阅读代码。

把结果写入新的证据文件，例如：

`docs/verification/macos-ios/YYYY-MM-DD-flow-columns.md`

记录 commit、macOS/Xcode/Flutter 版本、模拟器或设备型号与系统版本、每条命令结果、
三个模式的手工结果、截图路径和任何失败原因。不要改写已有验证文件。除非用户明确
要求，不要提交或推送；完成后报告修改文件、自动测试计数、各平台 PASS/FAIL/BLOCKED
和证据文件路径。

---

## 提示词自检

- 以既有 Mac Codex 项目为前提，不会重复建仓库或重置工作树。
- 将 Native columns 合同、UI 投影和内容身份分开验收。
- macOS/iOS/visionOS 共享实现均有自动与手工证据要求。
- 不以模拟回退、静态代码检查或历史截图替代本次编译运行结果。
