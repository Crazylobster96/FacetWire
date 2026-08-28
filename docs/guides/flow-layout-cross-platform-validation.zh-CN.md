# Flow Layout 0.1 跨平台验证指南

本指南用于验证 FacetWire Playground 与 visionOS 原生宿主是否真正调用
“org.facetwire.reference.flow-layout”，而不是只显示 Dart/Swift 模拟结果。

## 1. 验证范围

同一份 C 实现通过两条平台接入路径使用：

- Windows、macOS、iOS、Android：Flutter Native Assets CBuilder 自动编译并打包；
- visionOS：XcodeGen 工程静态编译 facetwire_ui_spike.c 与
  plugins/flow_layout/src/plugin.c。

Flutter 页面顶栏的流式布局图标打开“Flow Layout 0.1 验证”。visionOS 页面直接包含
Flow Layout 区域。设备验收必须看到 Native PASS；Native FAIL 代表当前运行的是
Dart 测试回退，不能记为平台通过。

## 2. 三层标准场景

未压缩演示包位于：

examples/placeholder_demo/assets/documents/flow-layout-recursive-demo.agscene/

它使用相对路径形成三层递归文档。每层包含一个 continuous Flow，逻辑页面为
600 × 700，顺序均为“文本—对象—文本”。

| 场景 | 对象行为 | 期望 Layout Plan |
|---|---|---|
| Level 1 | PNG 图片对象 | text, object, text；3 fragments |
| Level 2 | 嵌套文档中的 PNG 图片对象 | text, object, text；3 fragments |
| Level 3 | 未知对象类型 | text, placeholder, text；3 fragments |
| virtual-pages 探测 | 超出 0.1 实现切片 | composeStatus=11、complete=false、0 fragments |

Level 3 的 Placeholder 必须保留对象边界，后一段文字不能坍缩到未知对象的位置。

## 3. 通用手工验收

1. 启动 App，打开“Flow Layout 0.1 验证”。
2. 确认合同区显示 Native PASS、Complete PASS、Balanced PASS、Status 0 和
   3 fragments。
3. 依次选择 Level 1、Level 2，确认图片位于两段文字之间。
4. 选择 Level 3，确认中间出现橙色“Placeholder / 后备占位”。
5. 切换“探测 virtual-pages”，确认页面明确显示 UNSUPPORTED 和状态 11；
   关闭开关后恢复 Level 3。
6. 拖动“预览不透明度”，确认 1 为完全不透明、0 为完全透明，片段几何不变化。
7. 在“随窗口适配”和“固定 1:1”之间切换；固定模式保持 600 × 700 逻辑尺寸，
   只允许平移/缩放查看，不重排 Layout Plan。
8. 记录平台、设备/模拟器、系统版本、构建 commit、上述每项结果和截图。

## 4. Windows

在仓库根目录执行正式 Playground 的确定性构建脚本：

    powershell -ExecutionPolicy Bypass -File examples\placeholder_demo\scripts\build-windows.ps1

脚本会进入 Visual Studio Developer Environment，使用 Ninja 构建统一 C Bridge，
运行 CTest、Flutter analyze/test，并生成保留兼容文件名的正式入口：

    examples/placeholder_demo/build/windows-ninja/runner/facetwire_placeholder_demo.exe

若出现 stddef.h、stdint.h、windows.h 等 SDK 头缺失，应调用
`msvc-build-environment` skill。脚本还会在首次构建时确定性解压 media_kit 所需的
libmpv 与 ANGLE，避免 Ninja 在依赖文件生成前提前链接。

### 当前自动验证记录（2026-08-28）

- 统一原生 C Bridge CTest：PASS（4/4，其中 Playground Bridge 同时覆盖 Placeholder 与 Flow）。
- Flutter analyze：PASS（0 issues）。
- Flutter test：PASS（15/15，包含 Native Assets 双合同集成测试）。
- Windows Release Runner：PASS。
- Dart FFI smoke：PASS（8 commands，action=2）。

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
App，执行第 3 节手工验收。

iOS 真机仍需使用本地 Apple Developer Team 签名；Native Assets 会静态链接同一 C
实现，不依赖任意外部动态插件加载。

## 7. visionOS

    ./scripts/validate-visionos-spike-macos.sh

更新后的 XcodeGen 工程直接编译 Flow Layout 插件。XCTest 应包含：

- 三个平衡 fragments；
- Level 3 未知对象降级为 Placeholder；
- virtual-pages 返回状态 11。

在 visionOS Simulator 或 Vision Pro 真机中，Flow 区域应显示绿色
“PASS · native Flow”。真机签名使用测试者自己的 Apple Developer Team。

## 8. 结果判定

平台只有同时满足以下条件才可标记为 PASS：

- 构建成功；
- 自动测试成功；
- UI 显示 Native PASS；
- Level 1/2/3 与 virtual-pages 四个场景均符合预期；
- 透明度、随窗口适配和固定 1:1 交互符合预期。

仅 Dart 回退、仅 CTest、仅模拟器截图或仅旧版 Placeholder 通过，都不能替代完整平台
验收。