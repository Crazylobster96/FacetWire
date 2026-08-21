# Windows / Android 构建记录（2026-08-21）

## 构建基线

| 项目 | 结果 |
|---|---|
| Flutter | 3.47.1 |
| Framework commit | 6655482ec06e547f90abf8ae7590466f4415978d |
| Engine commit | 5d531788691ec3404cac0cee66ead4007b177363 |
| Dart | 3.13.1 |
| JDK | Android Studio JBR 21.0.10 |
| Android compile / target SDK | 36 / 36 |
| Android min SDK | 24 |
| ABI | arm64-v8a |
| Build mode | debug |

## 验证结果

| 检查 | 状态 | 证据 |
|---|---|---|
| FacetWire 根 CTest | 通过 | 2/2 |
| Playground Native CTest | 通过 | 1/1 |
| flutter analyze | 通过 | No issues found |
| flutter test | 通过 | 3/3 |
| flutter build apk | 通过 | app-debug.apk |
| APK package | 通过 | org.facetwire.facetwire_playground_ui_spike |
| APK application label | 通过 | FacetWire Playground |
| APK native ABI | 通过 | arm64-v8a |
| APK signature | 通过 | APK Signature Scheme v2 |
| Signer | 测试限定 | C=US, O=Android, CN=Android Debug |
| 真机安装 | 未执行 | 当前 Windows 主机未连接 adb 设备 |

## 本地交付物

文件名：

~~~text
FacetWire-Playground-UI-Spike-0.1.0-arm64-debug.apk
~~~

字节数：77129473

SHA-256：

~~~text
ADC79C4F55D88EC07FF89361130C393B5197C4FB1E452214D82A002900992AE5
~~~

## 范围说明

当前 APK 是 UI 技术 Spike，使用 DemoRuntimeClient 驱动显示列表，验证画布、
透明度、绘制和交互。它不代表 Android 原生 FacetWire 插件静态注册已完成。
Debug 证书不能用于应用商店或正式分发。

## 本章一致性检查

- 包架构与提供给用户的 arm64 说明一致。
- minSdk、targetSdk 和锁文件一致。
- 签名成功与发布就绪被明确区分。
- 未连接真机，因此不把静态 APK 校验误报为真机测试通过。
