# FacetWire VisualTransform 0.1

状态：**Experimental Draft**

## 1. 目的与适用范围

`VisualTransform` 是 Image、Animated Image（GIF/APNG/WebP）、Video 和未来 Chart
Renderer 共用的内容到 Zone 变换合同。它只处理固有尺寸、fit、alignment、clip、内容
旋转和透明留白，不拥有页面排版、兄弟 Layer 重排、播放器状态或背景绘制。

### 本章检查

- 四类视觉内容只有一套几何语义。
- 文档布局与像素/帧绘制的职责没有混合。
- 新图表插件可以直接复用，不需要修改 0.1 算法。

## 2. 输入

输入由 `fw_visual_transform_v1`、固有尺寸和 viewport 组成：

| 字段 | 约束 | 含义 |
| --- | --- | --- |
| `fit` | `none/contain/cover/fill` | 内容适配方式 |
| `alignment_x/y` | 有限数 `[0,1]` | 内容在 viewport 内的对齐比例 |
| `clip` | `0/1` | 是否裁剪到 viewport |
| `content_rotation_quarter_turns` | `0..3` | 顺时针内容旋转 |
| `intrinsic_size` | 有限、非负 | 解码内容的固有逻辑尺寸 |
| `viewport` | 有限、宽高非负 | 已由布局分配的 Zone |

`opacity` 不参与几何解析，由各 Renderer 在绘制时乘入内容 Alpha。`1` 完全不透明，`0`
完全透明。

### 本章检查

- 所有输入都有取值范围。
- opacity 与透明度方向没有反转。
- viewport 是布局结果，不从应用窗口大小隐式推导。

## 3. 确定性解析算法

```text
effective = intrinsic_size
if rotation is 90° or 270°:
    effective = swap(width, height)

scale according to fit using effective and viewport
destination = align(scaled_effective, viewport, alignment_x, alignment_y)
source_normalized = [0, 0, 1, 1]
uncovered_is_transparent = true
```

- `none` 保持有效固有尺寸；
- `contain` 取 `min(viewport.w/effective.w, viewport.h/effective.h)`；
- `cover` 取相同比例中的 `max`；
- `fill` 独立使用 viewport 宽高；
- 未知/零固有尺寸确定性回退到 viewport；
- 算法不得读取桌面尺寸、设备物理像素或旧帧缓存。

### 本章检查

- 90°/270° 是先交换固有宽高，再 fit，不是先 fit 后裁切补丁。
- 相同输入在所有平台产生相同逻辑矩形。
- 无需渲染的像素保持透明。

## 4. 输出与绘制合同

`fw_visual_transform_result_v1` 输出 viewport、有效固有尺寸、归一化 source、destination、
旋转、clip 和 `uncovered_is_transparent`。Image/GIF 的 transformed draw sink、Video 的
external surface、decoded frame 与 poster sink，以及未来 Chart sink 都必须消费这一等价
结果。

宿主只绘制 destination 内的内容。Renderer 不得填充 destination 外或 viewport 内的
空白；需要黑色、白色、磨砂或其他背景时，场景必须提供显式背景 Layer。Poster、首帧和
实时画面切换时不得同时占用同一视觉槽，也不得用未旋转 Poster 填补空白。

### 本章检查

- 各输出模式共享同一变换。
- 背景所有权属于场景/宿主而非基础 Renderer。
- 透明画布和透明 Layer 可显示其下方内容。

## 5. 内容旋转与 Layer 旋转

内容旋转保持 Zone 不变，只改变 Zone 内视觉内容。`fw_visual_transform_layer_bounds` 则
用于场景 Layer 旋转：围绕中心旋转，90°/270°交换有效 bounds 宽高。Layer 内容随 Layer
一起旋转；关联字幕、控件等兄弟 Layer 由 Layout Plan 重新定位，但默认保持正向。

两者可以组合，组合时宿主先解析 Layer bounds，再在新 viewport 内解析内容
`VisualTransform`。不得把 Layer 旋转写入内容旋转字段以逃避 reflow。

### 本章检查

- 两种旋转的坐标责任清晰。
- 组合顺序唯一且可测试。
- 字幕和控件不会因视频层旋转而被意外倒置。

## 6. ABI、兼容与扩展

C ABI 位于 `include/facetwire/visual_transform.h`。公共函数负责验证、内容变换解析和 Layer
bounds 解析。Image ABI 以尾部可选字段加入内容旋转，并通过
`draw_image_transformed` 传递完整结果；旧 draw sink 遇到非零旋转必须返回
`FW_STATUS_UNSUPPORTED`，不得静默忽略。Media ABI 的既有类型名是公共枚举的兼容别名。

后续可在结构尾部通过 `struct_size` 增加镜像、任意角度、裁剪窗口或变换矩阵；0.1
Renderer 不得自行解释未知 flags。Chart 0.1 应直接链接 `FacetWire::facetwire` 并调用公共
解析函数。

### 本章检查

- 旧宿主可以继续执行 0°绘制。
- 新能力失败方式明确，不会错误降级。
- 扩展保留 ABI 尾部增长空间。

## 7. 最低一致性测试

1. `1920×1080` 以 contain 放入 `400×300`；
2. 同一内容旋转 90°后得到 `168.75×300`；
3. Image、GIF、Video 对相同输入输出相同 destination；
4. Poster、decoded frame、external surface 使用相同旋转；
5. 未覆盖区域标记透明；
6. Layer `400×300` 旋转 90°后围绕中心得到 `300×400`；
7. 非法 fit、alignment、rotation 和非有限数被拒绝；
8. 旧 Image sink 对非零旋转返回 `UNSUPPORTED`；
9. opacity 0 不发视觉命令，但仍保留几何和语义；
10. acquire/release、open/close、frame acquire/release 在成功和失败路径平衡。

### 本章检查

- 覆盖算法、跨插件一致性、ABI 降级和资源生命周期。
- 图表加入后只需复用并扩展同一矩阵。
- 测试不依赖某个桌面窗口或平台播放器实现。
