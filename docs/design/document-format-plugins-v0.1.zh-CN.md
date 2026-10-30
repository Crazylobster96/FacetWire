# 文档格式插件 0.1：需求与设计

状态：首版参考实现；并非 Office 完整兼容声明。使用说明见 [用户手册](../guides/document-import-user-guide.zh-CN.md)。

## 范围与验收

- DOCX：本地 OOXML 包，正文段落、直接字符格式、基础段落样式、标题、图片、简单表格、显式分页。保留内容顺序和来源定位。
- PDF：固定页面、尺寸、按需栅格预览；不重排、不执行 JavaScript、不主动打开链接。初版不提供密码 UI、OCR、表单编辑或原格式保存。
- XLSX/PPTX：只保留格式标识；发现这类文件必须返回 unsupported，不能伪装 DOCX。
- 旧 DOC/XLS/PPT、加密 Office、宏文件不支持。
- Demo 可从文件选择器或路径打开，提供样本、状态、诊断、缩放、页导航；错误不能让进程退出，替换文档和离开页面须释放资源。

## 分层

`plugins/document_format` 是不依赖 Flutter 的 Dart 参考格式插件包，包含注册接口、预算、不可变文档 IR、ZIP/OOXML 解析；本版本不是可由 C runtime 动态加载的原生格式插件，不显示 Native Parser PASS。

`bindings/flutter/document_view` 负责 Flutter 文本/表格测量、内容绘制及 PDFium 后端适配。PDF 解析与页面绘制委托 pdfrx/PDFium，不重新发明 PDF 引擎。平台打包状态应分别验证，尤其不默认声明 visionOS 可用。

DOCX 测量后的块经宿主桥接到现有原生 `facetwire.layout.flow`；Flow 不读取 DOCX/XML。0.1 正常段落、图片和简单表格按整块分页；超过一页的段落/表格按测量后的行/表格行边界切成视觉片段，保持同一个来源 ID。单行表格本身超过一页时按高度切片。不静默裁掉正文；精细段内分页、孤行控制、重复表头、混合节尺寸和 Word 精确字体回退属于后续版本。

Demo 仅提供文件授权、路由与控制，不持有 OOXML 解析逻辑。沿用 `allowSnapshotting: false` 路由约束。

## 共用合同与 Forge 兼容

- 格式 ID：docx、pdf、xlsx、pptx；插件 ID 与版本单独暴露。
- 输入：宿主选定文件的字节、名称、导入预算；没有隐式网络权限。
- 输出：layoutKind（flow/fixed）、节点/页面、诊断及源 SHA-256。
- 节点来源：源摘要、包内 part、结构位置；同一文件重复导入 ID 稳定，文件改变后必须重新建立映射。不可承诺编辑前后 ID 永久稳定。
- 不修改输入文件；原文件仍是保存/往返编辑的权威来源。IR 是导入视图，不是无损 DOCX/PDF 替代品。
- 固定页不是元素级编辑模型；PDF 内文提取与编辑映射以后独立扩展。

## 安全与资源

输入字节、ZIP 条目数/展开大小、XML 大小/深度、模型节点数、图片字节、PDF 页数和单页像素均有上限。拒绝加密 ZIP、ZIP64、重复/非法包路径、DTD/实体声明；不解压到用户文件系统。外部关系和不支持的元素产生诊断。PDF 原生后端仍是进程内依赖，不能宣称已经提供恶意文档进程隔离。

默认预算：输入 32 MiB、ZIP 展开 64 MiB、单 part 8 MiB、条目 2048、模型块 10000、PDF 500 页/单页 400 万像素。DOCX 的原生桥接有独立的 10000 块/500 页上限。PDF 在页数检查之后显式驱动渐进式元数据加载，再按需渲染一页；渲染/关闭串行化，旧异步结果不得覆盖新文档。

### DOCX 图片资源合同

- 格式插件每次导入建立独立资源池，按包内路径缓存读取结果、按 SHA-256 合并内容相同的图片。`DocumentImageAsset` 只复制一次不可变编码数据；各 `DocumentImage` 引用共享资源，仍保留独立来源 ID、尺寸和替代文本。
- `maxImageResourceBytes` 默认 32 MiB，仅累计保留的去重编码资源；`maxImageSourcePixels` 默认单图 1600 万像素。不再按每次引用累加原图像素、触发旧的 3200 万总量拒绝。
- 单图尺寸或保留资源预算超限时，仅该图片显示占位，`image_unavailable` 诊断包含实际值、限制和资源路径；正文继续导入。ZIP、实际解压量、CRC、输入与 XML 安全检查依然可以拒绝整个文件，不能作为图片降级吞掉。
- `maxImageDecodePixels` 默认每个解码目标 100 万像素，必须大于零。显示适配层按实际块尺寸、页面缩放和设备像素比计算目标，使用 `ResizeImagePolicy.fit` 保持比例且不放大原图；相同资源和目标尺寸复用 Flutter 图片缓存。只有当前页构建图片控件，不预解码整份文件。
- 解码目标限制约束保留的输出图像，不保证原生解码器临时内存或 GIF 全动画帧总量；仍保留原始尺寸、输入大小等独立防线，仍不属于恶意文件安全沙箱。

## 验证

包含正常中英文 DOCX/PDF、段落/图片/表格/分页、损坏文件、ZIP 路径/预算/DTD、外部关系、稳定来源 ID、未知格式、资源释放和页面错误。原生通用 Flow 桥接增加输入校验、分页和空输入测试。真实 PDF 后端冒烟与 Flutter widget 测试分开报告。

2026-09-03 Windows 验证：原生 CTest 17/17、格式包测试 7/7、Demo Flutter 测试 36/36；格式包/显示包/Demo 静态检查无问题。Windows Release 构建与安装、Dart FFI 冒烟通过，安装目录确认包含 `pdfium.dll`、NativeAssetsManifest 和两个示例文件。DOCX 结构校验通过；DOCX 两页与 PDF 两页的离屏预览均已查看。PDF 测试调用真实 PDFium，覆盖翻页、透明像素、页数预算和重复关闭；测试只显式提供缓存目录，不替换解析/渲染后端。

尚未标记通过：macOS/iOS/Android/visionOS 的本次变更，以及 Windows Release 的人工文件选择器/GPU 导航验收。离屏图像与构建通过不替代这些设备测试。

2026-09-03 图片预算修复复验：已先复现旧 `image_limit`（同一张 1200 万像素图片引用三次，以及三张原始总像素超过 3200 万的不同图片），修复后格式包 13/13、Demo Flutter 41/41、原生 CTest 17/17 和三处静态检查通过。新增资源别名去重、不可变数据、局部超限占位、重新打开预算重置、真实 PNG 受限解码与共享缓存键测试；连续 DOCX/PDF 切换测试也通过。Windows Release 与 FFI 冒烟通过；为保留运行中的旧 Demo，修复产物输出到 `examples/placeholder_demo/build/windows-image-budget-fix/runner`。用户触发问题的原文件尚待人工复验。

参考：[pdfrx](https://pub.dev/packages/pdfrx)、[Dart XML](https://pub.dev/packages/xml)、[archive](https://pub.dev/packages/archive)。依赖版本锁定，不在运行时下载文档内容。
