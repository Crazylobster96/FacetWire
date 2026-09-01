# Core Chart Renderer 0.3 / Hierarchical Chart 0.1 跨平台验证指南

适用平台：Windows、macOS、iOS、Android、visionOS；Linux 使用同一正式 Playground。

## 1. 验证目标

本轮验证真实插件 `org.facetwire.reference.core-chart-renderer` 和
`org.facetwire.reference.hierarchical-chart-renderer`，不是综合场景中的旧 Dart
柱状图预览。正式 Playground 首页的图表图标打开独立 Chart 页面，设备验收必须显示：

- Native PASS；
- Balanced PASS；
- Transparent PASS；
- capability 为 `facetwire.renderer.chart`；
- semantics role 为 5。

### 本章检查

- UI 状态来自 Native Asset 返回的数据，而不是仅凭截图推断插件运行。
- 旧综合场景预览和正式 Chart 插件具有明确区分。

## 2. 自动测试

原生测试：

```sh
cmake -S . -B build -DFACETWIRE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

正式 Flutter Playground：

```sh
cd examples/placeholder_demo
flutter pub get
flutter analyze
flutter test
```

需要重点通过：

| 测试 | 覆盖内容 |
| --- | --- |
| facetwire.core_chart_renderer.contract | 23 种普通图表、Presentation、Transform、opacity、semantics、hit-test、预算、Sink 平衡 |
| facetwire.hierarchical_chart_renderer.contract | Treemap、Sunburst、Packed Bubble、节点验证、命中与语义 |
| facetwire.memory.lifecycle | 两个 Chart 插件 load/unload 与 Runtime 销毁 |
| facetwire.placeholder_demo.chart_bridge | 30 个图表入口的真实 Native Bridge JSON |
| chart_renderer_demo_test.dart | UI 类型、主题、图例、标签、自动布局、旋转、整体 opacity 和 Element Inspector 控件 |
| native_asset_clients_test.dart | 同一 Native Asset 提供 Placeholder、Flow、Chart 及元素覆盖 |

### 本章检查

- 原生算法、桥接和 Flutter 投影分别有自动测试。
- 测试不依赖网络、Excel、CSV 或平台专有图表库。

## 3. 手工检查表

| ID | 操作 | 预期结果 |
| --- | --- | --- |
| CH-01 | 打开 Chart 页面 | 显示真实插件 ID、Native PASS |
| CH-02 | 选择柱状图 | 两个系列共 8 根柱，轴、网格、类别和图例存在 |
| CH-03 | 选择折线图 | 两条折线和 8 个数据点，系列颜色稳定 |
| CH-04 | 选择饼图 | 4 个扇区，从 12 点方向按固定顺序排列 |
| CH-05 | opacity 调为 0 | Chart 内容完全消失，渐变宿主背景仍可见 |
| CH-06 | opacity 调为 0.1、0.99、1 | 内容按统一不透明度规则变化 |
| CH-07 | 旋转 90°/270° | Chart 内容旋转，contain 后保持完整，不填充空白背景 |
| CH-08 | 旋转 180° | 图形和标签属于同一内容变换，不出现只转部分命令 |
| CH-09 | 缩放窗口/旋转设备 | 图表保持 16:9 viewport，命令关系稳定，无溢出 |
| CH-10 | 检查状态 Chip | Balanced PASS、Transparent PASS 始终成立 |
| CH-11 | 逐个选择高级类型 | 面积、散点、气泡、环形、雷达、热力、仪表、箱线、直方、瀑布、漏斗、K线、时间序列和组合图均有对应几何 |
| CH-12 | 检查值标签 | 柱/线/饼及高级类型按策略显示值，图例仍只表达系列身份 |
| CH-13 | 选择一个 datum | 下方显示稳定 canonical ID，其他元素不被选择 |
| CH-14 | 调整元素 opacity | 只改变目标 datum 及其值标签，整体图表透明度保持不变 |
| CH-15 | 调整 X/Y、缩放和旋转 | 目标元素围绕自身锚点变换，其他数据节点保持原位 |
| CH-16 | 打开强调色 | 目标元素变为紫色，源系列颜色与源数据均未改变 |
| CH-17 | 提升为独立 Layer | 目标命令报告 promoted，并通过 zOffset 显示在普通元素之上 |
| CH-18 | 重置/切换图表类型 | 覆盖恢复缺省；新类型元素列表重新枚举且无悬空选择 |
| CH-19 | 切换 6 种主题 | 颜色和文字层级可辨认，未覆盖区域继续透明 |
| CH-20 | 切换图例/标签/自动布局 | Bottom/Right/Hidden 与 Auto/All/Important/None 正确生效 |
| CH-21 | 检查 6 种普通扩展 | 对比柱、分区折线、范围面积、密度热力、词云和玫瑰图符合数据语义 |
| CH-22 | 检查 3 种层级图 | Treemap、Sunburst 和 Packed Bubble 使用真实层级插件输出 |
| CH-23 | 调整元素后再切换展示策略 | 元素调整与当前主题、图例、标签和自动布局同时保持 |

### 本章检查

- 全部图表族、四种旋转、整体/元素 opacity 和元素几何覆盖均有可观察结果。
- 宿主渐变背景证明插件未绘制隐式白板或黑色背景。

## 4. 平台记录

每个平台记录：commit、设备/模拟器、OS、架构、Flutter 版本、编译方式、自动测试结果、
CH-01..CH-23、截图和异常。Apple 平台必须区分 macOS App、iOS Simulator/真机与
visionOS Simulator/Vision Pro 真机；没有真机证据时不得标记真机通过。

### 本章检查

- 验证证据能够关联到唯一 commit 和运行目标。
- 平台签名或工具链问题不会被误记为 Renderer 算法失败。

## 5. 当前边界

Core Chart 0.3 的数据由 Bridge 构造为规范化数组。CSV/Excel 导入、自定义坐标轴、双轴、
地图、火焰图、节点动画、数据写回和 retained layer 编辑器尚未实现。Flow Layout 可以把 Chart 当作
通用 Replaced Element 测量和放置，但不会解析其数据或绘制命令。

### 本章检查

- 通过 0.3 不表示数据源适配器、Forge 保存能力或专业图表系统已经完成。
- Renderer 与未来 FacetWire-Forge 的修改/保存职责仍然隔离。
