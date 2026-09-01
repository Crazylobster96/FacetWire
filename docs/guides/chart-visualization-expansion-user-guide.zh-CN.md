# FacetWire 图表可视化 0.3 用户手册

## 1. 能力概览

Core Chart Renderer 0.3 在原有 17 种公共 Chart Kind 基础上增加统一主题、自动布局、
标签治理，并新增 6 种普通 Kind。原有 17 种通过方向、堆叠和百分比样式形成 21 个
Playground 入口；Hierarchical Chart Profile 0.1 另以独立数据模型提供 3 种层级图表。

| 分组 | Playground 类型 |
| --- | --- |
| 原有 17 个公共 Kind / 21 个入口 | 柱、横向柱、堆叠柱、百分比柱、折线、饼、面积、堆叠面积、散点、气泡、环形、雷达、热力、仪表、箱线、直方、瀑布、漏斗、K 线、时间序列、组合图 |
| 普通扩展 | 对比柱图、分区折线、范围面积、密度热力图、词云、南丁格尔玫瑰图 |
| 层级 Profile | 矩形树图、旭日图、Packed Bubble |

全部 Renderer 只绘制自身内容，未覆盖区域保持透明；`opacity=1` 表示完全不透明，
`opacity=0` 表示完全透明，中间值支持任意有限小数。

### 本章检查

- 23 种普通公共 Chart Kind、30 个 Playground 总入口与 3 种层级 Kind 的关系明确。
- 主题和标签属于展示策略，不修改数据源。

## 2. Playground 使用

打开“Core Chart / 图表”页，在图表类型区域选择类型。页面的四组展示控件为：

1. **Theme / 主题**：Auto、Light、Dark、Business、Academic、High Contrast。
2. **Legend / 图例**：Auto、Bottom、Right、Hidden。
3. **Labels / 标签**：Auto、All、Important、None。
4. **Auto layout / 自动布局**：启用时按目标尺寸、类别数和系列数计算标签密度与图例位置；关闭时保留完整密度，由宿主承担溢出处理。

图表元素检查器仍可单独选择标题、坐标轴、图例、系列、数据项和标签，并调整颜色、
不透明度、平移、缩放、旋转与提升状态。元素调整和上述四项展示策略可以同时使用。

### 本章检查

- 选择元素后不会回退主题或标签策略。
- Hidden 图例只隐藏文字，不删除系列和数据元素的稳定身份。

## 3. 普通 Chart 数据约定

Core Chart 使用宿主规范化的 category、series 与 value 模型，不直接读取 CSV 或 Excel。
数据文件应先由独立 Data Source Adapter 转换，再提交给 Renderer。

| 类型 | 字段约定 | 关键限制 |
| --- | --- | --- |
| 对比柱图 | `value` 可正可负 | 零线位于中心，正负方向相反 |
| 分区折线 | 每个 series 一个面板 | 类别轴共享，面板纵域独立 |
| 范围面积 | `minimum`、`maximum` | 必须 `minimum <= maximum` |
| 密度热力图 | `x`、`value=y`、`size=weight` | weight 必须非负，使用有界 12×8 网格 |
| 词云 | category label、`value=frequency` | 频率非负，最多布局 256 个词 |
| 玫瑰图 | category、非负 value | 等角，半径按平方根映射，面积近似表达数值 |

相同输入、目标尺寸和展示策略必须产生相同命令顺序与 Cache Key。标签太密时，Auto
会确定性抽样；All 会尽量全部绘制；Important 保留标题、首尾类别和图例；None 只保留标题。

### 本章检查

- 每种图表的视觉通道都能回溯到输入字段。
- 非法范围、负权重和非有限数在绘制前返回稳定错误。

## 4. 层级 Chart 数据约定

`facetwire.renderer.hierarchical-chart.v1` 使用父节点在前的扁平节点数组：

```c
fw_hierarchical_chart_node_v1 nodes[] = {
  { .id = FW_SV("root"), .parent_index = FW_HIERARCHICAL_ROOT_INDEX,
    .label = FW_SV("全部"), .value = 100.0 },
  { .id = FW_SV("platform"), .parent_index = 0,
    .label = FW_SV("平台"), .value = 58.0 },
  { .id = FW_SV("service"), .parent_index = 0,
    .label = FW_SV("服务"), .value = 42.0 },
};
```

- **Treemap**：矩形面积表达权重，适合空间紧凑的父子占比比较。
- **Sunburst**：角度表达权重、环层表达深度，适合观察路径和层级。
- **Packed Bubble**：圆面积表达叶节点权重，适合弱化精确比较、强调聚类。

节点 0 必须是唯一根节点；其他节点的 `parent_index` 必须小于自身索引。默认预算为
2048 节点、64 层、65536 命令。节点 ID 必须稳定，不能从当前坐标生成。

### 本章检查

- 父先子后的约束同时禁止环和悬空父节点。
- 三种布局共享验证、主题、透明和语义，但不共享错误的表格型 Chart 数据模型。

## 5. C ABI 调用

普通图表先查询 `facetwire.renderer.chart.presentation.v1`，再用同一次调用组合主题与元素覆盖：

```c
fw_chart_presentation_v1 presentation = {
  .struct_size = sizeof(fw_chart_presentation_v1),
  .theme = FW_CHART_THEME_BUSINESS,
  .legend_placement = FW_CHART_LEGEND_AUTO,
  .label_policy = FW_CHART_LABEL_AUTO,
  .auto_layout = 1,
  .max_visible_labels = 32,
  .flags = FW_CHART_PRESENTATION_USE_THEME_PALETTE |
           FW_CHART_PRESENTATION_AVOID_COLLISIONS
};

api->render(plugin, &request, &presentation,
            overrides, override_count, viewport,
            &services, &observer, &result);
```

层级图表查询 `facetwire.renderer.hierarchical-chart.v1`，调用顺序为 `validate`、`measure`、
`render`，需要时再调用 `build_semantics` 和 `hit_test`。调用完成后插件不会保留 request、
字符串、节点或 sink 指针；分配与释放不得跨 ABI 边界。

### 本章检查

- 旧 `chart.v1` 和 `chart.elements.v1` ABI 未被修改。
- Presentation 和 Hierarchical 均可由单元测试直接构造输入，无需启动 UI。

## 6. 美观与可读性建议

- 默认使用 Auto；业务仪表板选 Business，论文/报告选 Academic，辅助模式选 High Contrast。
- 饼图、环形图和玫瑰图类别建议不超过 8 个；过多类别改用排序条形图。
- 标签优先显示值与占比，但不要用重复图例占据绘图区；小屏优先 Important。
- 密度热力图只表达分布，不用于读取精确值；需要精确数据时同时提供语义或数据表。
- Packed Bubble 与词云不适合精确比较；宿主应提供语义树或详情操作。
- 图层覆盖后的最终透明度为 Canvas、Layer、Chart 与 Element opacity 的乘积。

### 本章检查

- 视觉美观规则不会改变图表语义或数值编码。
- 颜色不是唯一信息通道，高对比主题与语义信息可并行使用。

## 7. 当前边界与后续方向

本版不包含地图、火焰图、双轴、CSV/Excel 解析、交互动画以及 FacetWire-Forge 写回。
地图应进入后续 Geo Chart Profile；火焰图应进入后续 Hierarchical/Profiling 扩展；数据源
变更触发重新渲染由宿主的数据绑定层负责，Renderer 仍保持确定性只读。

### 本章检查

- 未实现能力不会被示例 UI 伪装为已经实现。
- 当前 ABI 为地图、火焰图和 Forge 保留独立 Profile/适配器扩展路径。

## 8. 验证清单

| 项目 | 自动/人工 | 通过标准 |
| --- | --- | --- |
| 原生合同 | 自动 | Core Chart、Presentation、Element、Hierarchy 全部 CTest 通过 |
| 生命周期与内存 | 自动 | 注册/查询/卸载平衡，MSVC ASan 无报告 |
| Flutter | 自动 | `flutter analyze` 与 `flutter test` 通过 |
| 主题 | 人工 | 6 种主题均可区分，未覆盖区域透明 |
| 自动布局 | 人工 | 缩放窗口时标签不严重重叠，关闭后行为可辨认 |
| 新图表 | 人工 | 9 种新增 Gallery 图形与其数据语义一致 |
| Apple 平台 | 人工+构建 | macOS、iOS、visionOS 使用同一静态 C 源码通过 |

### 本章检查

- 每项需求都有自动化证据或明确视觉验收条件。
- 五平台差异只存在于宿主集成，不改变插件数据和渲染语义。
