# FacetWire Chart Visualization Expansion 0.3 详细设计

## 1. 分层架构

```text
Chart Model ── chart.v1 ───────────────┐
                  │                    │
                  ├─ presentation.v1 ──┼─ Draw Sink
                  └─ elements.v1 ──────┘

Hierarchy Model ─ hierarchical-chart.v1 ─ Draw Sink
```

Presentation 是同步伴随接口：复制请求的系列描述，应用主题调色板，再用标签 Sink 处理字号、
密度和图例位置；随后可选地进入 Element Override Sink。所有临时状态位于调用栈。

### 本章检查

- 主题与元素覆盖能够组合，元素覆盖拥有最终颜色和几何决定权。
- 层级模型不污染原 Chart ABI。

## 2. Presentation ABI

`fw_chart_presentation_v1` 输入 theme、legend placement、label policy、auto layout、标签预算、
padding 和三类字号比例。`resolve` 返回最终主题、图例位置、类别/值标签步长与预算；`render`
接受同一组 Element Overrides 和 Observer，避免主题渲染绕开元素图层。

失败：非法枚举/布尔/非有限比例返回 `INVALID_ARGUMENT`；标签预算超限返回
`RESOURCE_LIMIT`；下游拒绝返回 `SINK_REJECTED`。

### 本章检查

- 接口输入输出均可独立单测。
- 旧 `chart.v1` 不需要知道 Presentation 结构。

## 3. 主题解析

Auto 根据 `target.prefers_dark` 和 `target.high_contrast` 解析。每种主题含 8 色离散调色板、
前景、弱前景、网格、线宽、点半径、柱间距和填充 alpha。原请求透明度最后统一乘入 Sink，
未覆盖区域始终透明。

### 本章检查

- 颜色选择只依赖 series source order，跨平台确定。
- High Contrast 不只依赖红绿区分。

## 4. 标签规划

Resolver 先计算类别与值标签 stride。标签 Sink 使用近似文本框
`width = utf8_codepoints × font_size × 0.58`（左右各 `0.29`），按发射顺序检测矩形重叠。Title 和显式 All 标签优先，
Auto 标签在超出预算或碰撞时跳过。右侧图例被映射到固定列，底部图例按系列数均分。

### 本章检查

- 算法无需字体引擎仍能在所有宿主得到一致结果。
- 宿主仍可基于真实字体做第二阶段精细排版。

## 5. 六种普通图表算法

- Diverging Bar：复用 Bar 的零基线与正负域，强制 horizontal。
- Facet Line：绘图区按可见系列等高切分，各面板独立归一化并共享类别位置。
- Range Area：相邻类别的 upper/lower 四点组成带状 polygon，边界用 line 绘制。
- Density Heatmap：12×8 规则网格；每个点用高斯近似核累加，按最大密度归一化着色。
- Word Cloud：按频率降序的稳定输入顺序，中心向外螺旋搜索矩形，最多 256 词。
- Rose：类别等角，半径使用 `sqrt(value/max)`，面积与值近似成正比。

### 本章检查

- 所有算法有显式上限，不产生数据相关的无限循环。
- 每个 datum 命令携带 series/category ID，可进入元素图层。

## 6. 层级校验与布局

节点0必须为根且 parent 为 `UINT32_MAX`；其他节点 parent 必须小于当前索引。插件扫描父链
计算深度，超过预算即拒绝。Treemap 采用按深度交替方向的 slice-and-dice；Sunburst 扫描
子树权重并分配角度；Packed Bubble 对叶节点按半径降序做有界螺旋碰撞布局。

### 本章检查

- 父先子后的约束天然消除环并允许无堆分配遍历。
- 布局失败会返回资源限制，而不是重叠到不可读。

## 7. 测试与追踪

| 需求 | 测试 |
| --- | --- |
| 主题/自动布局 | Presentation resolve、palette、label stride、透明留白 |
| 六种普通图表 | 每 Kind 的图元类型、数量、ID 和非法输入 |
| 层级 Profile | 三布局、父索引、深度、命中、语义、Sink 平衡 |
| 生命周期 | Runtime 注册、查询、卸载与 ASan |
| 跨平台 | Native Asset、Flutter Widget、Windows Release、Apple 增量提示词 |

### 本章检查

- 每组需求都有至少一个原生合同测试。
- Demo 只消费真实 C 插件输出，不在 Dart 中重写布局算法。
