# DOCX / PDF 导入与预览 0.1 用户手册

## 现在能做什么

本版在现有 FacetWire Playground 中增加「Documents / 文档导入」页面，不另建 Demo App。

| 格式 | 当前实现 | 不应期待的能力 |
| --- | --- | --- |
| DOCX | 本地文件解析；段落、基础标题/字符样式、PNG/JPEG/GIF 图片、简单表格、显式分页；实际调用 Native Flow 排版 | 与 Word 像素一致、复杂浮动环绕、精确页码、完整公式/修订/目录/页眉页脚 |
| PDF | PDFium 固定页预览，保留页面几何，翻页、缩放、透明显示 | 重排、OCR、文本选取搜索、密码交互、表单编辑、元素级编辑 |
| XLSX / PPTX | 预留格式标识，返回 unsupported | 工作表、公式、幻灯片解析尚未实现 |

这是**格式插件参考实现 + 文档显示适配层**。DOCX 格式包是独立纯 Dart 库；PDF 使用独立 Flutter/PDFium 适配器。它们不是已经完成 C ABI 的动态格式 DLL，也不冒充 Native Parser PASS。页面里的 `Native Flow` 仅表示 DOCX 分页确实经过原生 Flow 插件。

## 操作步骤

1. 启动 `facetwire_placeholder_demo.exe`，点击首页工具栏的文档导入图标（提示文字「DOCX / PDF 文档导入」）。
2. 先点击「DOCX 示例」或「PDF 示例」。DOCX 样本包含中英文、图片、表格和两页内容；PDF 样本包含两页固定版面的文字与矢量图形。
3. 点击「选择文件」，或填入本机 DOCX/PDF 绝对路径后点击「打开」。移动设备优先使用系统文件选择器；Windows 路径不能直接在 iOS 使用。
4. 顶部左右箭头切换页面；缩放范围 50%–300%，放大后可滚动。透明度针对整个预览表面。
5. 展开底部「导入诊断」查看被简化或暂不支持的内容。错误不会覆盖源文件；新文件打开失败时，已打开文档仍保留。

文档页路由为 `/documents`。保留统一 `allowSnapshotting: false` 策略，避免与播放中的视频纹理转场冲突。

## 透明背景是谁控制的

「Demo 白色纸张背景」是宿主展示选项，不是格式插件强加的背景。关闭它时，未绘制区域保持透明；PDF 本身包含的白色矩形、DOCX 表格标题填色等仍属于内容，不会被删除。透明不等于应用窗口能透视桌面。

## DOCX 首版排版规则

- 文本保留基本粗体、斜体、下划线、字号、直接颜色及部分样式继承；字体统一回退到宿主可用无衬线字体。段距、缩进、主题字体和高级段落属性未完整复刻。
- 图片按独立块等比例放入正文，不复刻浮动锚点或文字环绕；缺失、超限或不支持的图形显示占位诊断。
- 表格以单元格纯文本和等宽列预览；不保留合并单元格及嵌套对象布局。
- 采用末节纸张尺寸和对称页边距。普通段落/表格整块分页；超高内容切成视觉片段，避免正文被静默裁掉。不会承诺与 Word 的分页一致。
- 列表暂时使用短横线；修订按最终正文预览，不作为审阅视图。外部图片和链接不自动访问。

## 给宿主开发者

### 图片较多的文件

重复图片会共享不可变资源，不再因「引用次数 × 原图像素」超过 3200 万而拒绝整份文档。默认允许单图 1600 万原始像素、全文件去重后图片资源 32 MiB；超过图片预算时保留文字与其他可用图片，并在对应位置及「导入诊断」中说明原因。ZIP/输入文件安全预算仍独立生效。

预览按屏幕尺寸、页面缩放和设备像素比等比例解码，单个目标默认最多 100 万像素；放大查看可能不具备原图全部细节，但不会更改原文件。重新打开文件会创建独立预算，不会累积上一次的图片用量。

### 接入接口

公共入口：

```dart
import 'package:facetwire_document_format/facetwire_document_format.dart';
import 'package:facetwire_document_view/facetwire_document_view.dart';

final registry = DocumentPluginRegistry([
  const DocxFormatPlugin(),
  const PdfFormatPlugin(),
]);
final document = await registry.open(bytes, fileName);
// composeFlow 由宿主提供；本 Demo 的 composeImportedDocument 调用原生 Flow。
final preview = DocumentPreview(
  document: document,
  composeFlow: composeImportedDocument,
);
// 移除预览后，或替换文档时，由所有者释放会话。
await document.close();
```

分层路径：

- `plugins/document_format`：格式识别、输入预算、DOCX 解析、不可变 IR 和来源定位；无 Flutter 依赖。
- `bindings/flutter/document_view`：测量/绘制、页面控制与 PDFium 适配；通过回调接入布局，不引用 Demo。
- `examples/placeholder_demo/lib/src/document_flow_client.dart`：Demo 宿主的 FFI 适配。
- `fwui_compose_document_blocks`：把测量结果交给 Flow，返回含块索引、页码和矩形的布局报告；不解析 XML/PDF。

DOCX 块的 `source` 包含原文件 SHA-256、包内 part 和结构位置；悬浮块可查看来源 ID。同一文件重复导入稳定，文件修改后不能把旧 ID 当作永久编辑锚点。布局切片保持原来源身份。

图片资源预算在 `DocumentLimits` 的 `maxImageResourceBytes`、`maxImageSourcePixels`、`maxImageDecodePixels` 中设置。资源通过 `DocumentImage.asset` 共享，`bytes` 读取接口继续兼容；自定义显示层必须遵守 `asset.maxDecodePixels`，避免直接将每一处引用按原图全尺寸解码。

本版不监控文件变化。修改源数据后重新打开文件即可重建预览。编辑、合并、撤销与原格式保存留给后续 FacetWire-Forge；当前 IR 并非无损往返格式。

## 构建与验证

Windows：在仓库根运行 `examples/placeholder_demo/scripts/build-windows.ps1`。该脚本包含格式包测试、显示包静态检查、Demo 回归、原生桥接测试与 Release 打包。

其他平台先执行：

```sh
cd plugins/document_format
dart pub get && dart analyze && dart test
cd ../../bindings/flutter/document_view
flutter pub get && flutter analyze
cd ../../../examples/placeholder_demo
flutter pub get && flutter analyze && flutter test
flutter run -d macos  # 或选择可用的 iOS / Android 设备
```

macOS Demo 的 Debug/Release entitlements 已增加 `com.apple.security.files.user-selected.read-only`，只授权读取用户选中的文件；示例文件不需要此权限。沙盒下直接输入路径不等于获得读取授权，请优先使用选择器。系统文件选择器由 `file_selector` 提供，PDF 原生库通过依赖构建流程打包；首次构建可能需要网络下载后端。未修改签名 Team 或 Xcode 项目配置。

平台验证必须分别报告。Windows 上的通过不代表 macOS/iOS/Android/visionOS 已通过；特别是 visionOS 的 PDF 原生后端需要单独确认兼容性。

可选离屏视觉 QA：设置 `FACETWIRE_DOCUMENT_QA=1` 和 `FACETWIRE_QA_FONT`（本机中英文字体绝对路径）再运行 `flutter test test/document_import_test.dart`。截图写入 Demo 的 `build/document-import-qa`，不提交系统字体。离屏截图不能替代设备上的 GPU/导航交互验证。

## 安全与限制

默认输入 32 MiB；不支持旧 DOC/XLS/PPT、宏格式、加密 Office、ZIP64、外部资源和 DTD。DOCX 导入在 Dart isolate 中运行，但不是操作系统进程隔离；PDFium 同样属于进程内原生依赖。预算降低资源耗尽风险，不构成恶意文档安全沙箱。

依赖固定版本以各包的 `pubspec.lock` 为准。分发程序时须保留 Flutter、pdfrx、PDFium 及其第三方依赖的许可证/NOTICE；项目的 MPL-2.0 不替代依赖许可证。
