# FacetWire Playground / 跨平台渲染验证应用

This is the canonical cross-platform FacetWire renderer Playground for Windows,
macOS, iOS, Android, and Linux. It keeps the original mature Placeholder page
and executable name, while adding Text/Image/GIF, Audio/Video, and Flow Layout
verification pages to the same app. Placeholder and Flow share one Native
Assets library; the Windows compatibility bundle remains
`facetwire_placeholder_demo.exe`.

这是 FacetWire 的正式跨平台 Renderer Playground，覆盖 Windows、macOS、iOS、
Android 与 Linux。它保留原有成熟 Placeholder 页面和 Windows 可执行文件名，同时在
同一个应用中加入 Text/Image/GIF、Audio/Video 与 Flow Layout 验证页。Placeholder
和 Flow 由同一个 Native Assets 库提供；Windows 兼容入口仍为
`facetwire_placeholder_demo.exe`。

| Page / 页面 | Contract / 内容 | Entry / 入口 |
| --- | --- | --- |
| Placeholder | 真实 `fwdemo_*` C ABI、三层递归、测量/语义/命中测试 | 默认首页 |
| Rich Media Showcase / 富媒体综合演示 | Text、PNG、GIF、内联图表数据、本地 MP4、三层原始坐标、重叠与独立 opacity | 首页文章图标 |
| Audio/Video | 本地 MP4/WAV、字幕层、控制层、音频层 | 首页媒体图标 |
| Flow Layout | 真实 `facetwire.layout.flow`、三层递归合成、独立 opacity、分页与 fallback | 首页流式布局图标 |

Desktop launch arguments may select an uncompressed Core Content package:

```powershell
facetwire_placeholder_demo.exe --demo "D:\demos\sample.agscene"
facetwire_placeholder_demo.exe "D:\demos\sample.agscene\sample.agscene.dis.json"
```

The built-in `rich-media-showcase.agscene` is the default package on the content
page. Video is real local playback through the implemented Core Media path. The
inline bar chart is a Playground-native preview of descriptor data for scene and
layout testing; it does **not** claim that the future Chart Renderer plugin is
implemented.

内置 `rich-media-showcase.agscene` 是内容页的默认演示包。视频通过已实现的 Core
Media 路径播放本地 MP4；内联柱状图由 Playground 根据描述数据原生绘制，用于场景与
布局验证，**不代表**后续独立 Chart Renderer 插件已经实现。
## Demonstrated capabilities / 演示能力

- plugin lifecycle, descriptor, capability and interface discovery；
- `validate`, seven measure-source outcomes, normalized DisplayList recording；
- hidden/minimal/standard/diagnostic modes and six action intents；
- 12 fallback reasons, six Presentation Session phases, fraction progress and
  stale state；
- opacity multiplication, target font scale, dark/high-contrast/reduce-motion
  profile fields；
- Semantics and real hit testing；
- deterministic parent Zone → child Canvas composition across three recursively
  nested documents, preserving 1:1 intrinsic size by default and applying
  `contain`/`cover`/`fill` only when explicitly requested；
- Canvas/Page/Layer/Zone relationship-path inspection；
- renderer parameter schema inspection。

The current renderer contract accepts phase/progress/reduce-motion fields, but
not every accepted field has a dedicated visual animation yet. The report strip
shows the actual native contract result so accepted state is not confused with
an already implemented visual treatment.

当前 Renderer 合同已经接收 phase/progress/reduce-motion，但并非每个字段都已有专属
视觉动画。界面底部展示真实 Native 合同结果，以区分“合同已支持”和“视觉已实现”。

Flow Layout 页面默认递归合成三层 Canvas：子层使用描述文件中的 Zone 坐标和自身逻辑
尺寸，不做隐式缩放；L1/L2/L3 可分别调整不透明度，以检查重叠后的下层内容。也可切换
到“单层检查”。分页开关与 Level 选择互不影响，Native Bridge v2 使用独立的
`contentCase` 和 `pageMode` 参数；`pageMode` 可选择 continuous、virtual-pages 或
columns，虚拟页数量由当前内容计算，不假定固定三页。columns 模式展示真实的两栏
内容边界、栏间距和每个 Fragment 的 `columnIndex`，内容身份与递归关系不得因切换
排版模式而变化。

## Package fixture / 示例包

The integrated rich-media fixture is bundled under
`assets/documents/rich-media-showcase.agscene/`:

```text
rich-media-showcase.agscene/
  rich-media-showcase.agscene.dis.json              # depth 1: Text/Image/GIF/Chart/Video
  resources/                                        # PNG, GIF, MP4, video poster
  documents/level-2.agscene/
    level-2.agscene.dis.json                         # depth 2: Image/Text/Chart
    resources/level-2.png
    documents/level-3.agscene/
      level-3.agscene.dis.json                       # depth 3: GIF + Text overlap
      resources/level-3.gif
```

All nested document placements use `fit=none`, so each child Canvas retains the
coordinates and logical size declared by its own descriptor. Undeclared Canvas
backgrounds stay transparent, and a document Zone opacity multiplies the entire
child subtree. Video shows its poster first and initializes the player only
after the play button is pressed.

综合富媒体 fixture 位于 `assets/documents/rich-media-showcase.agscene/`。所有嵌套
文档都显式使用 `fit=none`，子 Canvas 保持自身描述文件声明的坐标和逻辑尺寸。未声明
背景的 Canvas 保持透明，document Zone 的 opacity 乘到整个子树；视频先
显示海报，用户点击播放后才初始化播放器。

The source fixture is:

```text
examples/documents/recursive-placeholder-demo.agscene/
  recursive-placeholder-demo.agscene.dis.json       # depth 1
  resources/level-1.txt
  documents/level-2.agscene/
    level-2.agscene.dis.json                         # depth 2
    resources/level-2.txt
    documents/level-3.agscene/
      level-3.agscene.dis.json                       # depth 3
      resources/level-3.txt
```

The Demo bundles an identical copy under `assets/documents`. Flutter unit tests
load and procedurally validate all three descriptors, resources, path rules,
depth limits, IDs, and geometry.

## Windows build and run / Windows 构建运行

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass `
  -File examples\placeholder_demo\scripts\build-windows.ps1
```

The script builds and tests the C bridge, runs `flutter analyze` and
`flutter test`, builds the Release app, and bundles the DLL beside the EXE. It
prints the final executable path.

脚本会依次构建/测试 C Bridge、执行 Flutter 静态分析与单元测试、构建 Release 应用，
最后把 DLL 放到 EXE 同目录并输出可执行文件路径。

## macOS build and run / macOS 构建运行

From the repository root on a Mac with the pinned Flutter SDK bootstrapped:

```bash
bash examples/placeholder_demo/scripts/build-macos.sh
```

The script builds/tests the same C sources, runs Flutter checks, builds the
Release `.app`, puts the dylib in `Contents/Frameworks`, and ad-hoc re-signs the
local test bundle. It does not configure Developer ID distribution signing.

脚本使用同一份 C/Dart 源码完成测试，把 dylib 放入 `.app/Contents/Frameworks`，并对
本地测试包进行 ad-hoc 重签名；它不会代替正式 Developer ID 分发签名配置。

## Manual acceptance / 手工验收

1. Scene tree shows `document Zone → child Canvas` at depths 1, 2, and 3; the
   two embedding Zones are not hidden.
2. The center preview composes all three Canvases at once. With no placement in
   the fixture, every child Canvas keeps 1:1 intrinsic logical size. The viewer
   starts at 100% and uses horizontal/vertical scrolling; it does not implicitly
   fit the root Canvas to the window.
3. Selecting image/chart/video Zones changes the complete relationship path,
   reason, label, and semantics role.
4. “不透明度” from 100% to 0% reveals the actual parent Canvas below the
   selected Zone; final background alpha equals background alpha × opacity.
5. Mode and size/font scale change visual density from none through actions.
6. Clicking the action rectangle reports the actual native Action Intent.
7. Measure scenario changes the returned source/size without changing Zone
   placement.
8. Parameter Schema opens from the app-bar `{}` button.
9. Dark/high-contrast/stale/phase changes are reflected by drawing or the native
   report strip as defined by the current implementation.
## Mobile and Linux / 移动端与 Linux

The canonical app now contains committed Android, iOS, and Linux runners. The
same Dart pages and C sources are compiled through the Native Assets hook; no
runtime download or arbitrary dynamic plugin loading is required on restricted
Apple platforms. Use the pinned Flutter SDK and the platform commands in
`docs/guides/flow-layout-cross-platform-validation.zh-CN.md`.

正式 App 已包含 Android、iOS 与 Linux Runner。所有平台使用同一份 Dart 页面和 C
源码，由 Native Assets Hook 在构建期静态注册/随包发布；Apple 受限平台不需要运行时
下载或任意动态加载。具体命令见
`docs/guides/flow-layout-cross-platform-validation.zh-CN.md`。

visionOS remains the native SwiftUI host under `spikes/visionos_host` per ADR-0002; it reuses the same C contracts because Flutter is not the committed visionOS deployment target.

visionOS 仍按 ADR-0002 使用 `spikes/visionos_host` 下的 SwiftUI 原生宿主，并复用相同 C 合同；当前不把 Flutter 作为既定 visionOS 部署目标。

The former `spikes/playground_ui` project remains as a historical architecture
spike and migration reference. New user-facing renderer pages and device
acceptance belong here.

原 `spikes/playground_ui` 继续保留为历史架构 Spike 和迁移参考；后续面向用户的
Renderer 页面及设备验收统一进入本目录。
