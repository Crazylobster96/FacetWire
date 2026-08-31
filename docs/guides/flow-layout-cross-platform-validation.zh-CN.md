# Flow Layout 0.1 跨平台验证指南

本指南用于验证 FacetWire Playground 与 visionOS 原生宿主是否真正调用
“org.facetwire.reference.flow-layout”，而不是只显示 Dart/Swift 模拟结果。

## 1. 验证范围

同一份 C 实现通过两条平台接入路径使用：

- Windows、macOS、iOS、Android：Flutter Native Assets CBuilder 自动编译并打包；
- visionOS：XcodeGen 工程静态编译统一 C Bridge、
  plugins/flow_layout/src/plugin.c 与 flow_virtual_pages.c。

Flutter 页面顶栏的流式布局图标打开“Flow Layout 0.1 验证”。visionOS 页面直接包含
Flow Layout 区域。设备验收必须看到 Native PASS；Native FAIL 代表当前运行的是
Dart 测试回退，不能记为平台通过。

## 2. 三层标准场景

未压缩演示包位于：

examples/placeholder_demo/assets/documents/flow-layout-recursive-demo.agscene/

它使用相对路径形成三层递归文档。每层包含一个 continuous Flow，逻辑页面为
600 × 700，并同时提供“文本—块对象—文本”和“文本—行内对象—文本”两组内容关系。

| 场景 | 对象行为 | 期望 Layout Plan |
|---|---|---|
| Level 1 | PNG 图片对象 | text, object, text；3 fragments |
| Level 2 | 嵌套文档中的 PNG 图片对象 | text, object, text；3 fragments |
| Level 3 | 未知对象类型 | text, placeholder, text；3 fragments |
| Level 1 + virtual-pages | 240 高度虚拟页 | composeStatus=0、complete=true、3 pages；对象整体移至第 2 页，后一段文字位于第 3 页 |
| Level 2 + virtual-pages | 240 高度虚拟页 | composeStatus=0、complete=true、3 pages；对象整体移至第 2 页，后一段文字位于第 3 页 |
| Level 3 + virtual-pages | 240 高度虚拟页 | composeStatus=0、complete=true、2 pages；Placeholder 与首段位于第 1 页，后一段文字位于第 2 页 |
| Level 1/2/3 + columns | 2 个等宽栏，栏间距 24 | composeStatus=0、complete=true、1 page；前两个片段位于第 1 栏，末段位于第 2 栏；内容类型和 sourceItemId 不变 |
| Level 1/2 + inline | PNG 作为不可拆 replacement segment | text, object, text；3 fragments；文本 byte range 为 0..7、7..21；对象为 72 × 36 |
| Level 3 + inline | 未知行内对象类型 | text, placeholder, text；3 fragments；Placeholder 保留 72 × 36 边界 |
| inline + virtual-pages/columns | 同一行内内容切换区域策略 | 对象不可拆；text/object 游标连续；page/column 变化不改变 sourceItemId、byte range 或对象尺寸 |
| Level 1/2/3 + float-start/end | 图片或 Placeholder 成为浮动对象 | 对象位于逻辑 start/end；后一段文本接收矩形 exclusion 并在剩余宽度内绕排；3 fragments |
| float + virtual-pages/columns | 浮动对象跨区域放置 | 当前区域不足时对象整体进入下一栏/页；区域切换后旧 exclusion 不得泄漏；Page Sink 保持平衡 |

Level 3 的 Placeholder 必须保留对象边界，后一段文字不能坍缩到未知对象的位置。

Playground Bridge v2 将内容选择与分页策略拆成两个正交参数：`contentCase` 的 0..2
选择 Level 1/2/3 块对象，3..5 选择对应 Level 的行内对象，6..8 选择 float-start，
9..11 选择 float-end；`pageMode` 选择 continuous、virtual-pages 或 columns。切换排版
模式或段落内容关系不得改变当前 Level。旧
`fwui_compose_flow_demo` ABI 仅为兼容既有宿主保留；新宿主必须调用
`fwui_compose_flow_demo_v2`。

行内对象首先通过 Child Measure Service 得到精确尺寸，再由声明 `INLINE_PARTS` 能力的
Text Fragment Service 统一决定文字、对象、baseline、BiDi 和换行几何。Flow 插件只校验
有序部件并输出 Fragment；它不得自行估算字体位置或调用 Image/Placeholder Renderer。

columns 是 Flow Layout Plan 的真实排版模式，不是 Playground 的视觉分栏。Native Bridge
必须报告 `columnCount`、`columnGap`、`contentBounds` 和每个 Fragment 的
`columnIndex`。推进顺序固定为“当前栏 → 下一栏 → 下一页”；不可拆对象空间不足时整体
移至下一栏或下一页，不得被裁成两段。栏宽非法或不足最小文本宽度时必须明确拒绝。

验证页默认使用“递归合成”：L1、L2、L3 按描述文件中的 child Zone 逐级累加原始坐标，
每层保持自身 Canvas 的逻辑尺寸，不做隐式缩放。为了检查完整嵌套关系，Playground 使用
调试展开画布并绘出 child Zone 边界；“单层检查”只显示当前选中的 Level。递归模式中的
L1/L2/L3 分别提供独立不透明度，规则统一为 `1 = 完全不透明，0 = 完全透明`，用于
透视上层并核对下层位置；“预览不透明度”仍作用于整个合成结果。

## 3. 通用手工验收

1. 启动 App，打开“Flow Layout 0.1 验证”。
2. 确认合同区显示 Native PASS、Complete PASS、Balanced PASS、Status 0 和
   3 fragments。
3. 保持默认“递归合成”，确认 L1、L2、L3 同时存在，逐级以 child Zone 的原始坐标
   嵌套，Canvas 尺寸不因父级 Zone 自动缩放。选择 Level 只改变高亮和合同信息，不能
   隐藏另外两层。
4. 分别拖动 L1/L2/L3 不透明度；上层降低后必须看到其下层，设为 0 后该层完全透明，
   其他层的几何与不透明度不得改变。
5. 切换“单层检查”，依次选择 Level 1、Level 2，确认图片位于两段文字之间；选择
   Level 3，确认中间出现橙色“Placeholder / 后备占位”。
6. 保持 Level 1 或 Level 2，选择“虚拟页”，确认出现 3 张页面，三个片段
   pageIndex 依次为 0、1、2。选择 Level 3 时应为 2 张页面，三个片段 pageIndex 为
   0、0、1。对象不得被拆开，片段继续使用各自页面内的逻辑坐标。
7. 切换到“双栏”，确认出现两个真实栏边界且报告 `columnCount = 2`。三个片段的
   `columnIndex` 应为 0、0、1，仍位于同一页；Level 3 中间项仍是 Placeholder。
8. 在任意 Level 上反复切换 continuous、virtual-pages 和 columns，确认当前 Level、
   对象类型和 sourceItemId 保持不变；模式控件只能改变 Layout Plan 的排版策略。
9. 切换“行内对象”，确认合同区显示 Inline；每层显示 text/object/text，同一段落的两个
   Text Fragment 分别只显示 `Inline ` 与 ` stays atomic.`。Level 3 中间应显示紧凑的橙色
   Placeholder，不能溢出 72 × 36 边界。在三种页面模式间切换，对象不得被切开或重复。
10. 切换“起始浮动”和“末端浮动”，确认对象在连续模式中分别位于逻辑 start/end，后一段
    文本位于对象另一侧且不与对象重叠；切换虚拟页/双栏时对象整体推进，正文宽度不足时
    位于 float 下方或下一栏，旧区域的 exclusion 不得影响新区域。
11. 拖动“预览不透明度”，确认 1 为完全不透明、0 为完全透明，片段几何不变化。
12. 在“随窗口适配”和“固定 1:1”之间切换；连续模式的单层逻辑尺寸为 600 × 700；
   virtual-pages 的单层高度按实际 pageCount、页高和页间距计算。视口只允许平移或
   等比缩放查看，不得重新排版或改变 Layout Plan。
13. 记录平台、设备/模拟器、系统版本、构建 commit、上述每项结果和截图。

## 4. Windows

在仓库根目录执行正式 Playground 的确定性构建脚本：

    powershell -ExecutionPolicy Bypass -File examples\placeholder_demo\scripts\build-windows.ps1

脚本会进入 Visual Studio Developer Environment，使用 Ninja 构建统一 C Bridge，
运行 CTest、Flutter analyze/test，并生成保留兼容文件名的正式入口：

    examples/placeholder_demo/build/windows-ninja/runner/facetwire_placeholder_demo.exe

Windows 正式验证应使用上述脚本，不应以 `flutter build windows --release` 代替。
Flutter 默认的 Visual Studio Generator 路径会在 `media_kit` 原生依赖配置期间长期无输出；
项目脚本会在 CMake 配置前显式下载并校验固定版本的 libmpv 与 ANGLE（下载超时 10 分钟），
随后固定使用 Host x64 + Ninja，并按正确顺序构建 Runner。

若出现 stddef.h、stdint.h、windows.h 等 SDK 头缺失，应调用
`msvc-build-environment` skill。脚本还会在首次构建时确定性解压 media_kit 所需的
libmpv 与 ANGLE，避免 Ninja 在依赖文件生成前提前链接。

### 当前自动验证记录（2026-08-31）

- 根项目 MSVC/Ninja CTest：PASS（14/14）。
- Flow/Playground/Memory 定向 CTest：PASS（3/3）；独立 Spike Bridge：PASS（1/1）。
- 正式与 Spike Flutter analyze：PASS（均为 0 issues）。
- 正式 Flutter test：PASS（25/25）；Spike Flutter test：PASS（13/13）。
- MSVC ASan Spike Bridge：PASS（1/1）；通过开发者环境解析
  `clang_rt.asan_dynamic-x86_64.dll` 后未发现 sanitizer 错误。
- Windows Release Runner：PASS；确定性 Ninja 构建、安装及 Dart FFI smoke 均通过，产物为
  `examples/placeholder_demo/build/windows-ninja/runner/facetwire_placeholder_demo.exe`。
- 本次自动验证确认 columns 的 2 栏推进、`columnIndex`、内容身份稳定和非法栏宽拒绝，
  以及 inline 的三模式续排、四种 baseline、RTL、能力协商、旧 block-only ABI 兼容、
  精确 byte range、Fallback 边界，以及 float 逻辑侧、exclusion、最小宽度清除和活动预算。
  Apple 最终视觉结果仍按第 3 节记录截图。

## 5. Android

    powershell -ExecutionPolicy Bypass -File scripts\build-android.ps1 -Mode debug -TargetPlatform android-arm64

APK 成功安装后执行第 3 节全部项目。可用 APK 路径由脚本打印。

### 当前自动验证记录（2026-08-27）

Android arm64 构建在既有 media_kit_libs_android_video 配置阶段访问 GitHub 时超时：
arm64 libmpv JAR 已下载，armeabi-v7a JAR 下载失败。Gradle 脚本会无条件下载四个
ABI，因此即使目标是 arm64 也会触发该网络请求。失败发生在 Flow Native Assets/NDK
编译之前；网络恢复后需重新执行本节命令。

## 6. macOS 与 iOS

在已完成上一轮 macOS/iOS 验证的 Mac 项目中拉取本提交后执行增量验证：

    git pull --ff-only
    ./scripts/validate-mobile-macos.sh

该脚本依次运行根 C/CTest（含统一 Playground Bridge）、Flutter analyze/test、
macOS Debug 和 iOS Simulator Debug 构建。随后分别启动 macOS App 与 iOS Simulator
App，执行第 3 节手工验收。可直接把
[`../prompts/macos-ios-visionos-flow-float-incremental-validation.md`](../prompts/macos-ios-visionos-flow-float-incremental-validation.md)
交给 Mac 上已经存在的 FacetWire Codex 项目；该提示词只补验本次共享 C/Flutter/Swift
变更，不会把既有验证项目误当成新项目重建。

iOS 真机仍需使用本地 Apple Developer Team 签名；Native Assets 会静态链接同一 C
实现，不依赖任意外部动态插件加载。

## 7. visionOS

    ./scripts/validate-visionos-spike-macos.sh

更新后的 XcodeGen 工程直接编译 Flow Layout 插件。XCTest 应包含：

- 三个平衡 fragments；
- Level 3 未知对象降级为 Placeholder；
- contentCase 与 pageMode 正交；开关 virtual-pages 不得把 Level 2/3 改成 Level 1；
- Level 1/2 的 virtual-pages 返回 3 页且 pageIndex 为 0、1、2；Level 3 返回 2 页且
  pageIndex 为 0、0、1。
- columns 返回 1 页、2 栏，末段 columnIndex 为 1，并保留 Level 3 Placeholder 身份。
- contentCase 3 的 inline 返回 text/object/text、byte range 0..7 与 7..21；
- contentCase 5 在 columns 下保留 72 × 36 的 inline Placeholder，且报告
  `inlineObjects=true`。
- contentCase 6..8/9..11 分别返回 `placementMode=float-start/float-end`，三种页面模式均
  `composeStatus=0`、`complete=true`、3 fragments，且 `inlineObjects=false`。

在 visionOS Simulator 或 Vision Pro 真机中，Flow 区域应显示绿色
“PASS · native Flow”。真机签名使用测试者自己的 Apple Developer Team。

## 8. 结果判定

平台只有同时满足以下条件才可标记为 PASS：

- 构建成功；
- 自动测试成功；
- UI 显示 Native PASS；
- Level 1/2/3 的 block/inline/float-start/float-end 分别在 continuous、virtual-pages 与
  columns 下符合预期；
- 递归合成保留三层原始坐标与尺寸，单层检查只显示选中层；
- L1/L2/L3 独立不透明度、整体预览不透明度、随窗口适配和固定 1:1 交互符合预期。

仅 Dart 回退、仅 CTest、仅模拟器截图或仅旧版 Placeholder 通过，都不能替代完整平台
验收。
