# FacetWire Core Content Profile 0.1

状态：**Experimental Draft**

规范性术语“必须（MUST）”“不得（MUST NOT）”“应该（SHOULD）”“可以（MAY）”按
规范要求解释。本 Profile 标准化 ASP Zone 中的基础文本和媒体内容，不定义具体编解码
器，也不要求所有平台支持同一组媒体格式。

## 1. 范围与分阶段原则

0.1 定义五种 `content.type`：

- `text`：统一样式的 UTF-8 纯文本；
- `image`：静态图片；
- `animated-image`：GIF、APNG、动态 WebP 等帧动画图片；
- `video`：带可选海报和文本轨道的视频；
- `audio`：带可选标题、封面和文本轨道的音频。

图表、脑图、Visio、Excel/CSV 数据源不属于本 Profile。它们将在第二阶段通过独立的
结构化数据与图形 Profile 定义，并复用本 Profile 的 Resource、Placement、Session 和
降级原则。

图像能力采用分级设计：本 Profile 的 `image` 与 `animated-image` 属于 **Core Image
Profile**；渐变、羽化、通道、色阶、饱和度、多图层合成和 AI 派生结果属于后续独立的
**Image Composition Profile**。完整边界见 [ADR-0004](../docs/adr/0004-image-capability-tiering.md)。


### 本章检查

- 第一阶段内容类型闭合且能独立实现。
- 媒体容器语义与具体 codec 支持分离。
- 第二阶段不会迫使基础媒体 Renderer 理解表格或图结构。

## 2. 共同不变量与资源引用

除 `text` 外，内容必须通过 `resource`、`posterResource`、`artworkResource` 或 Track
中的 `resource` 引用当前 ASP 根 Descriptor 的 `resources[].id`，不得直接保存文件路径、
绝对路径或远程 URL。跨对象验证器必须确认：

1. 每个引用恰好解析到一个 Resource；
2. Resource 的 `source` 满足 ASP Portable Path 规则；
3. `mediaType` 与内容类型相容；
4. 无法解析时保留 Zone `bounds` 并转到 Placeholder Renderer。

`opacity` 的语义是不透明度：`1` 完全不透明，`0` 完全透明。Renderer 必须将它乘入
内容自身 Alpha，不得因透明内容自动填充白色或其他不透明背景。

### 本章检查

- 内容身份和物理存储路径解耦。
- 不透明度方向与此前窗口透明度约定一致。
- 资源失败不会改变页面、Layer、Zone 或兄弟内容几何。

## 3. 文本内容 `text`

最小对象：

```json
{
  "type": "text",
  "format": "plain",
  "text": "Hello，FacetWire。"
}
```

`text` 必须是 JSON 字符串，解析后按 Unicode 文本处理；换行使用 LF 语义，读取器应将
CRLF/CR 规范化为 LF。0.1 的 `format` 只能是 `plain`，Markdown、HTML、富文本 Run、
内嵌对象和脚本均不属于本版本。

可选字段：

| 字段 | 语义 |
| --- | --- |
| `language` | BCP 47 语言标签；缺省由宿主推断 |
| `direction` | `auto`、`ltr`、`rtl` |
| `selectable` | 是否允许文本选择；不等于允许编辑 |
| `opacity` | 整体内容不透明度 0–1 |
| `style` | 字体、字号、字重、颜色、行高等 |
| `layout` | 对齐、换行、溢出、最大行数、内边距 |

### 3.1 字体和确定性

`fontResource` 引用包内字体 Resource；`fontFamilies` 是按顺序排列的字体族候选。
同时提供时优先使用 `fontResource`。没有精确字体时，宿主可以 fallback，但必须在诊断
或测量结果中暴露替代事实；不得声称像素级排版确定。

`fontSize` 和 `letterSpacing` 使用 Canvas 逻辑单位，`lineHeightMultiplier` 是字号倍数。
`fontWeight` 范围 1–1000。`fontScale` 属于 Render Target/Profile，在排版阶段应用，不能
改写持久文档字段。

### 3.2 文本布局

`layout.overflow` 支持：

- `visible`：可以画出 Zone，但不得影响其他 Zone 的存储几何；
- `clip`：裁剪到内容框；
- `ellipsis`：在最后可见行截断；
- `scroll`：Zone 成为滚动视口，滚动位置属于 Presentation Session。

`maxLines` 缺省表示不设行数上限。`no-wrap` 与 `ellipsis` 组合时只形成一行；其他非法
或无法支持的组合必须确定性降级并返回诊断，不能静默改变 Zone 尺寸。

### 本章检查

- 0.1 文本能力足以实现基础段落而不暗示完整 Word 排版。
- 字体替代和无障碍字体缩放不会污染文档。
- 选择、滚动和编辑三种状态没有混为一谈。

## 4. 静态图片 `image`

```json
{
  "type": "image",
  "resource": "image.cover",
  "alt": "校园操场航拍图",
  "placement": {
    "fit": "contain",
    "alignment": {"x": 0.5, "y": 0.5},
    "clip": true
  }
}
```

`resource` 必须指向静态图片 Resource。`alt` 必须存在；空字符串明确表示装饰图片，
不是“尚未生成替代文本”。`sampling` 支持 `auto`、`smooth`、`pixelated`。
`orientation=from-metadata` 要求应用 EXIF/容器方向后再测量和放置。

Alpha 通道必须保留。颜色空间转换由 Decoder/宿主负责，Renderer 不得把未知颜色空间
当作改变布局的理由。

### 本章检查

Core Image 只定义单一 Resource 的基础展示和整体 `opacity`。以下能力不得通过向
`image` 对象随意追加字段实现：渐变透明度、蒙版、羽化、混合模式、通道混合、色阶、
曲线、色相/饱和度、滤镜栈、多源图层合成和 AI 处理任务。它们属于独立的
`image-composition` 内容类型；低能力设备应使用该合成提供的扁平 `previewResource`，
不可用时再进入 Placeholder。


- 图片可访问性、采样、方向和透明通道均有确定语义。
- 图片仍由 Zone 定位，不会隐式挤压其他 Zone。
- 解码失败可以原位替换为 Placeholder。

## 5. 动态图片 `animated-image`

`animated-image` 独立于 `image`，用于 GIF、APNG、动态 WebP 等具有时间轴的图片。
是否动态由解码后的资源事实决定，而不是仅根据扩展名判断。

```json
{
  "type": "animated-image",
  "resource": "animation.loading",
  "alt": "数据同步动画",
  "playback": {
    "autoplay": true,
    "loop": true,
    "playbackRate": 1,
    "controls": "hidden"
  }
}
```

默认自动播放、循环、1 倍速、隐藏控件。Render Target 的 `reduce_motion=true` 时，宿主
必须停止自动播放并展示稳定代表帧，除非用户在当前 Session 明确启动。当前帧、累计循环
次数和暂停状态属于 Session，不得回写文档。

### 本章检查

- GIF 被当作时间媒体，而不是静态图片的偶然特例。
- 减少动态效果设置具有强制降级规则。
- 播放进度与持久初始策略已经分离。

## 6. 视频 `video`

视频内容包含主 `resource`、可选 `posterResource`、`placement`、`playback` 和 `tracks`。
主 Resource 的 `mediaType` 必须是宿主认可的视频媒体类型；标准不保证某一 codec 在所有
平台均可播放。

```json
{
  "type": "video",
  "resource": "video.introduction",
  "posterResource": "image.video-poster",
  "label": "FacetWire 介绍视频",
  "playback": {
    "autoplay": false,
    "loop": false,
    "muted": false,
    "volume": 1,
    "playbackRate": 1,
    "controls": "auto"
  },
  "tracks": [
    {
      "resource": "subtitle.zh-cn",
      "kind": "subtitles",
      "language": "zh-CN",
      "label": "简体中文",
      "default": true
    }
  ]
}
```

`controls=auto` 由宿主根据输入方式、控制 Layer 和可访问性决定是否显示。隐藏内建控件
不等于禁止外部控制 Layer 或对话 Action。`startOffsetMs`/`endOffsetMs` 定义可播放片段，
跨字段验证必须保证结束时间大于开始时间且不超过已知媒体时长。

Track `kind` 支持 `subtitles`、`captions`、`descriptions`、`chapters`、`metadata`。0.1
定义轨道选择槽和资源关系，不定义自动翻译算法；翻译插件可以生成新 Track Resource，
再通过原子文档 Patch 插入 `tracks`。

### 本章检查

- 视频视觉布局、播放策略、控制 UI 和字幕生成职责相互独立。
- 自定义控制 Layer 可以操作同一 Session，而不修改视频内容对象。
- codec 不支持时有标准 Capability/Placeholder 路径。

## 7. 音频 `audio`

音频包含主 `resource`、必需的可访问 `label`，以及可选 `title`、`artworkResource`、
`playback` 和 `tracks`。Zone 可以展示宿主控件、封面和标题；即使控件隐藏，Semantics
仍必须让可访问性客户端识别其为音频内容。

音频和视频共享 AV 初始策略：默认不自动播放、不循环、不静音、音量 1、1 倍速、控件
自动。平台静音开关、音频焦点、后台播放和用户调整音量属于 Session/宿主策略，并优先于
文档默认值。

### 本章检查

- 无视觉帧的音频仍有 Zone、Semantics 和可选封面展示。
- 文档默认音量不会覆盖用户或系统音量策略。
- 音频文本轨道可以承载章节、歌词式字幕或描述，但不定义其生成算法。

## 8. Placement 统一语义

静态图片、动态图片和视频复用同一 `placement`：

| `fit` | 语义 |
| --- | --- |
| `none` | 使用内容固有逻辑尺寸，1:1，不缩放 |
| `contain` | 等比缩放并完整显示，默认值 |
| `cover` | 等比缩放并覆盖 Zone，允许溢出 |
| `fill` | 横纵独立缩放到 Zone |

默认 `alignment={0.5,0.5}`、`clip=true`。Placement 只计算内容到 Zone 的变换，不得
修改 Zone `bounds`、Page 分页或兄弟 Zone 布局。音频没有固有视觉画面，是否展示封面和
控件由 Renderer 与控制 Layer 决定。

### 本章检查

- 媒体 Placement 与递归 Document Placement 使用相同术语。
- 两者默认值可以不同且已明确：Document 默认 `none`，媒体默认 `contain`。
- 内容变换不会反向改写存储坐标。

## 9. 持久播放策略与 Session 状态

文档可以保存初始意图：`autoplay`、`loop`、`muted`、`volume`、`playbackRate`、片段范围
和 `controls`。以下状态不得默认持久化：

- 当前时间或当前动画帧；
- playing/paused/buffering/seeking；
- 已完成循环次数；
- 用户音量、系统静音和音频焦点；
- 选中的字幕/音轨；
- 控件渐隐计时；
- 解码和传输进度。

这些状态属于 Presentation Session。应用只有在用户显式要求“保存当前位置/默认轨道”
时，才可通过文档编辑生成新的持久字段或资源。

### 本章检查

- 同一文件在不同设备打开不会携带陈旧播放进度。
- 对话控制和按钮控制可以操作同一 Session 状态机。
- 初始作者意图与用户当前操作没有互相覆盖。

## 10. Capability 与 Renderer 路由

标准 Capability 与 Interface ID：

| Content | Capability ID | Interface ID |
| --- | --- | --- |
| `text` | `facetwire.renderer.text` | `facetwire.renderer.text.v1` |
| `image` | `facetwire.renderer.image` | `facetwire.renderer.image.v1` |
| `animated-image` | `facetwire.renderer.animated-image` | `facetwire.renderer.animated-image.v1` |
| `video` | `facetwire.renderer.video` | `facetwire.renderer.video.v1` |
| `audio` | `facetwire.renderer.audio` | `facetwire.renderer.audio.v1` |

声明 Capability 只表示插件可以接受该类请求，不保证支持所有 codec。Renderer 必须在
Validate 阶段根据 Resource `mediaType`、实际流和宿主服务返回明确结果。没有匹配插件、
codec 不支持、资源缺失和解码失败分别映射到 `renderer_missing`、`unsupported_type`、
`resource_missing/resource_unavailable`、`decode_failed` Placeholder 原因。

具体 C ABI 请求结构和函数表将在每个 Renderer 的详细设计中定义；不得把平台播放器、
Flutter Widget、AVPlayer、Media Foundation 对象跨 ABI 返回。

### 本章检查

- Content 类型、Capability 和 Interface 命名一一对应。
- 格式支持是运行时协商，不是由扩展名猜测。
- 标准化文件格式不依赖某个 UI 或播放器框架。

## 11. 可访问性与控制

- `text` 必须产生可阅读文本 Semantics，并保留语言和方向；
- `image`/`animated-image` 使用 `alt`；空 `alt` 表示装饰内容；
- `video`/`audio` 使用非空 `label`；
- Track 的语言、种类、标签和默认状态必须暴露给宿主；
- 可见控制按钮属于独立控制 Layer 或宿主 UI，通过标准 Action Intent 操作 Session；
- Content 对象不得嵌入平台控件实例、键码或 Gesture 对象。

最低控制动作词汇计划包括 `play`、`pause`、`toggle-playback`、`seek-relative`、
`seek-to`、`set-rate`、`set-muted`、`set-volume`、`select-track`、`next`、`previous`。
动作参数、权限和冲突规则将在 Interaction Profile 中规范，不在本文件中提前固化。

### 本章检查

- 内容可访问性不依赖控件是否可见。
- 控制层可以渐隐或完全不存在，但对话控制仍可工作。
- 动作词汇已预留，具体 ABI 没有被过早冻结。

## 12. 安全、隐私与资源限制

宿主必须在解码前设置可配置上限，包括：资源字节数、解压后像素、图像帧数、媒体时长、
文本长度、字体数、Track 数、解码内存、并发 Decoder 和单帧处理时间。Schema 上限只是
输入防线，不能替代 Decoder 沙箱和运行时配额。

元数据不得自动触发网络、脚本、外部进程、设备访问或文件系统越界。媒体内嵌 URL、字体
链接和 Track 外链必须按不可信数据处理。未知或损坏内容只能使当前 Zone 降级，不得中断
兄弟 Zone 或整个 Page。

### 本章检查

- 图片炸弹、超长文本、恶意字体和媒体容器均进入威胁模型。
- Renderer 不因 Content 声明获得网络或文件系统权限。
- 故障隔离单位仍然是 Zone。

## 13. AI 编辑语义

AI 应通过稳定 Zone ID、Resource ID 和原子 Patch 修改内容。新增媒体时，Resource 和所有
引用它的 Content 变更必须在同一事务中提交，或者先以 Placeholder 保留目标 Zone，待
Resource 可用后原位替换。删除 Resource 前必须确认不存在引用。

AI 可以调整文本、样式、Placement、Track、初始播放策略和控制 Layer；不得将当前播放
时间、下载任务 ID、用户音量或设备私有路径误写为内容。AI 若不能确认 codec，应保留
真实 `mediaType`，由 Capability 协商决定可展示性。

### 本章检查

- AI 编辑可以精确定位并保持引用完整性。
- 异步生成过程有 Placeholder 过渡，不需要临时破坏文档结构。
- 设备运行态不会被误保存为标准内容。

## 14. 验证与一致性要求

符合 0.1 的结构验证器必须检查 JSON Schema；语义验证器还必须检查 Resource 引用、
mediaType 相容性、唯一默认 Track、时间片段范围和跨字段组合。符合 0.1 的部分 Renderer
可以只实现一种 Content，但必须保留其他 Zone 几何并安全降级。
ASP 主 Schema 只离线识别五种类型判别符；验证器必须把每个对应 `content` 对象再次交给
`core-content-profile-v0.1.schema.json`，不得把主 Schema 通过当作完整媒体字段验证通过。

最低测试矩阵：每种 Content 的最小合法对象、完整对象、未知字段保留、缺失 Resource、
错误 mediaType、透明 Alpha、各种 fit/alignment、reduce-motion、非法时间范围、Track
冲突、Capability 缺失、解码失败和 Session 状态不持久化。

### 本章检查

- Schema 与跨对象/运行时验证职责已分开。
- 每项规范行为都有直接测试入口。
- 部分实现不会伪装成完整 codec 支持。

## 15. 整体关联与第二阶段边界

本 Profile 不改变 Canvas/Page/Layer/Zone 几何，不改变递归 Document Placement，也不
改变 Placeholder Renderer。它补充 Zone `content` 语义，并复用 Resource、Render Target、
Presentation Session、Capability 路由和未来 Interaction Profile。

第二阶段的图表/脑图/Visio/Excel/CSV 将拆成三层：结构化数据源、图形/图表模型、视图
投影。Excel/CSV 是数据源，不直接等同于图表；Visio/脑图是图结构输入，不直接等同于
静态图片。它们最终仍可以通过 Zone、Placement 和 Placeholder 与本阶段内容组合。

专业图像能力同样作为独立 Profile 演进。AI 图像处理由应用协调层执行，结果登记为新的
Resource 并以原子 Patch 更新目标图层；Renderer 不直接调用模型。合成的源图层、扁平
预览和最终导出是不同 Resource，不能以覆盖原素材的方式混为一体。

文本与图片/图表的自动排版由独立的
[Flow Content Profile 0.1](flow-content-profile-v0.1.zh-CN.md) 和
`facetwire.layout.flow` Capability 负责。Core Text/Image Renderer 只渲染已分配的文本
片段或 Object bounds，不拥有兄弟顺序、绕排或虚拟分页。

### 本章检查

- 第一阶段与 ASP、Runtime、Session、Placeholder 的现有契约无冲突。
- 第二阶段可以复用基础能力而不把专有格式塞入 Core Content。
- 文本和媒体 Renderer 可以在任何 Agent Scene Parser 实现之前独立开发测试。
