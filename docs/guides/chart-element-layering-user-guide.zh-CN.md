# FacetWire 图表元素图层用户手册（Chart Element Layering 0.2）

## 1. 功能概览

Core Chart Renderer 0.2 可以把一个图表内部的标题、绘图区、网格、坐标轴、分类标签、
图例、系列、单个数据节点和数值标签作为可寻址的逻辑元素。用户可以只调整一个 Q2
柱子，也可以调整整个 Revenue 系列，而不修改数据源。

支持的展示调整：

- 显示或隐藏；
- 不透明度，`1=完全不透明`、`0=完全透明`；
- 颜色覆盖；
- X/Y 平移；
- 统一缩放；
- 任意角度旋转；
- zOffset；
- 提升为可由宿主物化的独立 Layer。

“提升”不会把图表元素从数据源中复制出来。元素仍以 Chart Layer 为父级，继续使用图表
坐标系、裁剪和数据绑定。

## 2. 在 Playground 中操作

1. 启动 `facetwire_placeholder_demo`。
2. 在首页选择“Core Chart Renderer / 核心图表”。
3. 选择任一图表类型，例如“柱状图”。
4. 在右侧滚动到“Element Inspector / 元素图层调整”。
5. 在“逻辑元素”下拉框中选择元素，例如：
   `数据节点 · Q2`。
6. 调整以下控件：
   - 元素不透明度；
   - X 平移、Y 平移；
   - 缩放；
   - 旋转；
   - 提升为独立 Layer；
   - 强调色覆盖。
7. 点击“重置当前元素展示”恢复展示参数。
8. 选择“未选择 / 整体图表”退出元素编辑。

图表整体不透明度与元素不透明度相互独立。最终 Alpha 由整体 opacity、元素 opacity 和
图元自身 Alpha 共同决定。

## 3. 元素层级

```text
图表根 chart-root
├─ 标题 title
├─ 绘图区 plot-area
│  ├─ 网格 grid
│  ├─ X/Y 坐标轴 axis-x / axis-y
│  ├─ 分类标签 category-label
│  └─ 系列 series
│     └─ 数据节点 datum
│        └─ 数值标签 value-label
└─ 图例容器 legend-container
   └─ 图例项 legend-item
      ├─ 色标 legend-marker
      ├─ 标签 legend-label
      └─ 数值 legend-value（可选）
```

元素下拉框中的显示名称用于阅读；保存和 API 调用应使用 canonical ID。例如：

```text
chart/chart%3Aquarterly/series/revenue
chart/chart%3Aquarterly/datum/revenue/q2
chart/chart%3Aquarterly/value-label/revenue/q2
```

请不要使用元素在下拉框中的数组序号作为持久身份。序号仅供当前一次 Demo 报告选择；
正式应用应保存 canonical ID 或复合引用。

图例容器、图例项和其子部件的跨图表统一行为见
[Chart Legend Composition Profile 0.1](../../spec/chart-legend-composition-profile-v0.1.zh-CN.md)。

## 4. 常见自然语言操作

AI 应用可以把用户指令映射为 presentation override：

| 用户指令 | selector | fields |
| --- | --- | --- |
| “把 Revenue 系列调成半透明” | role=series, seriesId=revenue | opacity=0.5 |
| “把 Q2 柱子向右移动一点” | role=datum, seriesId=revenue, categoryId=q2 | translation.x=0.03 |
| “突出 Q4” | role=datum, categoryId=q4 | scale=1.15, zOffset=20, promoted |
| “隐藏 Cost 系列” | role=series, seriesId=cost | visible=false |
| “把标题旋转 10 度” | role=title | rotation=0.174533 |
| “只把 Q1 的值标成紫色” | role=value-label, categoryId=q1 | color override |
| “把 Revenue 图例整体向右移” | role=legend-item, seriesId=revenue | translation.x=0.03 |
| “只把 Revenue 的色标改成橙色” | role=legend-marker, seriesId=revenue | color override |
| “把全部图例调成 60% 不透明” | role=legend-container | opacity=0.6 |

展示覆盖不会改变 Q1/Q2 的数值。如果用户要求“把 Q2 收入改为 42”，AI 应调用数据层或
FacetWire-Forge 的 `dataPatch`，再触发图表重新渲染。

## 5. Selector 与级联

Selector 由 role、chartId、seriesId、categoryId 和 partIndex 组成。空的 series/category
表示通配；`FW_CHART_ELEMENT_PART_ANY` 表示任意子部件。

推荐按从宽到窄排列覆盖：

```text
1. chart-root：全图缺省展示
2. series/revenue：Revenue 系列展示
3. datum/revenue/q2：Q2 特例
```

后面的匹配项按字段覆盖前面的匹配项。例如系列 opacity=0.5，随后 Q2 opacity=0.9，
则 Q2 的有效元素 opacity 是 0.9；其他 Revenue 数据节点仍为 0.5。

## 6. C ABI 使用示例

查询伴随接口：

```c
const void *value = NULL;
fw_status status = plugin_api->query_interface(
    plugin_handle,
    (fw_string_view)FW_STRING_VIEW_LITERAL(
        FW_CHART_ELEMENT_INTERFACE_ID),
    FW_CHART_ELEMENT_INTERFACE_VERSION,
    &value);
const fw_chart_element_api_v1 *elements =
    (const fw_chart_element_api_v1 *)value;
```

枚举元素：

```c
fw_chart_element_enum_sink_v1 sink = {
    sizeof(sink), user_data, visit_element
};
fw_chart_element_enum_result_v1 result = {0};
result.struct_size = sizeof(result);
status = elements->enumerate(plugin_handle, &chart_request,
    &sink, &result);
```

为 `revenue/q2` 创建覆盖：

```c
fw_chart_element_override_v1 override = {0};
override.struct_size = sizeof(override);
override.selector.struct_size = sizeof(override.selector);
override.selector.role = FW_CHART_ELEMENT_ROLE_DATUM;
override.selector.chart_id = chart_request.chart_id;
override.selector.series_id = revenue_series.id;
override.selector.category_id = q2_category.id;
override.selector.part_index = FW_CHART_ELEMENT_PART_ANY;
override.fields = FW_CHART_OVERRIDE_OPACITY |
    FW_CHART_OVERRIDE_TRANSLATION |
    FW_CHART_OVERRIDE_PROMOTION;
override.opacity = 0.45f;
override.translation = (fw_point_f32){0.03f, 0.0f};
override.promotion = FW_CHART_ELEMENT_PROMOTED;

status = elements->render(plugin_handle, &chart_request,
    &override, 1u, viewport, &services, &observer, &render_result);
```

调用结束后插件不会保留 override、request、sink 或 observer 指针。

## 7. 保存到展示文件

0.2 推荐把展示调整保存在 Chart Zone 的 `elementOverrides` 中。该字段属于展示意图，
不属于数据源：

```json
{
  "chartId": "chart:quarterly",
  "elementOverrides": [
    {
      "selector": {
        "role": "datum",
        "seriesId": "revenue",
        "categoryId": "q2"
      },
      "presentation": {
        "opacity": 0.45,
        "translation": {"x": 0.03, "y": 0},
        "promotion": "promoted"
      }
    }
  ]
}
```

当前 FacetWire Renderer 负责读取规范化内存模型并渲染；文件解析、合并、事务、撤销和保存
将在 FacetWire-Forge 中实现。应用现阶段可以自行把 JSON 映射为 C ABI 结构。

## 8. 提升为独立 Layer

当 promotion 为 `promoted` 时，支持 retained layer 的宿主应：

1. 按 canonical ID 聚合同一元素的绘制命令；
2. 创建或复用对应子层；
3. 使用 Chart Layer 作为相对坐标父级；
4. 应用 zOffset；
5. 保留 role、label 和 series/category 数据绑定；
6. 在重新渲染后按 canonical ID 重新关联，而非保留旧命令指针。

不支持 retained layer 的宿主仍会获得插件已经处理后的颜色、透明度与几何，只是不能把
元素变成平台 UI 树中的长期对象。

## 9. 大数据与性能

- 不要为每个散点创建 Flutter Widget、UIView 或 Android View。
- 使用元素枚举回调建立轻量索引；实际绘制仍由批量 Canvas/GPU Sink 完成。
- 只对被选择、需要拖动或需要独立交互的元素执行 promoted。
- 每次请求最多 256 个展示覆盖；更多规则应先由 Host/Forge 合并。
- canonical ID 由调用方缓冲区承载，插件不会返回需要释放的字符串。

## 10. 故障排查

### 调整没有效果

检查 selector 的 chartId、seriesId、categoryId 是否与源模型稳定 ID 完全一致。不要使用
显示文本 `Q2 Revenue` 代替 ID。

### 透明度为 0 仍看到内容

确认看到的不是同位置的另一个系列、网格或标签。datum 与 value-label 是不同角色；datum
覆盖会级联到自己的 value-label，但 category-label 不属于 datum。

### 元素提升后没有独立拖动句柄

Renderer 只报告 promotion 意图。宿主必须实现 retained layer 物化和拖动 UI。Playground
通过 zOffset 与命令元数据显示这条链路，但不是完整编辑器。

### 数据刷新后覆盖丢失

确保数据 Adapter 保持 chartId、seriesId 和 categoryId 稳定。不要用数组索引或屏幕坐标
生成 ID。

### 返回 `RESOURCE_LIMIT`

减少覆盖数量或元素数量，并检查 Chart budget。插件不会静默截断元素树。

## 11. 当前限制

0.2 尚未包含：

- 数据修改、撤销、合并和文件写回；
- R-tree 大数据元素索引；
- 标签智能碰撞与自动引线；
- 元素动画插值；
- 多绘图区和双坐标轴元素树；
- 宿主 retained layer 的统一跨 UI 框架实现。

这些限制不会改变当前 canonical ID、selector、presentation override 和 Observer 的核心
语义，后续可以保持兼容扩展。
