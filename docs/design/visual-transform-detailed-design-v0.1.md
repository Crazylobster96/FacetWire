# FacetWire VisualTransform 0.1 详细设计

状态：**Experimental Draft**
规范：[VisualTransform 0.1](../../spec/visual-transform-v0.1.zh-CN.md)

## 1. 模块边界

实现位于 `src/visual_transform.c`，公共 ABI 位于
`include/facetwire/visual_transform.h`，由 `FacetWire::facetwire` 导出。Image、GIF、Video
和未来 Chart 插件只能调用公共函数，不得保留本地 placement 副本。该模块无堆分配、无
句柄所有权、无平台 API，也不绘制背景。

### 本章检查

- 算法只有一个实现位置。
- 模块可被 DLL、静态库、framework 和移动端静态注册共同使用。
- 资源生命周期仍归具体 Renderer/Host Service。

## 2. 数据结构

### `fw_visual_transform_v1`

输入结构。调用方初始化全部字段并令 `struct_size=sizeof(...)`。

| 字段 | 输入 | 输出/副作用 |
| --- | --- | --- |
| `fit` | `FW_VISUAL_FIT_*` | 无 |
| `alignment_x/y` | `[0,1]` | 无 |
| `clip` | 0/1 | 无 |
| `content_rotation_quarter_turns` | `FW_VISUAL_ROTATION_*` | 无 |
| `flags` | 0.1 必须为 0 | 无 |

### `fw_visual_transform_result_v1`

调用方提供存储并设置 `struct_size`；函数写入 viewport、有效固有尺寸、source、destination、
旋转、clip 和透明留白标志。结果不拥有任何指针。

### 本章检查

- 所有结构遵守 `struct_size` 约定。
- 输出无跨 ABI 内存所有权。
- 结果足以驱动像素、动画帧、外部视频 surface 和矢量图表。

## 3. 函数合同

### `fw_visual_transform_validate`

```c
fw_status fw_visual_transform_validate(
    const fw_visual_transform_v1 *transform);
```

- 输入：非空且结构大小完整的 transform。
- 成功输出：`FW_STATUS_OK`。
- 失败输出：空指针、非法枚举、非有限/越界 alignment 或 clip 时返回
  `FW_STATUS_INVALID_ARGUMENT`。
- 副作用：无。

### `fw_visual_transform_resolve`

```c
fw_status fw_visual_transform_resolve(
    fw_size_f32 intrinsic_size,
    fw_rect_f32 viewport,
    const fw_visual_transform_v1 *transform,
    fw_visual_transform_result_v1 *out_result);
```

- 输入：有限非负固有尺寸、有限非负 viewport、合法 transform、已初始化输出结构。
- 成功输出：确定性 `fw_visual_transform_result_v1`；
  `uncovered_is_transparent` 恒为 1。
- 失败输出：`FW_STATUS_INVALID_ARGUMENT`，不分配资源。
- 副作用：只写 `out_result`。
- 复杂度：时间 O(1)，空间 O(1)。

### `fw_visual_transform_layer_bounds`

```c
fw_status fw_visual_transform_layer_bounds(
    fw_rect_f32 bounds,
    fw_visual_rotation_quarter_turns layer_rotation_quarter_turns,
    fw_rect_f32 *out_bounds);
```

- 输入：有限非负 bounds、0..3 quarter-turn、非空输出。
- 成功输出：0°/180°保持 bounds；90°/270°围绕原中心交换宽高。
- 失败输出：`FW_STATUS_INVALID_ARGUMENT`。
- 副作用：只写 `out_bounds`。

### 本章检查

- 每个函数的输入、输出、错误和副作用均可直接单测。
- 内容变换与 Layer reflow 使用不同函数。
- 所有函数均可并发调用且无需全局状态。

## 4. Renderer 接入

### Image / GIF

`fw_image_renderer_request_v1.content_rotation_quarter_turns` 是尾部可选字段。新宿主提供
`fw_image_draw_sink_v1.draw_image_transformed`；静态图片和动画每一帧接收相同 transform
结果。旧请求大小默认旋转 0；旧 draw sink 对非零旋转返回 `FW_STATUS_UNSUPPORTED`。

### Video

Media Renderer 将 placement 与 Session 中的内容旋转组装为 `fw_visual_transform_v1`，并
把同一解析结果写入 `fw_media_surface_command_v1`。external surface、decoded frame 和
poster 三条输出路径消费相同 command。

### Chart

Chart Renderer 尚未实现。它的 0.1 ABI 必须复用 `fw_visual_transform_v1/result_v1`，无论
图表输出为矢量 display list、纹理还是节点缩略图。图表交互、数据范围和节点语义不进入
VisualTransform。

### 本章检查

- Image 与 GIF 不因时间轴不同而分叉几何规则。
- Video 三类 surface 不产生旋转跳变。
- 图表扩展点明确但没有提前固化图表业务字段。

## 5. 缓存与透明规则

Renderer cache key 必须包含 fit、alignment、clip、content rotation、destination 和资源
fingerprint。内容旋转后不得复用未旋转的 Poster/帧缓存。`VisualTransform` 不产生背景
绘制命令；viewport 未覆盖区域和透明源像素继续透出下层 Layer。

### 本章检查

- 缓存不会跨旋转错误命中。
- 透明规则不依赖宿主主题颜色。
- Demo 中的黑色画布必须保持显式背景 Layer 身份。

## 6. 单元测试映射

| 合同 | 测试位置 |
| --- | --- |
| fit/旋转/Layer bounds/非法值 | `tests/visual_transform_test.c` |
| Image/GIF transformed sink、透明留白、旧 ABI | `plugins/core_image_renderer/tests/core_image_renderer_test.c` |
| Video surface/poster/frame 共用旋转 | `plugins/core_media_renderer/tests/media_renderer_test.c` |
| acquire/release 与 media handle 平衡 | `tests/memory_lifetime_test.c` 及插件合同测试 |

### 本章检查

- 公共算法和各插件接线分别有测试。
- 回归矩阵覆盖旧 ABI。
- 后续 Chart 测试可追加一行，不改变现有合同。

## 7. 关联性与扩展性结论

VisualTransform 位于 Content Renderer 与 Host Visual Sink 之间；ASP Schema 保存意图，
Layout Plan 提供 viewport，Renderer 解析并提交绘制结果。Layer rotation 位于 Layout Plan
阶段，不覆盖内容旋转。未来任意角度、镜像、裁剪窗口和矩阵可通过结构尾部与 flags 的新
版本扩展；0.1 未识别字段不得被猜测执行。

### 本章检查

- Schema、布局、Renderer、Sink 的数据流闭合。
- 内容旋转和 Layer 旋转不存在责任冲突。
- 扩展不要求破坏现有二进制前缀。
