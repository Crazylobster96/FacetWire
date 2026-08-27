# FacetWire Playground UI Spike

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

Build native first and copy or link the produced library beside the desktop
runner executable. Mobile integration must statically register the same C ABI.
Production work must move FFI calls to a worker isolate; this spike keeps the
decoder and client ports separate so that isolation can be measured without
changing the presentation API.

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
