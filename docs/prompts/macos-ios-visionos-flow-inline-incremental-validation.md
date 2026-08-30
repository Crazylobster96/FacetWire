# Mac 既有 Codex 项目提示词：增量验证 Flow Layout inline object

用途：Mac 上已经有 FacetWire Codex 项目，并完成过 macOS、iOS、Android 与 visionOS
前序验证。本次只补验新增的 Flow Layout `inline object`、共享 C Bridge、Flutter UI 和
visionOS Swift Host；不新建仓库、Codex 项目、Flutter Runner 或 Xcode 工程。

把下面整段内容发送给 Mac 上现有的 FacetWire Codex 项目：

---

你正在 Mac 上已有的 FacetWire Codex 项目中继续工作。这个项目已经完成过 macOS、iOS
和 visionOS 的前序验证。本次任务是拉取包含 Flow Layout `inline object` 的最新 `main`，
执行增量验证并新增证据；不要新建仓库、Codex 项目、Flutter Runner 或手工替换 Xcode
工程。

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

若存在本地修改或无法快进，停止拉取，说明确切文件与分支状态；不要 reset、checkout
或覆盖修改。拉取后确认最新代码包含以下合同：

- `contentCase` 0..2 = Level 1/2/3 block，3..5 = 对应 Level inline；
- `pageMode` 0 = continuous，1 = virtual-pages，2 = columns；
- Text Fragment Service 通过 `FW_TEXT_FRAGMENT_SERVICE_INLINE_PARTS` 声明行内能力；
- report 含 `inlineObjects`，Fragment 含 `textStart`、`textEnd`；
- 支持切片为 `continuous+virtual-pages+columns+block+inline`。

执行自动验证：

```sh
./scripts/validate-mobile-macos.sh
./scripts/validate-visionos-spike-macos.sh
```

若脚本没有覆盖正式 Playground 的 Flutter 测试，再进入
`examples/placeholder_demo` 执行 `flutter analyze` 和 `flutter test`。必须实际运行 C/CTest、
Native Assets 与 Swift/XCTest；不得以 Dart fallback、静态阅读或历史截图冒充 Native PASS。

分别在 macOS App 与 iOS Simulator App 打开“Flow Layout 0.1 验证”，执行：

1. 确认“块对象/行内对象”与“连续/虚拟页/双栏”是两个正交选择；切换任一选择不改变
   当前 Level。
2. block 模式保持既有 text-object-text；Level 3 中间仍是块级 Placeholder。
3. inline 模式每层为 text-object-text，合同区显示 Inline；前后 Text Fragment 只显示
   `Inline ` 与 ` stays atomic.`，不能都重复显示整段。
4. Level 1/2 中间对象是 72 × 36 图片；Level 3 是同尺寸紧凑 Placeholder，不能出现
   RenderFlex overflow、裁切文字或扩大行高到块级占位尺寸。
5. continuous、virtual-pages、columns 下行内对象均保持不可拆、只出现一次；切换页面模式
   不改变 sourceItemId、对象尺寸或文本 byte range 0..7、7..21。
6. columns 报告 1 页、2 栏；virtual-pages/columns 的 text/object 游标均有进展，不产生
   空白页、重复对象或无穷循环。
7. 递归合成仍按 L1/L2/L3 原始 Zone 坐标与尺寸定位。三层不透明度 0 = 完全透明、
   1 = 完全不透明；切换 inline 不导致下层几何漂移。
8. “随窗口适配”和“固定 1:1”只改变查看方式，不触发 reflow 或改变 Layout Plan。
9. 合同区显示 Native PASS、Complete PASS、Balanced PASS、Status 0、3 fragments。

在 visionOS Simulator（有真机时也在 Vision Pro）执行：

1. Flow 区域出现 Block object / Inline object Picker，三种 Page mode 均可选。
2. Inline Level 1/2 显示三个有序 Fragment；Level 3 显示紧凑 Placeholder。
3. 诊断显示绿色 `PASS · native inline object · atomic text/object/text`。
4. XCTest 的 `testInlineObjectIsAtomicAndPreservesTextRanges` 与
   `testInlineFallbackWorksAcrossColumns` 实际通过。

把结果写入新的证据文件，例如：

`docs/verification/macos-ios/YYYY-MM-DD-flow-inline.md`

记录 commit、macOS/Xcode/Flutter 版本、模拟器或设备型号与系统版本、每条命令结果、
block/inline × 三种页面模式的手工结果、截图路径和失败原因。不要改写已有验证文件。
除非用户明确要求，不要提交或推送；完成后报告修改文件、自动测试计数、各平台
PASS/FAIL/BLOCKED 和证据路径。

---

## 提示词自检

- 明确以既有 Mac Codex 项目为前提，不会重复建仓库或覆盖签名配置。
- 分别验证 Child Measure、Text inline parts、Flow Fragment 与宿主 UI 投影。
- macOS、iOS、visionOS 均要求真实 Native 路径和新增证据。
- 不以 fallback、旧 columns 结果或静态代码检查替代本轮运行结果。
