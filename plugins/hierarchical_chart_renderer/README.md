# Hierarchical Chart Renderer 0.1 / 层级图表渲染器 0.1

Portable C ABI renderer for parent-first hierarchical data: Treemap, Sunburst
and Packed Bubble. Layout is deterministic, uncovered pixels stay transparent,
and rendering, semantics and hit testing share one transient shape plan.

The plugin retains no request strings, node arrays or host-owned sinks.

支持能力：

- Treemap：矩形面积表示节点权重；
- Sunburst：角度表示权重、环层表示深度；
- Packed Bubble：圆面积表示叶节点权重；
- 父先子后的稳定节点 ID、透明留白、主题颜色、值标签；
- `validate`、`measure`、`render`、`build_semantics` 与 `hit_test`；
- 默认 2048 节点、64 层和 65536 命令的显式资源预算。

数据格式、调用示例和可视化选择建议参见
[Chart Visualization 0.3 用户手册](../../docs/guides/chart-visualization-expansion-user-guide.zh-CN.md)。
