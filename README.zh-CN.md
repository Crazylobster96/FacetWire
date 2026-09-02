# FacetWire

[English](README.md) | **简体中文**

**一次编写渲染插件，连接所有平台。**

FacetWire 是一套面向 AI 智能体、支持跨平台富媒体渲染的可移植插件协议与运行时。它在宿主和独立开发的能力之间定义稳定的 C ABI；这些能力包括文本排版、图片、视频、字幕、图表、控制组件、文档分页和格式解析器。

> 项目状态：**0.1 启动阶段 / 实验性**。当前 ABI 和智能体场景包（Agent Scene Package，ASP）目录规范刻意保持较小范围；在 1.0 一致性测试套件发布前，接口尚不稳定。

## 设计目标

- 一套源代码覆盖 Windows、Linux、macOS、iOS 和 Android。
- 动态注册、静态注册、进程隔离、远程调用以及未来的 WebAssembly 传输共用相同的逻辑插件协议。
- 内存和渲染资源由宿主持有，不允许平台对象穿过 ABI 边界。
- 保持开放核心，同时允许采用独立许可证的专有格式和编解码器插件。
- 确定性的能力发现、版本协商和诊断机制。

## 仓库结构

```text
include/facetwire/                 公共 C ABI 与运行时 API
src/                               可移植运行时实现
spec/                              规范性与实验性协议
spec/schema/                       可机器读取的清单与场景 Schema
docs/                              架构与项目政策文档
examples/hello_plugin/             最小静态注册插件示例
examples/placeholder_demo/         Windows/macOS 真实渲染器演示
examples/documents/                合规的未压缩 .agscene 测试夹具
plugins/text_renderer/             Text Renderer 0.1 参考实现
plugins/core_image_renderer/       图片与动态图片 Renderer 参考实现
plugins/core_media_renderer/       音频与视频 Renderer 参考实现
plugins/core_chart_renderer/       Core Chart Renderer 0.3 参考实现
plugins/hierarchical_chart_renderer/ Hierarchical Chart Renderer 0.1 参考实现
plugins/flow_layout/               Flow Layout 0.1 实验性参考实现
spikes/playground_ui/              Windows/macOS/iOS/Android 共用演示宿主
plugins/placeholder_renderer/      Placeholder 后备 Renderer 参考实现
tests/                             ABI 与一致性冒烟测试
```

## 构建

FacetWire 要求支持 C11 的编译器和 CMake 3.21 或更高版本。

```sh
cmake -S . -B build -DFACETWIRE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

如果已安装 Ninja，可以使用仓库提供的预设：

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

构建并运行 Windows/macOS 演示请参阅 [`examples/placeholder_demo/README.md`](examples/placeholder_demo/README.md)。Text/Image/GIF 三层递归演示及其四平台验证矩阵记录在[核心内容 Renderer 演示验证指南](docs/guides/core-content-renderers-demo-validation.zh-CN.md)中。
### Flow Layout 当前实现状态

`org.facetwire.reference.flow-layout` 已提供公共 C ABI、插件 Manifest、确定性 Layout Plan，以及 continuous、virtual-pages、columns 三种模式下的 block/inline/float-start/float-end flow。当前实验切片支持文本与行内对象跨区域续排、不可拆 replacement object、四种 baseline、RTL 与逻辑浮动定位、带 margin 的矩形 exclusion、最小正文宽度清除、活动 float 预算、对象整体换栏/换页、相邻垂直 margin collapse、锚定 overlay、显式 break、有界 keepWithNext 链、keepTogether 和普通段落 orphan/widow 平衡。不能同时满足的美观约束通过 diagnostic flags 明确报告。Playground 原生投影新增“覆盖层”和“分页约束”场景；带 inline object 的段落当前保持对象原子性，在无法安全回滚精确行时报告 widow/orphan 已放宽。跨平台验收见 [Flow Layout 验证指南](docs/guides/flow-layout-cross-platform-validation.zh-CN.md)。

### Core Chart 当前实现状态

org.facetwire.reference.core-chart-renderer 已提供 facetwire.renderer.chart.v1、
facetwire.renderer.chart.elements.v1 和 facetwire.renderer.chart.presentation.v1，
支持规范化类别/系列/复合值输入、原有 17 种图表，以及对比柱图、分区折线、范围面积、
密度热力图、词云、南丁格尔玫瑰图；同时支持 6 种主题、自动布局、标签治理、统一 VisualTransform、透明度、
透明留白、语义、数据节点命中测试、资源预算和稳定 Cache Key。CSV/Excel 仍属于后续
独立 Data Source Adapter，不由 Renderer 直接解析。正式 Playground 已提供真实 Native
Asset Chart 验证页和 30 个 Gallery 入口，并可选择标题、坐标轴、图例、系列、datum 与标签，独立调整透明度、
颜色、平移、缩放、旋转、zOffset 和提升状态。
图例已按“容器 → 图例项 → 色标/标签/可选数值”统一拆分，既可整体调整一个图例项，
也可只修改其中的单个子部件。

`org.facetwire.reference.hierarchical-chart-renderer` 通过独立的
`facetwire.renderer.hierarchical-chart.v1` 支持矩形树图、旭日图和 Packed Bubble，节点采用稳定的
父先子后模型。参见[0.3 需求](docs/requirements/chart-visualization-expansion-requirements-v0.3.md)、
[详细设计](docs/design/chart-visualization-expansion-detailed-design-v0.3.md)和
[中文用户手册](docs/guides/chart-visualization-expansion-user-guide.zh-CN.md)，以及
[图表视觉设计系统 0.4](docs/design/chart-visual-design-system-v0.4.zh-CN.md)。

## ABI 模型

宿主取得 `fw_plugin_api_v1` 函数表，验证其结构大小和 ABI 版本，然后将插件注册到运行时。动态加载插件导出 `facetwire_plugin_query` 符号；受限平台可以将同一查询函数直接传给 `fw_runtime_register_static`。

桌面平台和受控 Android 宿主也可以把已经授权的绝对库路径传给 `fw_runtime_load_dynamic`。Core 不扫描插件目录，也不负责信任决策。宿主可以确定性地枚举和选择能力提供者，再查询带版本的接口。

内存不得由 ABI 另一侧释放。字符串使用 UTF-8 字节区间，不假定以 NUL 结尾。每个可扩展结构都以 `struct_size` 和 ABI 版本开头。

进一步阅读：[0.1 插件协议](spec/plugin-contract-v0.1.md)、实验性的[插件清单规范](spec/plugin-manifest-v0.1.zh-CN.md)、[ASP 目录规范](spec/agent-scene-package-directory-v0.1.md)、[核心内容规范](spec/core-content-profile-v0.1.zh-CN.md)、[VisualTransform 规范](spec/visual-transform-v0.1.zh-CN.md)、[流式内容规范](spec/flow-content-profile-v0.1.zh-CN.md)和[架构概览中文版](docs/architecture.zh-CN.md)。

内容插件和布局能力的设计文档包括：

- [Text Renderer 需求](docs/requirements/text-renderer-requirements-v0.1.md)与[函数级详细设计](docs/design/text-renderer-detailed-design-v0.1.md)
- [Flow Layout 需求](docs/requirements/flow-layout-renderer-requirements-v0.1.md)与[函数级详细设计](docs/design/flow-layout-renderer-detailed-design-v0.1.md)
- [Flow Layout Playground 五平台验证指南](docs/guides/flow-layout-cross-platform-validation.zh-CN.md)
- [Media Renderer 需求](docs/requirements/media-renderers-requirements-v0.1.md)与[函数级详细设计](docs/design/media-renderers-detailed-design-v0.1.md)
- [VisualTransform 函数级详细设计](docs/design/visual-transform-detailed-design-v0.1.md)
- [Core Chart Renderer 需求](docs/requirements/core-chart-renderer-requirements-v0.1.md)与[函数级详细设计](docs/design/core-chart-renderer-detailed-design-v0.1.md)
- [Chart Element Layering 0.2 需求](docs/requirements/chart-element-layering-requirements-v0.2.md)、[详细设计](docs/design/chart-element-layering-detailed-design-v0.2.md)与[用户手册](docs/guides/chart-element-layering-user-guide.zh-CN.md)
- [Chart Legend Composition Profile 0.1 图例组合规范](spec/chart-legend-composition-profile-v0.1.zh-CN.md)与[英文版](spec/chart-legend-composition-profile-v0.1.md)
- [Chart Visualization 0.3 需求](docs/requirements/chart-visualization-expansion-requirements-v0.3.md)、[详细设计](docs/design/chart-visualization-expansion-detailed-design-v0.3.md)与[用户手册](docs/guides/chart-visualization-expansion-user-guide.zh-CN.md)
- [架构决策记录索引](docs/adr/README.md)

## 许可证

FacetWire 采用 [Mozilla Public License 2.0](LICENSE)。MPL-2.0 要求对受其覆盖的源文件所做修改保持公开，同时允许独立的开源或专有插件。第三方格式、编解码器、SDK、字体和测试素材保留各自许可证，并可能需要额外的专利或厂商授权。详情参阅 [LICENSE_POLICY.md](LICENSE_POLICY.md)。

## 参与贡献

提交贡献或报告安全问题前，请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)、[行为准则](CODE_OF_CONDUCT.md)和 [SECURITY.md](SECURITY.md)。
