# FacetWire Chart Element Layering 0.2 需求规格

状态：**Experimental Draft**
依赖：[Core Chart Renderer 0.1](core-chart-renderer-requirements-v0.1.md)

## 1. 目标与范围

Chart Element Layering 0.2 让标题、绘图区、网格、坐标轴、系列、数据节点、分类标签、
数值标签、图例和注释成为可寻址的逻辑元素，并允许宿主或 AI 对元素施加展示覆盖。图表
整体仍是 FacetWire Scene 中的一个 Chart Layer；子元素默认是轻量逻辑层，只有显式提升
后才由宿主物化为可独立拖动、排序和交互的 Scene Layer。

| ID | 需求 |
| --- | --- |
| CEL-SCP-001 | 必须保留 `facetwire.renderer.chart.v1` 的输入与行为兼容。 |
| CEL-SCP-002 | 必须通过伴随接口 `facetwire.renderer.chart.elements.v1` 提供枚举、稳定 ID 格式化、覆盖验证和带覆盖渲染。 |
| CEL-SCP-003 | 展示覆盖不得修改类别、系列和值等源数据。数据修改属于 Adapter/Forge 的 `dataPatch`。 |
| CEL-SCP-004 | 插件不得直接修改 Scene、移动兄弟 Layer、保存文件或分配宿主持久对象。 |

### 本章检查

- Chart Renderer、Scene Layer Engine 与未来 FacetWire-Forge 的职责明确分离。
- 0.1 Host 不查询新接口时，渲染结果不发生行为变化。

## 2. 元素树与稳定身份

元素角色至少包括：chart-root、plot-area、grid、axis-x、axis-y、title、category-label、
legend-container、legend-item、legend-marker、legend-label、legend-value、series、datum、
value-label 和 annotation。图例层级遵循
[Chart Legend Composition Profile 0.1](../../spec/chart-legend-composition-profile-v0.1.zh-CN.md)。

| ID | 需求 |
| --- | --- |
| CEL-ID-001 | 每个元素必须由 `chartId + role + seriesId + categoryId + partIndex` 的复合引用唯一标识。 |
| CEL-ID-002 | 插件必须能把复合引用格式化为确定性的 UTF-8 canonical ID；特殊字节必须转义。 |
| CEL-ID-003 | 相同模型、元素角色和数据键在重新布局、缩放和跨平台渲染后必须获得相同 ID。 |
| CEL-ID-004 | 数据节点必须以类别稳定 ID 为键，禁止用临时屏幕坐标作为身份。 |
| CEL-ID-005 | 元素描述必须包含父引用、规范化 bounds、z 顺序、能力标记、语义标签及 bounds 精度标记。 |
| CEL-ID-006 | 枚举顺序必须固定：容器、装饰、标签、系列、数据节点、数据标签、注释。 |
| CEL-ID-007 | 每个可见系列必须产生一个 legend-item，并以 marker、label 和可选 value 作为其可独立寻址的子元素。 |
| CEL-ID-008 | legend-container 是 chart-root 的子元素；legend-item 是 container 的子元素；marker/label/value 是 item 的子元素。 |

Canonical ID 示例：

```text
chart/chart%3Aquarterly/series/revenue
chart/chart%3Aquarterly/datum/revenue/q2
chart/chart%3Aquarterly/value-label/revenue/q2
```

### 本章检查

- ID 不依赖指针、数组地址、GPU 对象或本地化后的显示文本。
- 包含 `/`、`%`、空格或非 ASCII 字节的源 ID 不会造成路径歧义。

## 3. 可调能力与展示覆盖

| ID | 需求 |
| --- | --- |
| CEL-OVR-001 | 覆盖必须支持 visible、opacity、颜色、平移、统一缩放、旋转、锚点、zOffset 和 promotion。 |
| CEL-OVR-002 | opacity 使用统一规则：`1=完全不透明`、`0=完全透明`，接受 0.1、0.99 等有限小数。 |
| CEL-OVR-003 | 覆盖通过字段 mask 区分“未指定”与显式零值。 |
| CEL-OVR-004 | selector 允许 exact、role、series 和 datum 范围；更晚的匹配覆盖项逐字段替换更早的匹配值。 |
| CEL-OVR-005 | root 覆盖可级联到全部元素，series 覆盖可级联到其 datum、value-label、legend-item 及图例子部件。 |
| CEL-OVR-006 | datum 覆盖可级联到对应 value-label，但不得隐式改变 datum.value。 |
| CEL-OVR-007 | 变换以元素 bounds 中心为默认锚点；显式锚点使用图表未旋转的 `[0,1]` 坐标。 |
| CEL-OVR-008 | 被隐藏或有效 opacity 为 0 的元素不得向下游绘制图元，但仍可被枚举和查询。 |
| CEL-OVR-009 | 插件必须将覆盖纳入带覆盖渲染的 Cache Key 或使该结果明确不可复用。 |
| CEL-OVR-010 | legend-container 覆盖必须级联到全部图例项及子部件；legend-item 变换必须以该项中心为共同锚点作用于 marker/label/value。 |
| CEL-OVR-011 | 对 legend-marker、legend-label 或 legend-value 的更窄覆盖必须只影响目标子部件。 |

### 本章检查

- 展示覆盖和数据修改不会混为一谈。
- 透明、隐藏、平移与旋转不会产生隐式背景。

## 4. 提升与宿主物化

| ID | 需求 |
| --- | --- |
| CEL-PRO-001 | 默认 promotion 为 inline；promoted 表示宿主可将该逻辑元素物化为独立 Scene Layer。 |
| CEL-PRO-002 | 插件通过元素观察回调报告 effective presentation，不直接创建或持有 Scene Layer。 |
| CEL-PRO-003 | promoted 元素必须保留父 Chart Layer 的坐标系、裁剪、语义关系和源数据绑定。 |
| CEL-PRO-004 | 宿主不支持物化时必须仍能应用颜色、透明度和几何覆盖，并报告降级状态。 |
| CEL-PRO-005 | zOffset 由支持分层捕获的宿主排序；顺序式 v1 Sink 可以保持源命令顺序。 |

### 本章检查

- “提升”是可移植意图，不等同于插件跨 ABI 创建 UI 对象。
- 受限平台可用静态注册实现相同语义。

## 5. 枚举、虚拟化与性能

| ID | 需求 |
| --- | --- |
| CEL-PERF-001 | 元素枚举必须为同步、无插件堆分配、流式回调。 |
| CEL-PERF-002 | Host 可设置 `maxElements`；超过预算时返回 `RESOURCE_LIMIT`，不得静默截断。 |
| CEL-PERF-003 | 大型散点/热力数据允许把未提升 datum 标记为 virtualized；命中或选择后按需物化。 |
| CEL-PERF-004 | 格式化 canonical ID 支持先查询所需长度，再写入调用方缓冲区。 |
| CEL-PERF-005 | 覆盖匹配复杂度首版允许 O(commands × overrides)，但 overrides 受预算限制且必须确定性。 |

### 本章检查

- 十万个数据点不会强制创建十万个 Flutter/Qt/原生 Widget。
- 插件不跨 ABI 返回需要释放的元素数组或字符串。

## 6. 交互、语义与可访问性

| ID | 需求 |
| --- | --- |
| CEL-A11Y-001 | hit-test 结果必须可映射到同一元素复合引用和 canonical ID。 |
| CEL-A11Y-002 | 逻辑阅读顺序由元素树确定，不因视觉 zOffset 自动改变。 |
| CEL-A11Y-003 | promoted 元素必须保持 role、label、series/category 数据关系。 |
| CEL-A11Y-004 | observer 必须在对应绘制命令之前收到当前元素及有效 presentation。 |

### 本章检查

- 鼠标、触摸、键盘和 Agent 使用同一元素身份。
- 视觉排序与无障碍阅读排序可以明确独立。

## 7. 验证、安全与生命周期

| ID | 需求 |
| --- | --- |
| CEL-SAFE-001 | selector 字符串、浮点数、mask、枚举、数量和结构大小必须在绘制前验证。 |
| CEL-SAFE-002 | scale 必须有限且大于 0；opacity、颜色和锚点必须位于合法范围。 |
| CEL-SAFE-003 | API 不保留 request、override、sink、observer 或格式化缓冲区指针。 |
| CEL-SAFE-004 | observer 或 draw sink 拒绝命令时必须平衡 begin/end 并返回 `SINK_REJECTED`。 |
| CEL-SAFE-005 | Windows/Linux 动态库和 Apple/Android 静态注册必须使用同一 C 实现。 |

### 本章检查

- 所有权均可由生命周期测试和 ASan 审计。
- 非法 selector 无法绕过 Chart Model 的原始预算限制。

## 8. 验收标准

1. 原生测试枚举全部角色，验证 ID 稳定、唯一、可两阶段格式化。
2. 对单个 bar/datum 设置 opacity、颜色、平移、缩放、旋转后，仅目标元素发生变化。
3. series 覆盖可级联，后置 datum 覆盖可覆盖继承值。
4. promoted 元素通过 observer 报告，普通 0.1 render 保持原输出。
5. Playground 可以选择元素，调节透明度、位置、缩放、旋转和提升状态。
6. Windows Release、Flutter 测试、原生 CTest、生命周期和 ASan 测试通过。
7. 用户手册包含 Agent 指令映射、API 示例、限制和故障排查。

### 本章检查

- 功能、ABI、跨平台演示和文档均有可执行验收项。
- 0.2 不承诺数据写回、标签智能避让、动画或大型数据索引；这些保留给后续版本。
