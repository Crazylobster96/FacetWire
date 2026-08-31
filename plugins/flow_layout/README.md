# Flow Layout Renderer 0.1 / 流式排版插件 0.1

## 中文

`org.facetwire.reference.flow-layout` 把宿主提供的 Flow Item、Page Template 和受限测量服务组合成稳定、可缓存的 Layout Plan。插件只负责排版，不解析 ASP/JSON，不读取资源，不塑形字体，也不调用图片、视频或图表 Renderer。

当前实验切片支持：

- `facetwire.layout.flow.v1` 公共 C ABI 与静态/动态查询入口；
- `continuous` 单页连续区域与 `block` Paragraph/Object；
- `virtual-pages` 多页区域、页内逻辑坐标、不可拆对象整体移页；
- `columns` 等宽多栏区域、栏间距、先换栏后换页与每片段 `columnIndex`；
- 文本片段跨栏/跨页连续范围、continuation 标记和页/片段预算；
- `inline object` 在三种页面模式中作为不可拆 replacement segment 参与换行与跨区域推进；
- alphabetic、middle、text-top、text-bottom 四种 baseline 和 RTL 行内定位；
- `float-start` / `float-end` 按逻辑方向定位，向 Text Fragment Service 传递带 margin 的轴对齐矩形 exclusion；
- 活动 float 受 `max_active_floats` 约束；剩余宽度低于 `minimum_text_width` 时正文下降到最近 float 底部，换栏/换页后清空区域 float；
- Text Fragment Service 的 `INLINE_PARTS` 能力协商，以及 Child Measure Service 的行内对象预测量；
- 相邻 block 垂直 margin 取最大值；
- Page/Fragment Sink 的成功、拒绝和失败平衡；
- 稳定派生 ID、与指针地址无关的 128-bit Plan Key；
- 预算、UTF-8、唯一 ID、inline 单一所有权和结构范围验证。

尚未支持：overlay、break/keep/widow/orphan 控制。相关合法请求返回 `FW_STATUS_UNSUPPORTED`。Playground 已提供 continuous、virtual-pages、双栏 columns 与 block/inline/float-start/float-end 四类内容关系的原生验证场景；页数由输入内容计算，不作为固定合同。

## English

`org.facetwire.reference.flow-layout` composes host-owned Flow Items, a Page Template, and bounded measurement services into a stable, cacheable Layout Plan. It does not parse ASP/JSON, access resources, shape fonts, or call image, media, or chart renderers.

The current experimental slice supports the public `facetwire.layout.flow.v1` C ABI; continuous, virtual-page, and multi-column block/inline layout; cross-region text and inline-object continuation; atomic replacement objects; four baseline modes; RTL placement; logical `float-start`/`float-end` placement; margin-inclusive rectangular exclusions; minimum-text-width clearing; bounded active floats; whole-object column/page advancement; capability-negotiated Text Fragment and Child Measure services; adjacent vertical margin collapse; balanced Page/Fragment sink calls; stable derived IDs; pointer-independent 128-bit Plan Keys; and bounded structural validation. Overlays and break/keep/widow/orphan rules remain pending and return `FW_STATUS_UNSUPPORTED`. The Playground exposes continuous, virtual-pages, two-column, block, inline, float-start, and float-end native verification modes; page count is content-dependent rather than a fixed contract.

## Build and test / 构建与测试

```sh
cmake -S . -B build -DFACETWIRE_BUILD_TESTS=ON -DFACETWIRE_BUILD_FLOW_LAYOUT=ON
cmake --build build
ctest --test-dir build -R "flow_layout|memory|plugin_manifests" --output-on-failure
```

Public headers / 公共头文件：

- `include/facetwire/flow_layout.h`
- `include/facetwire/text_fragment_service.h`
- `include/facetwire/child_measure_service.h`

### 状态检查 / Status check

- 实现边界与参数 Schema、Manifest 和返回码一致。
- Layout 与 Renderer/Parser/Resource 所有权没有混合。
- 未实现能力被明确拒绝，不会静默降级或虚报完成。
