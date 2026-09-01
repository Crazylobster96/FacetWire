# Core Chart Renderer 0.3 / 核心图表渲染器 0.3

`org.facetwire.reference.core-chart-renderer` implements the portable
`facetwire.renderer.chart.v1`、`facetwire.renderer.chart.elements.v1` 与
`facetwire.renderer.chart.presentation.v1` C ABI. It accepts a host-normalized chart model
and emits normalized vector primitives through a host-owned sink.

0.3 支持：

- 分组/横向/堆叠/百分比柱状图、折线图、面积/堆叠面积图；
- 散点、气泡、饼、环形、雷达、热力、仪表盘；
- 箱线、直方、瀑布、漏斗、K 线、时间序列和组合图；
- 独立类别标签、数值/百分比标签与图例；
- 多系列、缺失值、显式颜色与显隐；
- `VisualTransform` 的 contain/cover/fill、0/90/180/270° 内容旋转；
- `opacity`，其中 `1=完全不透明`、`0=完全透明`；
- 透明留白，不绘制隐式画布背景；
- 聚合语义、数据节点命中测试、稳定 Cache Key；
- 类别、系列、点数与命令数预算；
- Sink 失败后 `end_chart` 平衡和无跨 ABI 内存所有权。
- `facetwire.renderer.chart.elements.v1` 元素树、稳定 canonical ID；
- 标题、坐标轴、图例、系列、datum 和标签的展示覆盖；
- 元素透明度、颜色、平移、缩放、旋转、zOffset 与按需提升。
- 对比柱图、分区折线、范围面积、密度热力图、词云与南丁格尔玫瑰图；
- Auto/Light/Dark/Business/Academic/High Contrast 主题；
- Auto/All/Important/None 标签治理、标签预算、确定性碰撞避免；
- Auto/Bottom/Right/Hidden 图例位置与目标尺寸驱动的自动布局。

元素图层使用说明：
[Chart Element Layering 用户手册](../../docs/guides/chart-element-layering-user-guide.zh-CN.md)。
完整 0.3 使用说明：
[Chart Visualization 0.3 用户手册](../../docs/guides/chart-visualization-expansion-user-guide.zh-CN.md)。

CSV、Excel、数据库和网络数据不是 Renderer 的职责。宿主或未来的 Data Source
Adapter 必须先把它们规范化为 `fw_chart_category_v1`、`fw_chart_series_v1` 和
`fw_chart_value_v1`。Chart 插件不访问文件、网络或平台 UI。

The renderer never parses CSV/Excel, creates a background, retains request
pointers, or returns Flutter/Qt/platform objects. Custom axes, streaming data,
graph/mind-map formats, maps, flame graphs, retained host-layer widgets, and
data write-back remain later profiles.

Build and test:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
