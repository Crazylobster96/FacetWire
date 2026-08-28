# Text Renderer 0.1 与 Core Image Profile 演示验证

状态：实现验证指南 / Implementation validation guide
适用平台：Windows、macOS、iOS、Android

## 1. 交付内容

本轮交付包含两个基础渲染插件和一个共用的 Flutter 演示入口：

| 组件 | Capability | 主要验证点 |
| --- | --- | --- |
| Text Renderer 0.1 | `facetwire.renderer.text` | UTF-8、CRLF/LF 规范化、Text Service v2、测量、滚动、DisplayList、Semantics、opacity |
| Core Image Renderer | `facetwire.renderer.image` | Resource 解码服务、固有尺寸、none/contain/cover/fill、裁剪、采样、opacity、Semantics |
| Animated Image Renderer | `facetwire.renderer.animated-image` | GIF/帧动画资源、自动播放初始策略、reduce-motion 稳定帧、opacity |
| Playground Rich Media 页面 | 标准 ASP 目录包 + Playground Chart 预览 | 三层递归、原始坐标嵌套、Layer 覆盖、Text/PNG/GIF/Chart/Video、交互式 opacity |

原生插件验证与 Flutter 视觉验证是两套互补门禁。Flutter 能显示 PNG/GIF 不等于 C ABI
插件已通过；C 插件单元测试也不替代真实设备上的字体、解码器和 GPU 验证。

### 本章检查

- Text、静态图片、动态图片对应三个独立 Capability。
- 插件契约与跨平台演示的职责没有混用。
- 0.1 未引入视频、音频、图表或专业图像合成。

## 2. 三层标准目录包

演示包位于：

```text
examples/placeholder_demo/assets/documents/core-content-overlap-demo.agscene/
├── core-content-overlap-demo.agscene.dis.json       960 × 640
├── resources/
│   ├── level-1.png
│   └── level-1.gif
└── documents/level-2.agscene/
    ├── level-2.agscene.dis.json                     500 × 350
    ├── resources/
    │   ├── level-2.png
    │   └── level-2.gif
    └── documents/level-3.agscene/
        ├── level-3.agscene.dis.json                 230 × 170
        └── resources/
            ├── level-3.png
            └── level-3.gif
```

根文档到第二层、第二层到第三层均显式使用 `placement.fit = "none"`。嵌套 Zone 的大小
分别与子 Canvas 的 500×350 和 230×170 完全相同，因此子 Layer 按原始逻辑坐标显示，
不会发生隐式等比缩放。根 Canvas 为适配实际屏幕可以进行 viewport presentation scale；
该显示缩放不改写文档坐标、Zone bounds 或子文档 placement。

每一层都有 Text、Image 或 Animated Image，三层合计：

- 3 个 Text Zone；
- 3 个 Image Zone；
- 3 个 Animated Image/GIF Zone；
- 2 个递归 Document Zone；
- 11 个 Zone，且各层存在不同 `z` 值造成的可见覆盖关系。

### 本章检查

- 递归深度、Canvas 尺寸和 Document Zone 尺寸可以自动断言。
- 媒体路径均通过当前包 `resources[].id` 间接引用。
- 子包只使用自身相对路径，未使用绝对路径或 `..`。

## 3. opacity 交互规则

标准字段始终采用不透明度语义：

```text
opacity = 1.00  完全不透明 / fully opaque
opacity = 0.00  完全透明 / fully transparent
0.00 < opacity < 1.00  连续有效，包括 0.10、0.99
```

页面提供两类调试控制：

1. Text、Image、GIF、Chart、Video 类型滑杆是批量调试系数，用于快速观察某一类 Layer；
2. 点击任意 Layer 后，“当前层”滑杆直接覆盖该 Zone 的最终 session opacity，取值 0–1，
   不修改目录包内的持久值。

Renderer 不填充隐式白底。最终 Alpha 由内容 Alpha、颜色 Alpha 和有效 opacity 组合；
`opacity=0` 时内容不可见，但 Zone 几何、递归关系和兄弟 Layer 排版保持不变。
`document` Zone 的 opacity 作用于整个子 Canvas 与全部后代 Layer；未显式声明背景时，
子 Canvas 保持透明，不得合成白板。

### 本章检查

- 持久值与调试 Session 覆盖没有混为一谈。
- 0 和 1 的方向与 Core Content Profile 一致。
- 透明内容不会触发布局塌缩。

## 4. 自动化验证

### 4.1 原生静态库

```powershell
cmake -S . -B build/content-renderers -G Ninja `
  -DFACETWIRE_BUILD_TESTS=ON `
  -DFACETWIRE_BUILD_TEXT_RENDERER=ON `
  -DFACETWIRE_BUILD_CORE_IMAGE_RENDERER=ON
cmake --build build/content-renderers
ctest --test-dir build/content-renderers --output-on-failure
```

### 4.2 Windows DLL

```powershell
cmake -S . -B build/content-renderers-shared -G Ninja `
  -DFACETWIRE_BUILD_SHARED=ON `
  -DFACETWIRE_BUILD_TESTS=ON
cmake --build build/content-renderers-shared --config Release
ctest --test-dir build/content-renderers-shared --output-on-failure
```

预期制品包括 `facetwire_text_renderer.dll` 和
`facetwire_core_image_renderer.dll`。受限 Apple 平台使用同一 C 源码的静态库/framework，
通过各自唯一的注册函数接入；桌面动态构建同时导出标准 `facetwire_plugin_query`。

### 4.3 Flutter 共用测试

```powershell
cd examples/placeholder_demo
flutter pub get
flutter analyze
flutter test
```

若代理拦截本机 WebSocket，先把 `localhost,127.0.0.1,::1` 加入 `NO_PROXY` 与
`no_proxy`。依赖存在“有新版本但受锁定约束”提示不代表测试失败，不应在验证期间执行
`flutter upgrade`。

### 本章检查

- Static 与 Shared 构建使用同一实现源码和同一 API 测试。
- Flutter 测试覆盖旧 Placeholder 路径，避免新增页面造成回归。
- 依赖版本仍由根目录 `toolchains.lock.json` 与 `pubspec.lock` 控制。

## 5. 四平台构建与真机检查

应用启动后先显示 Placeholder 兼容性首页；点击顶栏文章图标进入
`FacetWire Rich Media Showcase`。内容页默认加载三层综合演示包，并保留顶栏开发板
图标返回 Placeholder Renderer 兼容性测试页。如果内容页只看到旧的大面积灰色占位框，
说明运行的是旧构建产物，需要重新构建并重新安装应用。

顶栏文件夹图标支持两种本地来源：

1. 未压缩包目录：自动选择目录顶层唯一的 `.dis.json` 或 `.dis`；若存在多个描述文件，
   必须直接指定目标文件。
2. 根描述文件：以该文件所在目录作为包根，解析相对资源和递归文档。

所有资源及递归描述文件必须位于包根以内。解析器会解析符号链接后的真实路径并拒绝
越界引用。输入空路径或点击“内置示例”会切回仓库自带的三层演示包。
桌面版还接受 `--demo <path>`、`--demo=<path>` 或单个位置参数；移动端由宿主通过
`SpikeApp.initialDemoPath` 注入应用沙箱内路径，或在运行界面中输入路径。

“画布尺寸”提供两种预览策略：

- `适应窗口`：根画布按预览区进行 contain 等比缩放，用于验证随动布局。
- `固定 1:1`：根画布保持描述文件声明的逻辑宽高并在预览区居中；窗口小于画布时发生
  视口裁切，但画布尺寸不变，并可通过拖动查看未显示区域。

两种策略只影响根预览视口，不改写描述文件，也不改变嵌套文档的 `fit=none` 语义。

### Windows

```powershell
cd examples/placeholder_demo
flutter build windows --debug
flutter run -d windows
```

### Android

```powershell
cd examples/placeholder_demo
flutter build apk --debug
adb install -r build/app/outputs/flutter-apk/app-debug.apk
```

### macOS 与 iOS

```bash
source "$HOME/.config/facetwire/flutter.env"
cd examples/placeholder_demo
export NO_PROXY="${NO_PROXY:+$NO_PROXY,}localhost,127.0.0.1,::1"
export no_proxy="${no_proxy:+$no_proxy,}localhost,127.0.0.1,::1"
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" pub get
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" analyze
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" test
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" build macos --debug
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" build ios --simulator --debug
```

iOS 真机使用 `flutter devices` 获取设备 ID，然后执行
`flutter run -d <device-id>`；签名 Team 与 Bundle ID 保持该 Mac 上已验证的配置。

### 手工检查表

| ID | 检查项 | 预期结果 |
| --- | --- | --- |
| CC-01 | 点击 Playground 顶栏文章图标 | 打开 `FacetWire Rich Media Showcase` |
| CC-02 | 查看层级 | 显示 3 recursive documents、12 zones |
| CC-03 | 查看递归位置 | Level 2 位于根坐标 900,350；Level 3 位于 Level 2 坐标 85,250，均为 `fit=none` |
| CC-04 | 查看覆盖 | Text/Image/GIF/Chart/Video 与递归文档按 z 顺序相互覆盖 |
| CC-05 | GIF | Level 1 与 Level 3 GIF 自动连续播放，无静态 PNG 冒充 |
| CC-06 | 类型调试滑杆 | 只改变对应 Text、Image、GIF、Chart 或 Video 类别 |
| CC-07 | 当前层 opacity=0 | 当前层完全不可见；若为 document，则整个子树消失并直接透出父 Canvas，不残留白板 |
| CC-08 | 当前层 opacity=1 | 当前层完全不透明，仍保留素材自身 Alpha |
| CC-09 | 点击嵌套层 | 当前层信息显示该 Zone 的原始 bounds |
| CC-10 | 缩放/旋转设备 | 根 viewport 适配屏幕，嵌套文档内部坐标关系不改变 |
| CC-11 | 视频海报与播放 | 初始显示海报；点击播放后加载本地 MP4，暂停/继续有效 |
| CC-12 | 图表能力标识 | 显示描述数据生成的柱状图；明确标注为 Playground 预览而非已完成 Chart Renderer 插件 |

### 本章检查

- 四个平台使用相同 Dart、JSON、PNG 和 GIF 文件。
- 平台差异限于构建、签名和宿主静态/动态注册方式。
- 手工检查同时覆盖视觉、动画、交互和坐标四类风险。

## 6. 当前边界与下一步

当前 Flutter 页面是标准目录包的跨平台视觉宿主，用于验证组合关系。综合演示中的
Chart 是宿主原生预览，不是已完成的 Chart Renderer 插件；Video 则复用现有 Core Media
播放路径。C 插件的 Host
Service/DisplayList 路径由原生契约测试验证。下一步生产化集成应让 Playground 的内容
投影器把同一 Zone 转成 `fw_text_renderer_request_v1` 或
`fw_image_renderer_request_v1`，再把真实 DisplayList/纹理结果桥接到 Flutter，而不是长期
保留两套独立渲染逻辑。

Core Image 0.1 只处理单一静态/动态资源、placement 和整体 opacity。渐变透明度、羽化、
通道、色阶、饱和度、滤镜、多源合成和 AI 派生图层仍属于后续 Image Composition
Profile，不应继续扩张当前 ABI。

### 本章检查

- 已明确演示宿主与生产 Native Bridge 的差距。
- Core Image 与专业 Image Composition 的边界保持不变。
- 下一步可以新增投影/桥接而不破坏现有文件、插件 ABI 或四平台 UI。
