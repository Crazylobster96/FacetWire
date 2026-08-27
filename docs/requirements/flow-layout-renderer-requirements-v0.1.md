# FacetWire Flow Layout 0.1 需求规格

状态：**Experimental Draft**
目标 Capability：`facetwire.layout.flow`
目标 Interface：`facetwire.layout.flow.v1`

## 1. 目标与非目标

Flow Layout 把规范化 Flow Item、Page Template、Render Target 和受限测量服务转换为确定的
Virtual Page/Fragment Plan。它负责兄弟关系、文本片段、Replaced Element、绕排和分页，
不负责解析 ASP、解码图片、绘制图表、执行 AI 或创建平台 UI。

### 本章检查

- 布局结果与内容绘制明确分离。
- 0.1 范围覆盖基础图文排版而非完整出版系统。

## 2. 输入与模型需求

| ID | 需求 |
| --- | --- |
| FLW-IN-001 | 输入必须是宿主从 Flow Content Profile 0.1 投影出的扁平、只读结构。 |
| FLW-IN-002 | Flow、Item 和 Segment ID 必须在所属域唯一且为有效 UTF-8。 |
| FLW-IN-003 | 数值必须有限；尺寸、margin、gap 非负；scale/font scale 大于 0。 |
| FLW-IN-004 | Paragraph 至少包含一个 Segment；text Segment 可为空，object Segment 必须解析到唯一 Object Item。 |
| FLW-IN-005 | `placement=inline` 的 Object 在顶层遍历时必须跳过，只能被一个 Object Segment 引用。 |
| FLW-IN-006 | Content Resource、codec 和图表数据不进入布局请求；只传稳定 content ID/kind。 |
| FLW-IN-007 | Template/Profile 选择由 Session/宿主完成，插件接收单一已选择模板。 |
| FLW-IN-008 | 最大 Item、Segment、Page、Fragment、float、递归和迭代数必须显式传入预算。 |

### 本章检查

- Parser、Profile selection 和 Layout 各有单一入口。
- 所有可导致无限处理的集合均有预算。

## 3. 排版关系需求

| ID | 需求 |
| --- | --- |
| FLW-PLC-001 | 缺省 Placement 为 block。 |
| FLW-PLC-002 | block 必须按 source order 占据独立块并应用 margin collapse 的确定性子集。 |
| FLW-PLC-003 | inline 必须作为不可拆分 replacement segment 参与 baseline 与换行。 |
| FLW-PLC-004 | float-start/end 必须按段落方向解析，并产生轴对齐矩形 exclusion。 |
| FLW-PLC-005 | 剩余行宽低于最小文本宽度时，文本必须移动到 float 下方。 |
| FLW-PLC-006 | overlay 不占流空间，必须具有 anchor、offset、z 和阅读顺序声明。 |
| FLW-PLC-007 | 不支持的 Placement 组合必须拒绝或明确降级，不能静默改成绝对坐标。 |

### 本章检查

- 每种关系对流空间、视觉位置和阅读顺序都有定义。
- RTL 和低剩余宽度具有确定规则。

## 4. 文本片段需求

| ID | 需求 |
| --- | --- |
| FLW-TXT-001 | Layout 必须通过 Text Fragment Service 分行，不自行实现字体、BiDi 或字形塑形。 |
| FLW-TXT-002 | Fragment byte range 必须连续、无重叠、位于 UTF-8 标量边界。 |
| FLW-TXT-003 | 组合字形簇、双向 run 和 inline object 不得在不可分边界拆开。 |
| FLW-TXT-004 | Text Service 返回零消费且非结束时必须视为无进展错误，防止无限循环。 |
| FLW-TXT-005 | 测量 fingerprint 在绘制阶段不一致时必须废弃 Layout Plan 并 reflow。 |
| FLW-TXT-006 | 字体 fallback、截断和断字事实必须进入 Fragment flags/诊断。 |

### 本章检查

- 文本完整性与循环终止均可自动验证。
- 测量和绘制使用不同字体结果时不会继续显示错误页面。

## 5. 子内容测量需求

| ID | 需求 |
| --- | --- |
| FLW-OBJ-001 | Image、GIF、Chart、Video 和未知扩展统一通过 Child Measure Service 返回 intrinsic/约束。 |
| FLW-OBJ-002 | Layout 插件不得调用子 Renderer 的 render、decode 或网络操作。 |
| FLW-OBJ-003 | 缺失测量能力时使用文档 fallback size；仍缺失则返回对应 Item 的 Placeholder fragment。 |
| FLW-OBJ-004 | aspect ratio、min/max 和可用区域冲突时按显式 fit/scale policy 确定处理。 |
| FLW-OBJ-005 | 子内容失败不得改变后续 source order；Placeholder 使用已分配 bounds。 |

### 本章检查

- 内容类型可扩展而无需修改布局插件。
- 降级仍保留正确占位关系。

## 6. Virtual Page 与 Fragment 需求

| ID | 需求 |
| --- | --- |
| FLW-PAG-001 | continuous、virtual-pages 和 columns 三种模式必须输出同一 Fragment 模型。 |
| FLW-PAG-002 | Virtual Page ID 由 flow ID、layout revision/profile hash 和 ordinal 派生，只在当前 Plan 稳定。 |
| FLW-PAG-003 | Fragment 必须包含 source item、page/column、bounds、clip、z、range 和 continuation。 |
| FLW-PAG-004 | Fragment bounds 使用 Virtual Page 逻辑坐标，不包含 viewer zoom。 |
| FLW-PAG-005 | 文本可跨页；不可拆 Object 默认整体移动到下一页。 |
| FLW-PAG-006 | 不得生成没有 Fragment 且没有显式 break 原因的无限空白页。 |
| FLW-PAG-007 | 达到 page/fragment/iteration budget 返回 `RESOURCE_LIMIT` 和已完成前缀统计，不输出可冒充完整结果的 Plan。 |

### 本章检查

- 三种展示模式共享可测试的派生结构。
- 页/片段 ID 和坐标不会被误作持久对象。

## 7. 分页控制需求

| ID | 需求 |
| --- | --- |
| FLW-BRK-001 | 支持 breakBefore/After、keepTogether、keepWithNext、orphans、widows。 |
| FLW-BRK-002 | 冲突按 Profile 规定的固定优先级降级并输出 diagnostic flags。 |
| FLW-BRK-003 | keepWithNext 链必须受最大回溯长度限制。 |
| FLW-BRK-004 | 超大 Object 必须缩小、overflow 或 Placeholder，不得重复推到新空页。 |
| FLW-BRK-005 | 显式 page break 在 continuous 模式转换为分段标记，不强制产生屏幕空白纸页。 |

### 本章检查

- 分页约束能够终止且跨媒介语义明确。
- 无法满足的美观规则不会破坏安全边界。

## 8. 响应式与 Session 需求

| ID | 需求 |
| --- | --- |
| FLW-SES-001 | viewport、font scale、medium、template、capabilities 或字体解析改变必须使 Plan 失效。 |
| FLW-SES-002 | viewer zoom 不触发 reflow，也不得修改 Fragment bounds。 |
| FLW-SES-003 | 当前阅读位置以 source item + logical offset 保存，不以 Virtual Page ordinal 保存。 |
| FLW-SES-004 | Layout Plan、当前页、scroll 和 selection 属于 Session，不默认持久化。 |
| FLW-SES-005 | 同一 document/layout revision 的结果可缓存；插件本身不保存 Session 状态。 |

### 本章检查

- reflow 与 zoom 分离。
- 跨设备恢复位置不会依赖易变页码。

## 9. 输出、语义与渲染协调需求

| ID | 需求 |
| --- | --- |
| FLW-OUT-001 | 变长 Plan 必须通过同步 Sink 回调输出；Sink 必须复制所需数据。 |
| FLW-OUT-002 | begin/end page、fragment 顺序和 z 必须确定，失败时立即停止并平衡已开始的 page。 |
| FLW-OUT-003 | 阅读顺序使用 source/Segment 顺序；overlay z 不自动改变 Semantics 顺序。 |
| FLW-OUT-004 | 宿主按 Fragment content kind 路由 Text/Image/Chart/Placeholder Renderer。 |
| FLW-OUT-005 | Text Fragment 绘制必须验证 layout fingerprint；对象绘制使用 exact bounds。 |
| FLW-OUT-006 | Layout 插件不生成 DisplayList；它生成 Layout Plan。 |

### 本章检查

- 布局输出、内容渲染和无障碍顺序解耦。
- C ABI 没有跨边界分配变长数组。

## 10. 错误、线程与安全需求

| ID | 需求 |
| --- | --- |
| FLW-NFR-001 | API 可重入；同一 plugin handle 可并发处理独立请求。 |
| FLW-NFR-002 | 插件无网络、文件系统、GPU、窗口、codec 和平台 UI 权限。 |
| FLW-NFR-003 | 所有服务回调同步且只在调用线程有效。 |
| FLW-NFR-004 | Sink/service 返回错误时保留首个错误并停止产生新 Fragment。 |
| FLW-NFR-005 | 递归、浮动回溯、分页迭代和总调用时间必须受预算约束。 |
| FLW-NFR-006 | 相同规范化输入、服务结果和实现版本产生相同结构 Plan 与 128-bit key。 |

### 本章检查

- 插件合同、线程模型、确定性和拒绝服务防护完整。
- 失败不会泄漏平台或文档私有数据。

## 11. 验收测试矩阵

| 类别 | 最低用例 |
| --- | --- |
| 基础流 | 空 Flow、单段、多段、block 图片、连续模式 |
| inline | 行首/中/尾、baseline 四种、过宽 inline object、RTL |
| float | start/end、多个 float、剩余宽度不足、跨页结束 |
| overlay | anchor、offset、z、decorative/阅读顺序 |
| 分页 | 跨页段落、显式 break、keep、widow/orphan、超大 Object、空页防护 |
| 响应式 | compact/regular/print、font scale、zoom 不 reflow、source anchor 恢复 |
| 降级 | child missing、text service missing、fallback size、Placeholder bounds |
| 失败 | Sink 第 N 次失败、零消费、非法 UTF-8、预算耗尽、循环引用 |
| 确定性 | 指针无关 key、重复 Plan、Windows/macOS/Linux 结构比较 |

### 本章检查

- 每组核心需求均有正向、边界和失败测试。
- Fake 服务足以覆盖算法，真实文本/图片后端另做集成测试。

## 12. 完成定义与关联检查

0.1 完成需要：Flow Profile、公共 ABI、参考 Layout 插件、Fake Text Fragment/Child Measure
Service、Plan Sink、单元测试、Manifest、参数 Schema、三种 fixture、Playground 页面检查器
和跨平台结构快照。

### 本章检查

- 与 ASP、Core Content、Text Renderer、Placeholder、Session 和 Runtime 均有接口闭环。
- Text Renderer v1 不承担兄弟布局；后续 Text Fragment Service 可独立版本化。
- Chart/Visio/脑图只要实现 Child Measure/Renderer 合同即可加入 Flow。
