# FacetWire Flutter 本地工具链与移动端构建

## 1. 目标

本项目不依赖机器 PATH 中碰巧存在的 Flutter。仓库根目录的
toolchains.lock.json 固定 Flutter、Dart、Engine、Gradle 和 Android
关键工具版本；引导脚本验证实际 Git 提交。后续人工或 Codex 会话均应使用
FACETWIRE_FLUTTER_ROOT 下的可执行文件，禁止对这套 SDK 执行 flutter upgrade。

当前锁定基线：

| 项目 | 版本 |
|---|---|
| Flutter | 3.47.1 |
| Flutter framework commit | 6655482ec06e547f90abf8ae7590466f4415978d |
| Flutter engine commit | 5d531788691ec3404cac0cee66ead4007b177363 |
| Dart | 3.13.1 |
| Gradle | 9.3.1 |
| Android min / target SDK | 24 / 36 |
| Android NDK | 28.2.13676358 |
| 推荐 JDK | 21 |

官方参考：

- Flutter 手动安装：https://docs.flutter.dev/install/manual
- Flutter PATH 配置：https://docs.flutter.dev/install/add-to-path
- Android APK 构建：https://docs.flutter.dev/deployment/android
- iOS 开发环境：https://docs.flutter.dev/platform-integration/ios/setup
- iOS 归档与 IPA：https://docs.flutter.dev/deployment/ios
- Android sdkmanager：https://developer.android.com/tools/sdkmanager

## 2. Windows：一次安装，后续 Codex 可复用

先安装 Git 和 Android Studio。Android Studio 自带的 JBR 21 可作为 JDK。
在 FacetWire 仓库根目录运行：

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap-flutter.ps1 -ToolchainsRoot D:\AICode\_toolchains -AndroidSdk "$env:LOCALAPPDATA\Android\Sdk" -Jdk "C:\Program Files\Android\Android Studio\jbr" -PersistEnvironment -UseChinaMirror
~~~

PersistEnvironment 会把 FACETWIRE_FLUTTER_ROOT 写入当前用户环境，供以后新开的
Codex 或终端读取。当前会话如需立即使用：

~~~powershell
$env:FACETWIRE_FLUTTER_ROOT = "D:\AICode\_toolchains\flutter"
& "$env:FACETWIRE_FLUTTER_ROOT\bin\flutter.bat" doctor -v
~~~

若网络可直接稳定访问 Google Storage，可省略 UseChinaMirror。镜像只负责分发；
锁定的 framework/engine commit 和 Gradle SHA-256 仍用于防止版本漂移。

### 2.1 构建 Android 真机测试包

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-android.ps1 -Mode debug -TargetPlatform android-arm64 -UseChinaMirror
~~~

输出在 dist/android。dist 已被 Git 忽略，不会把大体积二进制误提交到源码仓库。
debug APK 使用 Android Debug 证书，只用于侧载测试。Google Play 或正式分发必须
另配受保护的 release keystore，且绝不能把密钥、口令或 key.properties 提交。

安装到已开启 USB 调试的设备：

~~~powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" devices
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r .\dist\android\FacetWire-Playground-UI-Spike-0.1.0-arm64-debug.apk
~~~

如果设备不是 arm64-v8a，应分别使用 android-arm 或 android-x64 构建；不要用
一个包含全部 ABI 的巨大调试包掩盖架构问题。

## 3. macOS：固定 SDK、Xcode 与模拟器

Mac 必须安装完整 Xcode，而不只是 Command Line Tools。首次使用应启动一次
Xcode，完成组件安装并接受许可。还需要 CMake；可通过 Homebrew 安装。
然后：

~~~bash
git clone https://github.com/Crazylobster96/FacetWire.git
cd FacetWire
chmod +x scripts/bootstrap-flutter-macos.sh scripts/validate-mobile-macos.sh
FACETWIRE_USE_CHINA_MIRROR=1 ./scripts/bootstrap-flutter-macos.sh
source "$HOME/.config/facetwire/flutter.env"
./scripts/validate-mobile-macos.sh
~~~

引导脚本默认安装到：

~~~text
$HOME/Library/Application Support/FacetWire/toolchains/flutter
~~~

它会生成 $HOME/.config/facetwire/flutter.env。后续 Codex 每次开始工作先 source
该文件，然后始终调用：

~~~bash
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" <command>
~~~

validate-mobile-macos.sh 会验证 SDK commit，执行根运行时 CTest、UI Native
桥接 CTest、Flutter analyze/test、macOS Debug 构建和无签名 iOS Simulator
构建。

## 4. iPhone / iPad 真机测试

真机二进制必须由 Mac 上的 Xcode 使用你的 Apple Development 身份和 provisioning
profile 签名。Windows 产物不能替代此步骤。

1. 用 USB 或已配对的无线连接连接设备，在设备上信任 Mac，并开启 Developer Mode。
2. 打开 spikes/playground_ui/ios/Runner.xcworkspace。
3. 在 Runner > Signing & Capabilities 选择你的 Team。
4. 如 org.facetwire.facetwirePlaygroundUiSpike 不唯一，改为你账号下唯一 Bundle ID。
5. 不要把 Apple ID、证书、私钥、profile 或临时签名配置提交到 Git。
6. 确认设备可见并直接运行：

~~~bash
source "$HOME/.config/facetwire/flutter.env"
cd spikes/playground_ui
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" devices
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" run -d <device-id>
~~~

若账号具有可导出的 development provisioning profile，可生成开发测试 IPA：

~~~bash
"$FACETWIRE_FLUTTER_ROOT/bin/flutter" build ipa --release --export-method development
mkdir -p ../../dist/ios
cp build/ios/ipa/*.ipa ../../dist/ios/
shasum -a 256 ../../dist/ios/*.ipa
~~~

仅使用免费 Personal Team 时，Xcode/Flutter 直接运行到已登记设备通常比导出 IPA
更可靠；若 IPA 导出被 provisioning 权限拒绝，应记录为签名条件未满足，而不是
关闭校验或提交个人凭据。

## 5. 后续 Codex 会话必须遵守的约束

1. 先读取 toolchains.lock.json。
2. 先验证 Flutter SDK 的 HEAD 与锁文件完全一致。
3. 使用绝对路径调用 Flutter，不把 PATH 作为唯一发现机制。
4. 不执行 flutter upgrade，不自动改 channel，不静默更新 Gradle wrapper。
5. 先运行 analyze 和 test，再构建平台包。
6. 签名材料仅保留在系统钥匙串、Xcode 或本地忽略文件。
7. dist、build、.dart_tool 和平台 ephemeral 目录不提交。
8. 产物交付同时给出包名、版本、ABI、签名类型、字节数和 SHA-256。

## 6. 故障定位

| 现象 | 处理 |
|---|---|
| 找到错误 Flutter | 检查 FACETWIRE_FLUTTER_ROOT 和 commit，不使用 PATH 中的 flutter |
| flutter doctor 报 Xcode 不完整 | 安装并启动完整 Xcode，运行 Xcode 的首次设置 |
| iPhone 不可见 | 检查信任、Developer Mode、线缆/配对和 flutter devices |
| IPA 无法安装 | 检查 Team、Bundle ID、设备 UDID、证书和 provisioning profile |
| Android licenses 未接受 | 使用 sdkmanager --licenses 或 flutter doctor --android-licenses |
| Google Storage 过慢 | 显式启用中国镜像，但保留版本锁与哈希验证 |
| APK 安装报 ABI 不匹配 | 针对设备重新选择 android-arm64、android-arm 或 android-x64 |

## 7. 本章一致性检查

- SDK 版本来源只有 toolchains.lock.json，不存在文档与脚本各自漂移的版本源。
- Windows、macOS 都通过相同 framework commit 验证，开发体验保持一致。
- 模拟器构建不冒充真机签名验证；Windows 不宣称能够产出 iOS 安装包。
- Debug APK 与 development IPA 均明确限定为测试用途。
- 产物目录与签名材料均不会进入源码提交。
