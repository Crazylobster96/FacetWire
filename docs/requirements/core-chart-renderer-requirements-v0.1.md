# FacetWire Core Chart Renderer 0.1 需求规格

状态：**Experimental Draft**

## 1. 范围与边界

Core Chart Renderer 0.1 把宿主已规范化的 Chart Model 转换为可跨平台重放的矢量
命令。首版支持柱/线/面积/散点/极坐标/统计/金融与组合图族。它不解析 CSV、Excel、数据库或网络响应，也不负责
Flow Layout、兄弟图层移动或持久编辑。

| ID | 需求 |
| --- | --- |
| CHR-SCP-001 | 插件必须通过 `facetwire.renderer.chart.v1` 暴露 validate、measure、render、semantics、hit-test 和 parameter schema。 |
| CHR-SCP-002 | 输入必须是类别、系列和值组成的规范化内存模型，所有指针由宿主拥有。 |
| CHR-SCP-003 | 插件不得执行文件、网络、数据库、CSV 或 Excel 解析。 |
| CHR-SCP-004 | 插件不得创建隐式背景；未覆盖区域和源 Alpha 必须透出下层 Layer。 |

### 本章检查

- Chart 是视图模型，数据源适配器是独立能力。
- Renderer、Flow Layout 与未来 FacetWire-Forge 的职责没有混合。

## 2. 数据模型与验证

| ID | 需求 |
| --- | --- |
| CHR-MOD-001 | 类别和系列 ID 必须是非空、唯一、合法 UTF-8。 |
| CHR-MOD-002 | 每个系列的值数量必须等于类别数量。 |
| CHR-MOD-003 | 值必须有限；缺失值通过显式 `missing` 标记表达。 |
| CHR-MOD-004 | 饼图和环形图只接受一个系列、非负值且可见值总和大于 0。 |
| CHR-MOD-005 | 颜色、opacity、样式、约束和 Transform 必须逐字段验证。 |
| CHR-MOD-006 | 类别、系列、点和命令必须受请求预算约束，超限返回 `RESOURCE_LIMIT`。 |

### 本章检查

- NaN、重复 ID、悬空形状和无界输入均不能进入绘制阶段。
- 缺失值与数值 0 不会混淆。

## 3. 测量与 VisualTransform

| ID | 需求 |
| --- | --- |
| CHR-GEO-001 | 缺省固有尺寸为 640×360，显式固有尺寸必须为两个正值或两个 0。 |
| CHR-GEO-002 | measure 根据 min/max 约束确定尺寸，不移动兄弟内容。 |
| CHR-GEO-003 | render 必须调用共享 `fw_visual_transform_resolve`。 |
| CHR-GEO-004 | 柱、线、点、环形扇区、多边形和标签使用未旋转的 `[0,1]` 规范化坐标。 |
| CHR-GEO-005 | Sink 使用同一 Transform 应用适配、裁剪与四分之一圈旋转。 |

### 本章检查

- Image、GIF、Video 与 Chart 共享同一套视觉变换规则。
- Chart 矢量几何不会把 viewer zoom 写回文件或 Cache Key 意图层。

## 4. 绘制与透明度

| ID | 需求 |
| --- | --- |
| CHR-REN-001 | `begin_chart` 成功后无论中途命令是否失败，都必须调用一次 `end_chart`。 |
| CHR-REN-002 | opacity=0 不得调用 Sink，但仍返回合法 Transform、透明标记与 Cache Key。 |
| CHR-REN-003 | `1=完全不透明`、`0=完全透明`，允许 0.1、0.99 等有限小数。 |
| CHR-REN-004 | Sink 拒绝任何命令时返回 `SINK_REJECTED`，不伪报完整输出。 |
| CHR-REN-005 | 图表命令只描述内容，不填充 Zone 或 App 背景。 |

### 本章检查

- 失败路径保持 Sink 栈平衡。
- Chart 全透明时可以直接看到下层内容。

## 5. 图表算法

| ID | 需求 |
| --- | --- |
| CHR-ALG-001 | 柱状图按类别分组、按可见系列均分组内宽度，坐标域包含零基线。 |
| CHR-ALG-002 | 折线图按类别中心生成数据点；missing 必须断开折线。 |
| CHR-ALG-003 | 饼图从 12 点方向开始，按值占总和比例生成稳定扇区。 |
| CHR-ALG-004 | 相同输入、目标和版本必须产生相同命令顺序、几何和 Cache Key。 |
| CHR-ALG-005 | orientation 与 stack mode 必须组合出横向、普通堆叠和百分比堆叠柱/面积图。 |
| CHR-ALG-006 | 散点/气泡使用 x、value 和 size；箱线与 K 线使用显式复合字段并验证顺序关系。 |
| CHR-ALG-007 | 环形、雷达、热力、仪表盘、直方、瀑布、漏斗、时间序列和组合图必须复用同一 Sink 与 Transform。 |
| CHR-ALG-008 | 类别标签、值标签和图例必须独立开关；值标签支持 value、percent、value-and-percent。 |

### 本章检查

- 所有算法都能映射到相同命令 Sink。
- 系列显隐不会改变源模型，只改变派生输出。

## 6. 语义与交互

| ID | 需求 |
| --- | --- |
| CHR-A11Y-001 | 聚合语义必须使用 `FW_SEMANTICS_ROLE_CHART`，包含标题、摘要、系列数和值数。 |
| CHR-A11Y-002 | hit-test 必须返回柱、折线数据点或饼图扇区的系列、类别和值。 |
| CHR-A11Y-003 | hit-test 接收 viewport 坐标，并逆向应用同一 VisualTransform。 |
| CHR-A11Y-004 | z/视觉顺序不得隐式改变宿主语义阅读顺序。 |

### 本章检查

- 键盘、触摸和 Agent 查询可定位同一数据节点。
- 旋转后的视觉位置与命中位置保持一致。

## 7. 生命周期与安全

| ID | 需求 |
| --- | --- |
| CHR-LIF-001 | 插件只在 load 分配 context，并在 unload 释放。 |
| CHR-LIF-002 | validate、measure、render、semantics 和 hit-test 不保留输入指针。 |
| CHR-LIF-003 | 插件不跨 ABI 分配命令、字符串或图表模型。 |
| CHR-LIF-004 | Windows/macOS/Linux 动态库和 Apple/Android 静态注册使用同一 C 实现。 |

### 本章检查

- 生命周期可由统一 Runtime 测试和 ASan 检查。
- 受限平台无需任意外部动态加载即可获得相同插件行为。

## 8. 验收矩阵与后续边界

自动测试必须覆盖全部图表族、正负柱/线、missing、旋转、opacity 0/小数/1、语义、命中、
预算、非法 UTF-8、重复 ID、Sink 中途失败、Cache Key 与 load/unload。Playground 必须显示
真实插件 ID、Native PASS、命令平衡和透明留白状态。

0.1 不包含自定义刻度、双轴策略、动画过渡、流式数据、CSV/Excel 解析、Visio/脑图/亿图
导入、标签碰撞求解或专业主题。这些能力通过后续版本或独立 Adapter/Profile 扩展。

### 本章检查

- 首版范围足以验证标准 ABI、跨平台矢量输出和交互。
- 后续扩展无需把数据源解析塞入 Renderer。
