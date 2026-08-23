# FacetWire Presentation Session Projection 0.1

## 1. 目的

本规范定义 FacetWire 持久文档与当前设备展示运行态之间的最小投影边界。它允许 AI
应用把远程生成、按需传输和能力协商的结果安全地交给 FacetWire 展示，同时不把
任务调度、网络或通知协议写入文件格式和 Renderer 插件。

本规范受 ADR-0003 约束。

### 本章检查

- 规范对象是展示投影，不是远程任务协议。
- 文档、会话和 Renderer 的所有权边界清晰。

## 2. 数据所有权

| 数据 | 所有者 | 是否默认持久化 |
| --- | --- | ---: |
| Canvas、Page、Layer、Zone、资源引用 | FacetWire Document | 是 |
| 当前设备、窗口、字体、媒介和 Capability | Presentation Session | 否 |
| 资源可用性、进度、陈旧状态 | Presentation Session | 否 |
| AI 任务、服务器地址、通知和凭据 | 应用协调层 | 否，且不得进入 Renderer |
| DisplayList、Semantics、Hit Regions | Renderer Runtime Frame | 否 |

应用可以显式把一份脱敏的展示状态快照写成普通文档内容，但此行为属于文档编辑，
不得与 Session 的瞬时状态自动混合。

### 本章检查

- 每类状态只有一个权威所有者。
- 瞬时任务状态不会意外污染可移植文件。

## 3. Availability Projection

Placeholder Renderer 0.1 使用以下尾部字段表达当前可展示性：

```c
typedef uint32_t fw_placeholder_phase;
#define FW_PLACEHOLDER_PHASE_NONE              0u
#define FW_PLACEHOLDER_PHASE_QUEUED            1u
#define FW_PLACEHOLDER_PHASE_RUNNING           2u
#define FW_PLACEHOLDER_PHASE_WAITING           3u
#define FW_PLACEHOLDER_PHASE_TRANSFERRING       4u
#define FW_PLACEHOLDER_PHASE_READY_FOR_HANDOFF  5u

typedef uint32_t fw_placeholder_progress_kind;
#define FW_PLACEHOLDER_PROGRESS_NONE           0u
#define FW_PLACEHOLDER_PROGRESS_INDETERMINATE  1u
#define FW_PLACEHOLDER_PROGRESS_FRACTION       2u

typedef struct fw_placeholder_progress_v1 {
    uint32_t struct_size;
    fw_placeholder_progress_kind kind;
    uint64_t completed;
    uint64_t total;
} fw_placeholder_progress_v1;
```

`fw_placeholder_request_v1` 在其持久内容与布局字段之后追加：

```c
uint64_t presentation_revision;
fw_placeholder_phase phase;
fw_placeholder_progress_v1 progress;
uint32_t stale;
uint32_t flags;
```

字段规则：

- `presentation_revision` 由 Session 为同一展示绑定单调递增；Renderer 只将其纳入
  缓存身份，不自行比较历史；
- `phase` 只描述通用展示阶段，不标识云厂商、模型或任务队列；
- `FRACTION` 要求 `total > 0` 且 `completed <= total`；
- 进度使用整数分数，避免跨 ABI 浮点累计误差；
- `stale` 只能是 0 或 1；非规范值按 1 归一化并记录标志；
- 未知 phase/progress kind 安全归一化为 NONE。

### 本章检查

- 字段足以显示排队、执行、等待、传输和等待设备选择。
- 没有任务 ID、访问令牌、服务器路径或用户 Prompt。
- 新字段使用结构尾部追加规则，保持 ABI 可扩展。

## 4. reason 与 phase 的正交关系

`reason` 回答“为什么目标内容当前没有由主 Renderer 展示”；`phase` 回答“宿主希望
向用户表达的通用准备阶段”。两者不可合并。

示例：

| reason | phase | 含义 |
| --- | --- | --- |
| `loading` | `queued` | 结果尚未开始准备 |
| `loading` | `running` | 外部系统正在生成或解析 |
| `resource_unavailable` | `transferring` | 结果存在，但当前设备尚未收到 |
| `renderer_missing` | `ready_for_handoff` | 结果已准备，但应在兼容设备展示 |
| `policy_blocked` | `waiting` | 宿主正在等待用户或管理员决策 |

主 Renderer 可以正常显示后，宿主应停止调用 Placeholder Renderer，而不是提交一个
“完成”占位状态。

### 本章检查

- 生命周期扩展不会造成 reason 枚举无限增长。
- “结果完成”最终表现为主 Renderer 原位替换，而不是永久占位。

## 5. AI 亲和要求

AI Agent 修改持久展示时必须通过稳定对象 ID 和原子 Patch；Session 状态更新使用
独立 revision，不修改文档 revision。一次状态更新不得改变 Zone 外部几何、Layer
顺序或 Page 分页结果。

AI 或应用可以请求：

- 改变占位显示模式或样式；
- 改变目标设备的展示 Profile；
- 选择替代表示或目标设备；
- 在 Artifact 可用后触发主 Renderer 替换。

Renderer 返回的 Action Intent 只是建议。宿主负责把 Intent 映射到任务、设备、
资源或权限操作，并再次执行安全检查。

### 本章检查

- AI 编辑和运行态投影使用不同 revision 域。
- Action Intent 不会绕过宿主权限和设备信任策略。

## 6. 安全与隐私

- Availability Projection 不得包含原始 Prompt、模型响应、私有路径或网络凭据；
- `diagnostic_code` 必须是已脱敏的稳定代码，不得是异常堆栈；
- 通知、设备发现、Artifact 下载和签名验证均在 FacetWire Renderer 之外完成；
- Placeholder 插件必须可以在禁用网络和文件系统的进程中运行；
- Session 在调用 Renderer 前负责拒绝过期、乱序或不可信来源的状态。

### 本章检查

- Renderer 不需要任何远程系统权限。
- 展示状态即使被日志记录，也不应泄露任务内容或凭据。

## 7. 验证

0.1 实现至少验证：

1. 未知 phase、progress kind 和 stale 值具有确定归一化结果；
2. 非法整数分数被拒绝；
3. revision 改变缓存键但不改变测量尺寸和 DisplayList 指令形状；
4. 透明背景不会被插件替换成不透明默认色；
5. phase 与 stale 进入 Semantics 的结构字段；
6. 插件测试无需网络、文件系统、GPU、窗口或真实字体。

### 本章检查

- 每条会话投影规则都有直接的合同测试入口。
- 测试可以跨 Windows、Linux、macOS 复用同一份 C 源码。

## 8. 整体一致性检查

- 本规范与 ADR-0003 的职责划分一致。
- Placeholder Renderer 仍然只负责 Zone 降级展示。
- 持久文档保持 AI 可编辑、可移植，不绑定具体任务系统。
- 运行态信息足以支持随身设备与远端计算分层协作。
