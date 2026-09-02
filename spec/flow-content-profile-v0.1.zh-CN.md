# FacetWire Flow Content Profile 0.1

状态：**Experimental Draft**

本 Profile 定义文本、图片、图表及其他可测量内容在虚拟文档中的顺序、嵌入、绕排和分页
语义。它扩展 Zone `content`，但不替代 ASP 的 Canvas/Page/Layer/Zone 层级。

## 1. 范围

Flow 0.1 标准化：顺序块、统一样式段落、行内替换元素、块级/浮动/叠加关系、矩形绕排、
页模板、派生 Virtual Page、Fragment、基础分页控制和响应式重排。

不在 0.1：任意多边形绕排、复杂脚注/尾注、目录生成、跨栏表格、数学公式、专业断字、
Ruby、竖排文字、完整 CSS、Word 兼容布局和像素级 PDF 复刻。

### 本章检查

- 能支持基础图文混排和跨页文本，同时保持可实现范围。
- 未把完整出版系统伪装成首版能力。

## 2. Flow Content

Flow 是一个 Zone 内容对象：

```json
{
  "type": "flow",
  "id": "article.main",
  "pageTemplate": {
    "mode": "virtual-pages",
    "size": {"width": 794, "height": 1123},
    "margins": {"top": 72, "right": 64, "bottom": 72, "left": 64},
    "gap": 24
  },
  "items": [
    {
      "id": "paragraph.intro",
      "kind": "paragraph",
      "segments": [{"type": "text", "text": "FacetWire 支持图文流式排版。"}]
    },
    {
      "id": "image.campus",
      "kind": "object",
      "content": {"type": "image", "resource": "image.campus", "alt": "校园航拍图"},
      "placement": {"mode": "block", "align": "center"}
    }
  ]
}
```

`id` 在所属 Flow 内唯一且稳定。`items` 同时是定义集合和默认顶层顺序；`placement.mode=inline` 的 Object 在顶层遍历时必须跳过，只能在 Paragraph Segment 的引用位置参与阅读和布局。未知 Item kind 必须
保留顺序和 fallback size，并路由 Placeholder。

### 本章检查

- Flow 仍然是 Zone 内容，外部 Zone bounds 由上级布局决定。
- Item ID 与顺序足以支持 AI 原子编辑和来源追踪。

## 3. Paragraph 与 Segment

Paragraph 由有序 `segments` 组成。0.1 Segment 类型：

- `text`：UTF-8 纯文本，继承 Paragraph 的统一 Text Style；
- `object`：引用同一 Flow 内 `kind=object` 的 Item，作为行内替换元素。

```json
{
  "id": "paragraph.inline-demo",
  "kind": "paragraph",
  "style": {"fontSize": 18, "lineHeightMultiplier": 1.4},
  "segments": [
    {"type": "text", "text": "温度趋势 "},
    {"type": "object", "item": "chart.sparkline", "baseline": "middle"},
    {"type": "text", "text": " 已趋于稳定。"}
  ]
}
```

行内 Object Item 必须显式声明 `placement.mode=inline`，在顶层 Item 遍历时跳过，并且只能由一个 Object Segment 引用；它通过该 Segment 的位置确定阅读顺序。`baseline` 为 `baseline`、`middle`、`text-top`、`text-bottom`。文本换行语义继承
Core Text；Paragraph 可以跨页，但单个 Unicode 标量、组合字形簇和 inline object 不得拆分。

### 本章检查

- 图片嵌入段落不需要向 Core `text` 塞入平台对象或 U+FFFC 私有约定。
- 统一样式保持 0.1 简单性，未来 Rich Text 可新增 Segment 类型。

## 4. Object 与 Replaced Element

Object 的 `content` 可以是已注册 Content Profile，例如 `image`、`animated-image`、未来
`chart` 或递归 `document`。布局器只读取标准测量合同：

- intrinsic width/height 和 aspect ratio；
- min/max width/height；
- 可用 fallback size；
- 是否允许缩小、放大、跨页或切片；
- Placement 与 margins。

Object 资源缺失、Renderer 缺失或解码失败不改变已分配 bounds。Placeholder 使用相同
bounds。动态图片和视频参与静态分页时使用其视觉 viewport，播放状态仍归 Session。

### 本章检查

- 图片和图表具有相同布局接口但保留独立渲染语义。
- 内容失败不会触发重新折叠或破坏后续分页。

## 5. Placement 模式

| mode | 占用流空间 | 文本关系 | 默认 |
| --- | --- | --- | --- |
| `block` | 是，独立块 | 文本位于前后 | 是 |
| `inline` | 是，行内 | 参与 baseline 和换行 | 否 |
| `float-start` | 是，矩形排除区 | 后续文本从逻辑 start 侧绕排 | 否 |
| `float-end` | 是，矩形排除区 | 后续文本从逻辑 end 侧绕排 | 否 |
| `overlay` | 否 | 不改变文本流 | 否，必须显式 |

`start/end` 随段落方向解析。`float` 0.1 只允许轴对齐矩形 exclusion；当剩余行宽小于最小
文本宽度时，文本移到排除区下方。`overlay` 必须声明 anchor Item、offset、z 和可访问
阅读顺序，不能仅依赖视觉坐标。

参考实现 0.1 将 overlay 锚定到同一请求中更早的非 inline、非 overlay Item 的首个
Fragment；overlay 继承 anchor 的页面、栏和 clip，不推进 Flow cursor。跨已关闭历史页的
反向锚定属于当前未实现组合，必须明确返回 `UNSUPPORTED`。

### 本章检查

- 默认 block 最确定，复杂关系需要显式选择。
- RTL、视觉 z-order 与阅读顺序分别有定义。

## 6. Page Template 与展示模式

`pageTemplate.mode`：

- `continuous`：单一连续流区域，可由宿主滚动；
- `virtual-pages`：按模板生成离散 Virtual Page；
- `columns`：连续或分页区域内生成 1–N 栏。

模板包含 size、margins、column count/gap、page gap、header/footer reserve 和背景语义。
所有单位为 Canvas 逻辑单位。屏幕缺省不得把整页 fit 到窗口；宿主应根据 Responsive
Profile 选择更合适的模板并重新排版。打印/导出可选择固定纸张模板。

### 本章检查

- 重新排版与视口 zoom 是两个独立操作。
- 手机、平板、桌面和打印可以选择不同模板而共享源 Item。

## 7. Authored Page、Virtual Page 与 Fragment

ASP `canvas.pages[]` 是 Authored Page。Flow 生成的 Virtual Page 是当前布局投影：

```text
Authored Page / Flow Zone
  └─ Layout Plan
      ├─ Virtual Page 0
      │   ├─ Text Fragment (paragraph A bytes 0..420)
      │   └─ Object Fragment (image B)
      └─ Virtual Page 1
          └─ Text Fragment (paragraph A bytes 420..end)
```

Fragment 必须记录 source Item ID、类型、Virtual Page index、bounds、clip、z、continuation
flags；Text Fragment 还记录 UTF-8 byte range，并保证端点是合法标量边界。Fragment ID 为
派生 ID，只在相同 document/layout revision 中稳定，不得写入持久 AI Patch。

### 本章检查

- 一个文本 Item 跨页时仍只有一个持久源对象。
- 派生页面和片段不会与 ASP 稳定 Page/Zone ID 混淆。

## 8. 分页控制

Item 可以声明：`breakBefore`、`breakAfter`、`keepTogether`、`keepWithNext`、`orphans`、
`widows`。缺省 orphans/widows 为 2。约束不能同时满足时按以下优先级降级并输出诊断：

1. 不拆分原子 inline object/字形簇；
2. 不超出页面安全边界；
3. keepTogether；
4. keepWithNext；
5. widow/orphan；
6. 视觉平衡。

实现无法精确满足第 3 至第 5 项时必须在 Layout Result 中设置对应 relaxation diagnostic。
包含 inline object 的 Paragraph 始终优先保持对象原子性；若服务不支持安全的逐行回滚，
widow/orphan 可以放宽，但不得静默。

大于完整内容区域且允许缩小的 Object 可缩至 min size；仍放不下时占用独立 overflow
Fragment 或 Placeholder，不能无限创建空白页。

### 本章检查

- 冲突规则确定且可终止。
- 大对象和不可满足约束不会导致无限分页。

## 9. 响应式 Profile

文档可以为 `screen-compact`、`screen-regular`、`screen-wide`、`print`、`export` 提供模板
变体。选择依据是 Render Target、可用逻辑尺寸、输入方式和用户字体缩放，不得使用设备
品牌或私有路径。未匹配时使用 base template。

Profile 改变会生成新的 Layout Plan；源 Item ID 不变。当前页应由宿主按 source anchor
恢复，而不是沿用旧 Virtual Page ordinal。

### 本章检查

- 跨设备差异通过显式 Profile 和 reflow 表达。
- Session 恢复依据源内容锚点，避免页码变化导致跳错位置。

## 10. Capability 与降级

| 能力 | ID |
| --- | --- |
| Flow Layout | `facetwire.layout.flow` |
| Flow Layout Interface | `facetwire.layout.flow.v1` |
| Text Fragment Service | `facetwire.service.text-fragment.v1` |
| Child Measure Service | `facetwire.service.child-measure.v1` |

Flow 插件缺失时，宿主保留 Flow Zone bounds 并显示 Placeholder。子内容 Renderer 缺失时，
只替换对应 Object Fragment。Text Fragment Service 缺失时不得猜测换行；整个 Flow Zone
降级或使用明确保存的 flattened preview。

### 本章检查

- 布局能力、文本服务和内容 Renderer 可以独立协商。
- 局部失败与整体无法布局具有不同降级粒度。

## 11. Session、交互与可访问性

Layout Plan、当前 source anchor、Virtual Page、连续滚动 offset、选择范围和折叠状态属于
Presentation Session。文档只保存模板、Item、样式和初始策略。

Semantics 顺序默认使用 Flow 阅读顺序，而不是 z-order。跨页 Text Fragment 在平台桥接层
合并为同一源 Paragraph 语义；inline object 在其 Segment 位置进入阅读顺序；overlay 必须
声明独立阅读位置或 decorative。

### 本章检查

- 运行态页码和滚动位置不会污染可移植文件。
- 视觉合成顺序不会错误决定读屏顺序。

## 12. 安全与资源限制

宿主必须限制 Flow 深度、Item/Segment/Page/Fragment 数、单段文本字节数、浮动对象数、
回溯次数、分页迭代数和总布局时间。布局器不得读取资源文件、调用网络或执行子内容。
递归 Flow/Document 必须检测 ID/路径循环并在对应 Item 降级。

### 本章检查

- 恶意分页约束、超长文本、浮动对象和递归内容均进入威胁模型。
- Layout 插件只获得规范化数据和受限测量服务。

## 13. AI 编辑与一致性

AI 修改以稳定 Flow/Item ID 定位。插入图片必须在同一事务中新增 Resource、Object Item 和
顺序引用；移动图片只修改 Placement/顺序，不复制 Resource。修改前文可能改变全部后续
Virtual Page，因此 AI 不应 Patch Fragment bounds 或 Virtual Page ordinal。

符合 0.1 的结构验证器必须应用 `spec/schema/flow-content-profile-v0.1.schema.json`；语义验证器还必须检查 ID 唯一性、Segment 引用、inline object 单一所有权、Placement 组合、有限尺寸、分页控制、循环、Resource 引用，以及每个 Object `content` 对应的注册 Content Profile。未知扩展必须保留。

### 本章检查

- AI 操作源语义对象，不操作易失的排版结果。
- 资源、对象和顺序变更具有可原子提交的关系。

## 14. 整体关联与扩展性检查

- Flow 作为 Zone content，不改变 Canvas/Page/Layer/Zone 外部模型。
- Virtual Page 是 Session/Layout Plan 投影，与 Authored Page 无冲突。
- Text Renderer、Image Renderer、Chart Renderer 接收布局结果而不拥有兄弟关系。
- 图片/GIF/图表通过 Replaced Element 测量合同统一参与排版。
- Rich Text、复杂形状绕排、脚注、表格和专业分页可通过新增 Segment、Item 和 Capability
  版本扩展，不需要破坏 Core Content v1。

### 本章检查

- 持久内容、派生布局、当前交互和最终绘制的所有者完整闭合。
- 标准能够先支持基础图文排版，再迭代到专业出版能力。
