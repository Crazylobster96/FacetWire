# FacetWire Playground UI Spike

> **Migration status / 迁移状态**
>
> The renderer pages, scenes, platform runners, and Flow Native Bridge from
> this spike are now integrated into `examples/placeholder_demo`, the canonical
> FacetWire Playground. Keep this directory for architecture history and
> isolated experiments; do not ship it as a second user-facing app.
>
> 本 Spike 的 Renderer 页面、场景、平台 Runner 与 Flow Native Bridge 已迁移到
> `examples/placeholder_demo`（正式 FacetWire Playground）。本目录仅保留作架构历史
> 和隔离实验，不再作为第二个面向用户的 App 发布。
This disposable spike validates the boundary selected by ADR-0001:

~~~text
FacetWire C ABI -> owned byte buffer -> Dart FFI decoder -> CustomPainter
                                              -> Flutter Semantics
~~~

It is deliberately not a production package. The native bridge and its CTest
suite can run without Flutter. Android, iOS, macOS, Windows, and Linux runner
sources are committed so every platform starts from the same reviewed project.
Generated SDK caches, local signing data, and build outputs remain untracked.

## Core Content demo / 基础内容演示

The app starts on the Text + Core Image 0.1 page. It loads a conforming
uncompressed ASP directory package with three native-size recursive canvases,
overlapping Text/Image/GIF layers, independent type debug multipliers, and a
direct 0–1 opacity override for the selected layer. Nested documents use
`fit=none`; no implicit child scaling is applied. The developer-board icon
opens the legacy Placeholder Renderer compatibility screen.

应用启动后默认进入 Text + Core Image 0.1 页面。该页面使用三层标准目录包，验证原始
坐标递归、图层覆盖、PNG/GIF、文字和透明度交互；顶栏的开发板图标用于打开旧版
Placeholder Renderer 兼容性测试页。完整四平台步骤与手工检查表见
[`../../docs/guides/core-content-renderers-demo-validation.zh-CN.md`](../../docs/guides/core-content-renderers-demo-validation.zh-CN.md)。

The folder icon accepts either an uncompressed package directory or its root
`.dis.json`/`.dis` descriptor. Desktop runners also accept `--demo <path>` or
one positional path. Relative resource and nested-document references remain
resolved inside the selected package root; paths escaping that root are denied.

顶栏文件夹图标可以输入未压缩包目录，或直接输入根 `.dis.json`/`.dis` 描述文件。
桌面程序也支持启动参数：

~~~powershell
facetwire_playground_ui_spike.exe --demo "D:\demos\sample.agscene"
facetwire_playground_ui_spike.exe "D:\demos\sample.agscene\sample.agscene.dis.json"
~~~

移动端可通过同一界面输入应用沙箱内的包路径；正式宿主也可通过
`SpikeApp.initialDemoPath` 直接注入已接收文件的路径。

The preview provides two independent viewport policies: `Fit viewport` scales
the root canvas proportionally as the app window changes; `Actual 1:1` keeps
the document's logical pixel size fixed, centers it, disables wheel/pinch
scaling, and allows panning when the viewport is smaller than the canvas.

预览控制栏可切换“适应窗口”和“固定 1:1”。后者始终保持描述文件中的逻辑尺寸并居中，
窗口较小时只改变可视区域，可拖动画布查看，不会因 App 窗口变化重新缩放内容。

## Flow Layout 0.1 verification / 流式排版验证

The stream icon opens a verification page backed by the real native
org.facetwire.reference.flow-layout plugin. Its uncompressed package has three
recursive levels. Levels 1 and 2 compose text-image-text; Level 3 proves that
an unknown child keeps its bounds through Placeholder fallback. Recursive mode
composes all three native-size Canvases at their cumulative child-Zone offsets
and exposes independent per-level opacity. The page also tests content-dependent
virtual pagination, whole-view opacity, fit mode, and fixed 1:1 mode.
Block and inline paragraph modes are orthogonal to the three page modes. Inline
mode verifies ordered text/object/text parts, atomic replacement objects, exact
UTF-8 byte ranges, and compact Placeholder fallback.

顶栏的流式布局图标打开 Flow Layout 0.1 验证页。该页面通过 Native Assets 调用真实
C 插件，不是 Dart 布局模拟。三层未压缩场景依次验证图片对象、嵌套图片对象和未知对象
Placeholder 降级。默认递归合成按 child Zone 的累计坐标同时显示三层，每层保持原始
逻辑尺寸并可独立调整不透明度；“单层检查”用于隔离当前 Level。`contentCase` 与
`pageMode` 相互独立，`pageMode` 可选择 continuous、virtual-pages 或 columns；切换
排版模式不得切换 Level，页数由当前内容决定。columns 模式显示真实栏边界与
`columnIndex`。“块对象/行内对象”与页面模式正交；行内模式验证 text/object/text
有序部件、不可拆对象、精确 UTF-8 byte range 和紧凑 Placeholder。设备
验收时合同区必须显示 Native PASS。完整命令与五平台验收表见
../../docs/guides/flow-layout-cross-platform-validation.zh-CN.md。
## Audio/Video Renderer demo / 音视频演示

The movie icon on the Core Content page opens the Audio/Video Renderer 0.1
demo. The fixture is a conforming uncompressed package and contains generated,
local-only MP4, WAV, poster, and artwork resources. Video, subtitles, controls,
and audio are four independently positioned layers. Each layer has its own
opacity control using the FacetWire rule `1 = fully opaque, 0 = fully
transparent`.

基础内容页顶栏的影片图标用于打开 Audio/Video Renderer 0.1 演示。演示包符合未压缩
目录格式，包含项目内生成的本地 MP4、WAV、视频海报和音频封面，不依赖网络。视频、
字幕、控制按钮和音频会话是四个独立定位的图层，可分别调整不透明度。控制层支持播放、
暂停、前后跳转、倍速、旋转和渐隐；字幕层的位置与透明度不会改变视频层的几何关系。

完整构建命令、手工检查表和当前验证结果见
[`../../docs/guides/media-renderer-demo-validation.zh-CN.md`](../../docs/guides/media-renderer-demo-validation.zh-CN.md)。

## Native verification

Run these commands from spikes/playground_ui:

~~~powershell
cmake -S native -B ../../build/playground-ui-native -G Ninja
cmake --build ../../build/playground-ui-native
ctest --test-dir ../../build/playground-ui-native --output-on-failure
~~~

## Flutter verification

Use the SDK pinned in ../../toolchains.lock.json, then run:

~~~powershell
flutter pub get
flutter analyze
flutter test
~~~

The Flutter build hook compiles the native bridge and Flow Layout plugin through
Native Assets for Windows, macOS, iOS, and Android; no manual DLL copy is
required. visionOS statically compiles the same sources through its XcodeGen
project. Production work must move long-running FFI calls to a worker isolate;
the spike keeps decoder and client ports separate so that isolation can be
measured without changing the presentation API.

## Binary display-list batch v1

All integers and IEEE-754 floats are little-endian. The 12-byte header is
FWDL, uint16 version, uint16 headerSize, uint32 commandCount. Every
40-byte command contains uint8 opcode, three reserved bytes, then nine
float32 values: x, y, width, height, radius, r, g, b, a.

Unknown versions and opcodes fail closed. Buffers are owned by the native
library and must be released exactly through fwui_buffer_release.

## Mobile builds

Windows:

~~~powershell
powershell -ExecutionPolicy Bypass -File ..\..\scripts\build-android.ps1 -UseChinaMirror
~~~

For macOS and iOS, follow the Flutter local toolchain guide under docs/guides
from the repository root.
