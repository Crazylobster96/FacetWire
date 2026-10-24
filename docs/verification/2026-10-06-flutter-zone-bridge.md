# Desktop native-to-Flutter zone-profile bridge candidate

2026-10-06，Windows/MSVC Release，固定 FacetWire 源码与合成 Status Tile 内容；没有真实用户文档、外部服务或密钥。

- 单独构建 `FACETWIRE_BUILD_FLUTTER_ZONE_BRIDGE=ON` 的 bridge 与 `facetwire_status_tile_renderer.dll`。相关 CTest 3/3：原生桥准确检查插件 ID、能力、v1 接口和字节缓冲区；错 ID、缺能力、过小缓冲区及无效路径均拒绝；新回归项把插件单独放入中文、较长的安装路径再加载。
- `facetwire.plugin_manifests.contract` 1/1，已明确加入新参考清单的身份、能力与接口核对；使用现有 `scripts/package-renderer.py` 产出仅含该 DLL、原清单与许可证的独立 ZIP。在 Pillow 的**临时**插件管理目录，以新能力槽和现有原生 Probe 完成实际安装、清单/descriptor/制品摘要核对；未改变用户插件目录。
- Flutter/Dart 相关旧新测试 6/6，包含对编译 DLL 的真实 FFI 读取、双方 SHA-256 固定、能力描述 JSON 解析与可见文字 Widget；错摘要和错插件身份拒绝。`dart analyze lib test` 无问题。

在一次完整链测试中，构建目录 DLL 可绘制、安装目录同一 DLL 却返回 `FW_STATUS_NOT_FOUND`。实际路径约 255 字符。FacetWire Core 的 Windows 加载层已将绝对 UTF-8 路径转换为扩展长度 Win32 路径；修复后，Flutter 从 Pillow 临时管理器中的**已安装副本**再次运行 2/2 并可见文字 Widget。上述长路径 CTest 固化该故障，不以单次手测替代。

原有 `facetwire.runtime.discovery`、Unicode discovery 与七类独立原生 Renderer 动态加载回归 9/9，通过同一已修复 Core；未重新运行全部 FacetWire 测试或 Apple 编译。

范围严格限于自定义 Zone 的受限静态声明式视图。旧原生渲染 DLL 未实现新接口，不会自动映射；任意绘图、动画、交互与完整语义树尚不支持。Pillow Viewer 的实际配置/启动消费、Mac 编译/iPhone 视觉尚未验证，因此不能宣称 U-06 或阶段完成。
