# FacetWire 架构

[English](architecture.md) | **简体中文**

## 1. 范围

FacetWire Core 是智能体场景包（Agent Scene Package，ASP）Renderer 下方的插件边界。Core ABI 不依赖完整的场景文档、排版引擎或图形 API。仓库另外定义实验性的 ASP 目录规范，让宿主和 Renderer 能共享确定性的测试夹具，同时避免把该文件格式耦合到 ABI。Core 负责定义宿主如何发现能力、协商兼容协议、提供受约束的服务，以及在不同平台上管理插件生命周期。

### 本章检查

- 项目边界将插件机制与场景文件格式分离。
- 渲染、解析、交互和传输可以独立演进。
- 公共协议不要求任何平台专属对象。

## 2. 分层模型

```text
应用 / AI 智能体
        |
ASP 场景与布局模型
        |
FacetWire 能力路由
        |
+----------------+------------+--------------+
| Renderer 插件  | Parser 插件 | Interaction |
| 文本/图片/视频 |            | 插件         |
+----------------+------------+--------------+
        |
宿主服务与标准化 DisplayList（规划中）
        |
Skia / Metal / Direct2D / Vulkan / WebGPU / 平台 UI
```

能力（capability）的粒度小于插件。一个插件可以公开多项能力，宿主按照稳定标识符和策略选择能力。

### 本章检查

- 插件是部署单元，能力是选择单元。
- 图形后端位于标准化宿主服务边界之下。
- 架构同时支持展示能力和非展示能力。

## 3. 部署模式

所有模式使用相同的逻辑描述符和生命周期：

| 模式 | 连接方式 | 典型平台 |
| --- | --- | --- |
| 静态注册 | 应用注册查询函数 | iOS、嵌入式及所有平台 |
| 原生动态加载 | DLL/SO/dylib 导出的查询符号 | 桌面平台、受控 Android |
| 进程隔离 | 围绕协议的序列化适配器 | 不可信 Parser |
| WebAssembly | 生成的适配器与受限导入 | 可移植沙箱 |
| 远程调用 | 带版本的 RPC 适配器 | 服务端和重型 Renderer |

0.1 已实现静态注册、受支持平台上的显式原生动态加载、确定性能力选择和版本化接口查询。进程、Wasm 和远程传输仍是后续适配器，但必须保持相同的能力语义。

### 本章检查

- 受限平台不依赖下载可执行代码。
- 平台政策允许时仍可使用动态加载。
- 隔离只改变传输和信任模型，不改变插件的概念协议。

## 4. ABI 不变量

1. 边界使用 C ABI，即使任一侧使用其他语言实现。
2. 每个可扩展函数表和描述符都以 `struct_size` 开头。
3. ABI 主版本必须一致；宿主可以接受较旧的次版本。
4. 字符串使用 UTF-8 `(pointer, length)` 视图。
5. 所有权保留在分配方。返回的函数表和描述符是插件所有的不可变数据，在插件卸载前有效。
6. 除非接口明确规定异步行为，否则调用均为同步调用。
7. 插件只能使用明确提供的宿主服务；不得自行推断文件系统、网络、图形或 UI 权限。

### 本章检查

- 这些规则避免 C++ ABI、分配器、异常和标准库耦合。
- 通过结构大小进行能力检测，可以支持追加式演进。
- 基于能力的宿主服务支持沙箱和远程实现。

## 5. 规划中的能力族

- `facetwire.renderer.*`：文本、图片、动画、视频、图表和控制组件。
- `facetwire.parser.*`：ASP 以及可选的第三方文档格式。
- `facetwire.layout.*`：流式排版、分页、锚定和响应式变体。
- `facetwire.interaction.*`：输入、滚动、拖动和命令。
- `facetwire.transport.*`：动态加载、IPC、Wasm 和远程适配器。
- `facetwire.export.*`：扁平化、光栅化和文档导出。

每个能力族在单独的头文件中定义版本化函数表。插件通过 `query_interface` 发现可选宿主服务并公开可选接口。

### 本章检查

- 能力族覆盖已经确定的媒体和静态文档用例。
- 新增能力族不需要修改基础生命周期 ABI。
- 不支持的内容可以通过 Placeholder 元数据保留布局位置。

## 6. 安全边界

进程内原生插件拥有宿主进程的有效权限，ABI 本身不提供沙箱。不可信和专有 Parser 应支持进程隔离模式，并限制输入、内存、CPU、递归深度和输出大小。Manifest 声明权限、依赖和许可证元数据，但宿主始终是策略决策方。Core 只接受明确提供的绝对动态库路径，不执行插件发现、签名验证或信任决策。

### 本章检查

- 能力声明不等同于操作系统隔离。
- Parser 威胁模型覆盖恶意文档和资源耗尽。
- 许可证元数据属于分发政策，不属于运行时授权。

## 7. 演进顺序

1. **已实现：** 生命周期、发现、诊断和静态注册。
2. **已实现：** Manifest Schema、精确路径原生动态加载和确定性能力路由。
3. **已实现：** DisplayList、共享 Semantics、宿主服务协议和 Placeholder 参考能力。
4. **已实现：** 文本、图片、动态图片、视频和音频核心内容规范，以及 Placeholder、Text、Core Image、Core Media 四个参考插件。
5. **部分实现：** Flow Layout 0.1 已落地公共 ABI、Manifest、确定性 Layout Plan，以及 continuous、virtual-pages、columns 三种模式下的 block/inline/float 排版；已支持文本/行内对象跨区域续排、不可拆 replacement object、四种 baseline、RTL 与逻辑浮动定位、矩形排除区、最小正文宽度清除和对象整体移至下一栏或下一页。下一门禁是 overlay 和分页控制。
6. 定义并实现 Subtitle/Cue Renderer 与 Media Controls/Interaction Profile。
7. 定义结构化数据源层，先支持 CSV，再通过独立适配器支持 Excel。
8. 在结构化数据源之上定义 Chart Model 与 Chart Renderer。
9. 定义通用 Graph Model，并为脑图、Visio、亿图等格式提供导入适配器。
10. 在数据、图表和图结构能力稳定后实现 Image Composition 专业图像合成 Profile。
11. 把已验证的 Spike 收敛为正式 FacetWire Playground，并增加一致性认证。
12. 再增加 ASP/第三方 Parser、进程隔离、Wasm 和语言绑定。

此顺序刻意将结构化数据源、图表和图结构提前到专业图像合成之前；Excel/CSV 是数据源，图表是视图模型，Visio/脑图是图结构输入，三者不会被错误合并成单一 Renderer。

### 本章检查

- 每个阶段都生成可使用、可测试的增量。
- 高风险加载与渲染功能建立在稳定基础之上。
- 跨平台一致性由协议和测试夹具验证，而不是只依赖相同名称。

## 8. 参考宿主与 UI 技术边界

FacetWire Core 不指定应用 UI 框架。当前 Playground 参考宿主在 [ADR-0001](adr/0001-cross-platform-ui-framework.md) 中采用有条件的实现决策：Flutter/Dart 是首选候选，但必须通过五平台 UI Spike；Qt Quick 是第一备用方案。

无论 Playground 使用何种框架，以下边界保持不变：

```text
UI 框架展示层
        |
类型化应用端口
        |
Playground C Bridge
        |
FacetWire Runtime / C ABI 插件
        |
版本化 DisplayList + Semantics + HitRegion
```

插件不得返回 UI 控件、框架对象、平台 View 或指向 UI 线程的回调。替换 Playground UI 时可以替换展示、绑定和播放器适配器，但不得因此修改插件 ABI 或场景文件格式。

### 本章检查

- 参考应用的技术决策有明确链接，但不会被提升为 Core 标准。
- Flutter、Qt、Avalonia 和非可视化宿主都能使用同一套 C ABI 协议。
- UI Spike 失败时，替换范围受控，Core 一致性测试保持不变。
